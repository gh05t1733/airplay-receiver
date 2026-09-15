// src/core/rtsp/rtsp_server.cpp — RTSP/1.0 TCP server implementation.
// Listens on a TCP port, accepts connections, parses RTSP requests, and routes them.
// Handles GET /info (returns bplist), documents errors for unimplemented routes.

#include "rtsp_server.h"
#include "bplist.h"
#include <spdlog/spdlog.h>

// Winsock2 must come before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace arak::rtsp {

// Maximum request body size (1 MB).
static constexpr size_t kMaxBodySize = 1 * 1024 * 1024;

// Parse an RTSP request from a raw buffer.
// Returns true if the request was successfully parsed.
static bool parseRtspRequest(const std::string& raw, RtspRequest& req) {
    // Find end of headers (double CRLF)
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    std::string headers = raw.substr(0, headerEnd);
    size_t bodyStart = headerEnd + 4;

    // Parse request line
    size_t lineEnd = headers.find("\r\n");
    if (lineEnd == std::string::npos) return false;
    std::string requestLine = headers.substr(0, lineEnd);

    // "METHOD SP URI SP RTSP/1.0"
    size_t sp1 = requestLine.find(' ');
    if (sp1 == std::string::npos) return false;
    size_t sp2 = requestLine.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return false;

    req.method = requestLine.substr(0, sp1);
    req.uri = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);

    // Parse headers (case-insensitive name match)
    size_t pos = lineEnd + 2;
    while (pos < headers.size()) {
        size_t nextLine = headers.find("\r\n", pos);
        if (nextLine == std::string::npos) nextLine = headers.size();

        std::string line = headers.substr(pos, nextLine - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading space from value
            if (!value.empty() && value[0] == ' ') value = value.substr(1);

            // Store with lowercase name for case-insensitive lookup
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            req.headers[lowerName] = value;

            if (lowerName == "cseq") {
                req.cseq = std::stoi(value);
            } else if (lowerName == "content-type") {
                req.contentType = value;
            }
        }
        pos = nextLine + 2;
    }

    // Store body (will be read separately by the connection handler)
    if (bodyStart < raw.size()) {
        req.body.assign(raw.begin() + bodyStart, raw.end());
    }

    return !req.method.empty() && !req.uri.empty();
}

// Build an RTSP response string.
static std::string buildRtspResponse(const RtspResponse& resp) {
    std::string out = "RTSP/1.0 " + std::to_string(resp.statusCode) + " " + resp.reason + "\r\n";
    out += "CSeq: " + std::to_string(resp.cseq) + "\r\n";
    if (!resp.contentType.empty()) {
        out += "Content-Type: " + resp.contentType + "\r\n";
    }
    if (!resp.body.empty()) {
        out += "Content-Length: " + std::to_string(resp.body.size()) + "\r\n";
    }
    out += "Connection: close\r\n";
    out += "\r\n";
    if (!resp.body.empty()) {
        out.append(resp.body.begin(), resp.body.end());
    }
    return out;
}

struct RtspServer::Impl {
    SOCKET listenSock = INVALID_SOCKET;
    std::atomic<bool> running{false};
    std::thread acceptThread;
    std::vector<std::thread> sessionThreads;
    std::mutex sessionMutex;
    RtspConfig config;
    identity::ReceiverIdentity identity;
    uint16_t boundPort_ = 0;
    RequestHandler customHandler;

    // Default handler: routes requests based on method + URI.
    RtspResponse handleRequest(const RtspRequest& req) {
        // Use custom handler if set
        if (customHandler) return customHandler(req);

        RtspResponse resp;
        resp.cseq = req.cseq;

        // GET /info — the one Sprint 1 route that returns real data
        if (req.method == "GET" && req.uri == "/info") {
            bplist::BplistDict info;
            info["name"] = bplist::BplistValue(identity.name);
            info["deviceID"] = bplist::BplistValue(identity.deviceId);
            info["macAddress"] = bplist::BplistValue(identity.deviceId);
            info["model"] = bplist::BplistValue(identity.model);
            info["sourceVersion"] = bplist::BplistValue(identity.sourceVersion);
            info["features"] = bplist::BplistValue(identity::encodeFeatures(identity.features));
            info["statusFlags"] = bplist::BplistValue(static_cast<int64_t>(identity.flags));
            info["vv"] = bplist::BplistValue(int64_t{1});  // protocol version

            bplist::BplistDict root;
            root["txtAirPlay"] = bplist::BplistValue(std::move(info));

            std::vector<uint8_t> plistData;
            auto status = bplist::encode(bplist::BplistValue(std::move(root)), plistData);
            if (status != CoreStatus::Ok) {
                resp.statusCode = 500;
                resp.reason = "Internal Server Error";
                return resp;
            }

            resp.statusCode = 200;
            resp.reason = "OK";
            resp.contentType = "application/x-apple-binary-plist";
            resp.body = std::move(plistData);
            spdlog::info("RTSP: GET /info -> 200 OK ({} bytes)", resp.body.size());
            return resp;
        }

        // POST /pair-setup, /pair-verify, /fp-setup — documented errors (Sprint 1)
        if (req.method == "POST" &&
            (req.uri == "/pair-setup" || req.uri == "/pair-verify" || req.uri == "/fp-setup")) {
            resp.statusCode = 501;
            resp.reason = "Not Implemented";
            spdlog::info("RTSP: POST {} -> 501 Not Implemented (Sprint 1)", req.uri);
            return resp;
        }

        // Unknown method or URI — documented error
        resp.statusCode = 405;
        resp.reason = "Method Not Allowed";
        spdlog::info("RTSP: {} {} -> 405 Method Not Allowed", req.method, req.uri);
        return resp;
    }

    void handleSession(SOCKET clientSock, sockaddr_in clientAddr) {
        char addrBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrBuf, sizeof(addrBuf));
        std::string peerAddr = addrBuf;

        spdlog::info("RTSP: new connection from {}:{}", peerAddr, ntohs(clientAddr.sin_port));

        // Set a receive timeout (15 seconds, matching keepAliveTimeoutMs)
        int timeout = config.keepAliveTimeoutMs;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        // Read request (simplified: read all data available in one recv)
        std::string rawData;
        char buf[4096];
        while (true) {
            int n = recv(clientSock, buf, sizeof(buf), 0);
            if (n <= 0) break;
            rawData.append(buf, n);
            if (rawData.size() > kMaxBodySize) {
                spdlog::warn("RTSP: request body exceeds {} bytes from {}", kMaxBodySize, peerAddr);
                break;
            }
            // Check if we have a complete request
            if (rawData.find("\r\n\r\n") != std::string::npos) {
                // Check Content-Length to know if body is complete
                size_t headerEnd = rawData.find("\r\n\r\n") + 4;
                size_t contentLen = 0;
                size_t clPos = rawData.find("Content-Length:");
                if (clPos != std::string::npos && clPos < headerEnd) {
                    contentLen = std::stoul(rawData.substr(clPos + 15));
                }
                if (rawData.size() >= headerEnd + contentLen) break;
            }
        }

        if (rawData.empty()) {
            closesocket(clientSock);
            return;
        }

        // Parse and handle
        RtspRequest req;
        req.peerAddress = peerAddr;
        if (!parseRtspRequest(rawData, req)) {
            spdlog::warn("RTSP: malformed request from {}", peerAddr);
            RtspResponse errResp;
            errResp.cseq = 0;
            errResp.statusCode = 400;
            errResp.reason = "Bad Request";
            std::string respStr = buildRtspResponse(errResp);
            send(clientSock, respStr.c_str(), static_cast<int>(respStr.size()), 0);
            closesocket(clientSock);
            return;
        }

        RtspResponse resp = handleRequest(req);
        std::string respStr = buildRtspResponse(resp);
        send(clientSock, respStr.c_str(), static_cast<int>(respStr.size()), 0);

        closesocket(clientSock);
        spdlog::info("RTSP: closed connection to {}:{}", peerAddr, ntohs(clientAddr.sin_port));
    }

    void acceptLoop() {
        while (running) {
            // Use select() with a timeout so we can check the running flag
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listenSock, &readSet);

            timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int sel = select(0, &readSet, nullptr, nullptr, &tv);
            if (sel <= 0) continue;

            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock = accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
            if (clientSock == INVALID_SOCKET) continue;

            // Spawn a thread to handle this session (maxSessions enforced simply)
            {
                std::lock_guard lock(sessionMutex);
                // Clean up finished threads
                sessionThreads.erase(
                    std::remove_if(sessionThreads.begin(), sessionThreads.end(),
                        [](std::thread& t) { return !t.joinable(); }),
                    sessionThreads.end());

                if (static_cast<int>(sessionThreads.size()) >= config.maxSessions) {
                    spdlog::warn("RTSP: max sessions ({}) reached, rejecting connection", config.maxSessions);
                    closesocket(clientSock);
                    continue;
                }
            }

            sessionThreads.emplace_back(&Impl::handleSession, this, clientSock, clientAddr);
        }
    }
};

RtspServer::RtspServer() : impl_(std::make_unique<Impl>()) {}
RtspServer::~RtspServer() { stop(); }

CoreStatus RtspServer::start(const RtspConfig& config, const identity::ReceiverIdentity& identity) {
    impl_->config = config;
    impl_->identity = identity;

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("RTSP: WSAStartup failed");
        return CoreStatus::IoError;
    }

    // Create listening socket
    impl_->listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->listenSock == INVALID_SOCKET) {
        spdlog::error("RTSP: socket() failed: {}", WSAGetLastError());
        return CoreStatus::IoError;
    }

    // Allow address reuse
    int optval = 1;
    setsockopt(impl_->listenSock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config.port);

    if (bind(impl_->listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        spdlog::error("RTSP: bind failed on port {}: WSA error {}", config.port, err);
        closesocket(impl_->listenSock);
        impl_->listenSock = INVALID_SOCKET;
        return CoreStatus::BindFailed;
    }

    // Get the actual bound port
    sockaddr_in boundAddr{};
    int boundLen = sizeof(boundAddr);
    getsockname(impl_->listenSock, reinterpret_cast<sockaddr*>(&boundAddr), &boundLen);
    impl_->boundPort_ = ntohs(boundAddr.sin_port);

    // Listen
    if (listen(impl_->listenSock, SOMAXCONN) == SOCKET_ERROR) {
        spdlog::error("RTSP: listen() failed: {}", WSAGetLastError());
        closesocket(impl_->listenSock);
        impl_->listenSock = INVALID_SOCKET;
        return CoreStatus::IoError;
    }

    impl_->running = true;
    impl_->acceptThread = std::thread(&Impl::acceptLoop, impl_.get());

    spdlog::info("RTSP: listening on port {} (max {} sessions)", impl_->boundPort_, config.maxSessions);
    return CoreStatus::Ok;
}

CoreStatus RtspServer::stop() noexcept {
    if (!impl_->running) return CoreStatus::Ok;
    impl_->running = false;

    if (impl_->listenSock != INVALID_SOCKET) {
        closesocket(impl_->listenSock);
        impl_->listenSock = INVALID_SOCKET;
    }

    if (impl_->acceptThread.joinable()) {
        impl_->acceptThread.join();
    }

    // Wait for session threads to finish
    {
        std::lock_guard lock(impl_->sessionMutex);
        for (auto& t : impl_->sessionThreads) {
            if (t.joinable()) t.join();
        }
        impl_->sessionThreads.clear();
    }

    WSACleanup();
    spdlog::info("RTSP: server stopped");
    return CoreStatus::Ok;
}

uint16_t RtspServer::boundPort() const { return impl_->boundPort_; }
bool RtspServer::isRunning() const { return impl_->running; }
void RtspServer::setRequestHandler(RequestHandler handler) { impl_->customHandler = std::move(handler); }
const identity::ReceiverIdentity& RtspServer::identity() const { return impl_->identity; }

}  // namespace arak::rtsp

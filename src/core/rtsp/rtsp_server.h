// src/core/rtsp/rtsp_server.h — Minimal RTSP/1.0 server for AirPlay control.
// Architecture brief C2, PRD §8.3. Handles GET /info, documents errors for unimplemented routes.
#pragma once

#include "common/types.h"
#include "common/identity.h"
#include <cstdint>
#include <memory>
#include <string>
#include <functional>

namespace arak::rtsp {

// Session state machine (brief §5.3).
enum class SessionState {
    Connecting,    // TCP accepted, waiting for first request
    InfoDone,      // GET /info completed
    Paired,        // Pairing completed (Sprint 2)
    Streaming,     // SETUP/RECORD completed
    Tearing,       // TEARDOWN received
    Closed,        // session ended
};

// RTSP request parsed from the wire.
struct RtspRequest {
    std::string method;   // GET, POST, SETUP, etc.
    std::string uri;      // /info, /pair-setup, etc.
    int cseq = -1;        // CSeq header (mandatory per spec)
    std::string contentType;
    std::vector<uint8_t> body;
    std::string peerAddress;
    std::unordered_map<std::string, std::string> headers;
};

// RTSP response to send.
struct RtspResponse {
    int statusCode = 200;
    std::string reason = "OK";
    int cseq = -1;
    std::string contentType;
    std::vector<uint8_t> body;
};

// Server configuration.
struct RtspConfig {
    uint16_t port = 7000;
    int maxSessions = 1;
    int keepAliveTimeoutMs = 15000;
};

// Callback for when a new request arrives.
using RequestHandler = std::function<RtspResponse(const RtspRequest&)>;

class RtspServer {
public:
    RtspServer();
    ~RtspServer();

    // Start listening. Returns BindFailed if port is in use.
    CoreStatus start(const RtspConfig& config, const identity::ReceiverIdentity& identity);

    // Stop listening and close all sessions.
    CoreStatus stop() noexcept;

    // Get the actual port the server bound to (may differ from config.port if 0 was used).
    uint16_t boundPort() const;

    // Is the server running?
    bool isRunning() const;

    // Set a custom request handler (for testing).
    void setRequestHandler(RequestHandler handler);

    // Get identity used by this server.
    const identity::ReceiverIdentity& identity() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arak::rtsp

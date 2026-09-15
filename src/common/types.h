// src/common/types.h — CoreStatus, FrameRef, and shared type definitions.
// Architecture brief §5.1.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>

namespace arak {

enum class CoreStatus {
    Ok = 0,
    Again,           // non-blocking would block; retry later
    Timeout,         // operation timed out
    Unsupported,     // feature not supported
    ProtocolError,   // malformed protocol data
    CryptoError,     // cryptographic operation failed
    IoError,         // file/network I/O error
    NotInitialized,  // component not yet initialized
    ShuttingDown,    // component is shutting down
    AlreadyStarted,  // component already running
    BindFailed,      // socket bind failed
    Unauthorized,    // not paired/not allowed
};

// Convert CoreStatus to a human-readable string.
inline std::string_view coreStatusToString(CoreStatus s) {
    switch (s) {
    case CoreStatus::Ok:            return "Ok";
    case CoreStatus::Again:         return "Again";
    case CoreStatus::Timeout:       return "Timeout";
    case CoreStatus::Unsupported:   return "Unsupported";
    case CoreStatus::ProtocolError: return "ProtocolError";
    case CoreStatus::CryptoError:   return "CryptoError";
    case CoreStatus::IoError:       return "IoError";
    case CoreStatus::NotInitialized:return "NotInitialized";
    case CoreStatus::ShuttingDown:  return "ShuttingDown";
    case CoreStatus::AlreadyStarted:return "AlreadyStarted";
    case CoreStatus::BindFailed:    return "BindFailed";
    case CoreStatus::Unauthorized:  return "Unauthorized";
    default:                        return "Unknown";
    }
}

// A non-owning reference to a frame buffer (brief §5.1).
struct FrameRef {
    uint8_t* data = nullptr;
    size_t   size = 0;
    int64_t  pts_ns = 0;  // presentation timestamp in nanoseconds
};

}  // namespace arak

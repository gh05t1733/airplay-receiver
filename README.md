# AirPlay HD Receiver

A lightweight, open-source AirPlay receiver for Windows — receive HD screen mirroring from your MacBook directly to your PC.

## Status

| Milestone | Status |
|-----------|--------|
| Architecture & Design | ✅ Complete |
| Toolchain Setup (CMake, vcpkg, MSVC) | ✅ Complete |
| CI/CD Pipeline (GitHub Actions) | 🔄 In Progress |
| Sprint 1 — Discovery & Advertisement | ⏳ Up Next |
| Sprint 2 — Video Pipeline (1080p/60fps) | ⏳ Planned |
| Sprint 3 — Audio + UI | ⏳ Planned |
| Sprint 4 — Polish + Release v1.0.0 | ⏳ Planned |

**Current state:** Infrastructure is being bootstrapped. No functional AirPlay receiver code yet. See [docs/architecture/ARCHITECTURE-BRIEF.md](docs/architecture/ARCHITECTURE-BRIEF.md) for the full technical spec.

## What This Is

- **AirPlay receiver** — your PC advertises as an AirPlay target; MacBook discovers it via Control Center
- **1080p @ 60fps** target, low-latency video + audio sync
- **Transient pairing** (no PIN required for initial connection)
- Native C++20 / Win32 / DirectX 11 / WASAPI — zero extra runtime dependencies
- < 100MB installer, CPU < 20%, RAM < 500MB

## What This Is Not

- Not a sender (can't mirror your PC to a Mac)
- Not 4K (stretch goal post-v1.0)
- Not a clone of AirServer/Reflection — fully custom implementation

## Internationalization

The application supports multiple languages:
- **English** (default)
- **Bahasa Indonesia** (included)

Language strings are stored in `assets/i18n/*.json`. Contributions for additional languages are welcome.

## Building

### Prerequisites

- Windows 10/11 (64-bit)
- Visual Studio 2022 or later with C++ workload
- CMake 3.20+
- vcpkg

### Quick Start

```bash
# Install vcpkg (if not already installed)
git clone https://github.com/microsoft/vcpkg C:/dev/vcpkg
C:/dev/vcpkg/bootstrap-vcpkg.bat

# Configure
cmake --preset ci

# Build
cmake --build build --config Release

# Test
ctest --test-dir build --output-on-failure
```

## Architecture

See [docs/architecture/ARCHITECTURE-BRIEF.md](docs/architecture/ARCHITECTURE-BRIEF.md) for the full technical specification.

Key components:
- **RTSP/RTP Receiver** — handles AirPlay protocol handshake
- **Video Decoder** — H.264/H.265 via FFmpeg (NVDEC hardware decode when available)
- **Video Renderer** — DirectX 11 flip-model
- **Audio Decoder** — AAC/ALAC via FFmpeg → WASAPI output
- **mDNS Advertiser** — announces `_airplay._tcp` + `_raop._tcp` via mDNSResponder
- **IPC Bridge** — named pipe JSON-lines between engine and UI

## References

- [UxPlay](https://github.com/FD-/UxPlay) — AirPlay receiver for Raspberry Pi (GPL-3.0, protocol reference only)
- [shairport-sync](https://github.com/mikebrady/shairport-sync) — AirPlay audio receiver (protocol reference only)
- [FFmpeg](https://ffmpeg.org/) — multimedia framework (LGPL)
- [mDNSResponder](https://github.com/apple-oss-distributions/mDNSResponder) — mDNS/DNS-SD (Apache-2.0)

## License

[Apache License 2.0](LICENSE) — patent grant included.

Note: This project references AirPlay protocol behavior for interoperability purposes. AirPlay is a trademark of Apple Inc.

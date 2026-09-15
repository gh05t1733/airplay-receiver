# 📡 ARAHAN: AirPlay HD Receiver — App Lokal

> Ditujukan untuk: **Group Arakatian**
> Status: Draft ready untuk di-implement
> Dibuat: 30 Aug 2026

## ⚡ TL;DR
PC Windows butuh **aplikasi sendiri** yang bisa terima AirPlay HD dari MacBook. Bukan AirServer/Reflection — buatan kita, open source, bisa dikembangkan bareng. Arahan ini roadmap buat grup Arakatian.

## 🎯 Kenapa Kita Buat Sendiri?
- Tools gratis (AirParsec): sering lag, unstable, build Windows outdated
- Tools berbayar (AirServer ~$30): mahal, nggak bisa kustomisasi
- Kita butuh **HD stable** (1080p/60fps), **audio sync**, **low latency**
- Project ini bisa jadi **unggulan Arakatian** — open source, bermanfaat banyak orang

## 📋 Target Spesifikasi
| Item | Target |
|---|---|
| Platform | Windows 10/11 (64-bit) |
| Resolusi | 1080p @ 60fps (min), 4K @ 30fps (ideal) |
| Audio | Sync video, stereo/spatial, latency < 50ms |
| Discovery | Otomatis deteksi MacBook (mDNS) |
| Installer | < 100MB, single file, plug-and-play |
| Performance | CPU < 20%, RAM < 500MB |
| Latency total | < 100ms (interactive feel) |

## 🛠️ Tech Stack (Rekomendasi Final)

### Core Engine (C/C++) — Performance-Critical
| Modul | Library | Alasan |
|---|---|---|
| RTSP/RTP | **Live555** (C++) | Open source, battle-tested, native AirPlay |
| Streaming/Demux | **FFmpeg** (libav*) | Standard, semua codec didukung |
| Video Decode | FFmpeg + **NVDEC** | Hardware NVIDIA, hemat CPU |
| Video Render | **DirectX 11** | Native Windows, performa tinggi |
| Audio Output | **WASAPI** | Windows audio, latency rendah |
| Discovery | **mDNSResponder** (Apple) | Standar AirPlay device discovery |
| Build System | **CMake** | Cross-platform, standar industri |

### UI Frontend (Pilih Satu)
- **WPF (C#)**: Native Windows, XAML, tercepat develop → **RECOMMENDED MVP**
- **Tauri (Rust + Web)**: Modern, ringan, cross-platform → kalau tim suka web stack
- **Electron (JS/TS)**: Paling gampang, tapi besar (~150MB) → kalau tim mayoritas web dev

### Distribution
- **Installer**: WiX Toolset (MSI) atau Inno Setup (EXE)
- **CI/CD**: GitHub Actions (Windows runner, auto-build per PR)

## 🏗️ Architecture

```
┌────────────────────────────────────────────────┐
│              UI Layer (WPF/Tauri/Electron)      │
│   Settings · Controls · Display · Status Bar    │
├────────────────────────────────────────────────┤
│            IPC / API Layer                      │
│   REST API · WebSocket · Named Pipes           │
├────────────────────────────────────────────────┤
│          Core Engine (C/C++)                    │
│                                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ mDNS      │  │ RTSP/RTP │  │ FFmpeg       │ │
│  │ Discovery │  │ Receiver │  │ Codec Engine │ │
│  └────┬─────┘  └────┬─────┘  └──────┬───────┘ │
│       │              │              │          │
│  ┌────▼──────────────▼──────────────▼────────┐ │
│  │         Stream Pipeline                    │ │
│  │  Demux → Decode → Sync → Render/Play     │ │
│  └────────┬──────────────────┬──────────────┘ │
│           │                  │                │
│  ┌────────▼──────┐  ┌───────▼────────────┐   │
│  │ DirectX 11    │  │ WASAPI Audio       │   │
│  │ Video Render  │  │ Output (Low Lat)   │   │
│  └───────────────┘  └────────────────────┘   │
├────────────────────────────────────────────────┤
│             Windows API Layer                 │
│   Win32 · DirectX · WASAPI · Network          │
└────────────────────────────────────────────────┘
```

## 📅 Sprint Breakdown (4 Sprint × 2 Minggu = 8 Minggu)

### Sprint 1: Foundation (Minggu 1-2)
**Goal**: Aplikasi dasar bisa terima stream & render frame.
**Deliverable**: RTSP receiver + mDNS discovery + window dengan frame
- [ ] Setup repo GitHub + CI/CD skeleton (GitHub Actions)
- [ ] Integrasi Live555 + FFmpeg (RTSP connection, demuxer)
- [ ] mDNS discovery (Mac di jaringan, port 5353)
- [ ] Window minimal (WPF atau Tauri — tergantung tim)
- [ ] Render frame pertama (wallpaper/placeholder)
- [ ] Basic config system (resolusi, audio toggle)
**Acceptance**: Connect ke MacBook → window nampil, meski frame hitam.

### Sprint 2: Video Pipeline (Minggu 3-4)
**Goal**: Video HD bisa ditampilkan tanpa lag.
**Deliverable**: 1080p/60fps smooth, hardware decode aktif
- [ ] Video decoder H.264/H.265 (libavcodec)
- [ ] Video renderer DirectX 11 (NVDEC hardware accelerate)
- [ ] Streaming test dari MacBook (live mirror)
- [ ] Performance profiling (CPU/GPU/RAM via PerfView)
- [ ] Resize/scaling pipeline (fit-to-window, aspect-ratio)
- [ ] Frame sync vs audio (preliminary)
**Acceptance**: 1080p/60fps stabil, frame drop < 1%, CPU < 30%.

### Sprint 3: Audio + UI Polish (Minggu 5-6)
**Goal**: Audio sync + UI fungsional + kontrol user.
**Deliverable**: Aplikasi fitur lengkap
- [ ] Audio decoder + WASAPI output (sync video, latency < 50ms)
- [ ] UI Settings page (resolusi, audio config, network info)
- [ ] Status bar realtime (FPS, latency, connection status)
- [ ] System tray integration (minimize, notifications)
- [ ] Hotkeys (toggle mirror, screenshot, volume control)
- [ ] Disconnect/reconnect handling (graceful recovery)
**Acceptance**: Audio-video sync < 50ms, semua UI fitur bekerja.

### Sprint 4: Polish + Release (Minggu 7-8)
**Goal**: Production-ready + distributable.
**Deliverable**: v1.0.0 Stable release
- [ ] Performance optimization (memory leak fix, buffer tuning)
- [ ] Error handling (bad stream, network drop, recovery auto)
- [ ] Logging system (file log, rotasi, verbosity levels)
- [ ] Installer build (WiX/Inno Setup) — MSI & EXE
- [ ] Documentation: README.md, API reference, troubleshooting
- [ ] Beta release (group internal, 1 minggu testing)
- [ ] Stable release v1.0.0 + GitHub Release + installer download
**Acceptance**: Installer < 100MB, clean Windows install, all features work.

## 👥 Peran (Role Assignment Arakatian)
| Peran | Tugas | Minimal 1 Orang |
|---|---|---|
| **Project Lead** | Koordinasi, roadmap, review PR, release approval | ✅ |
| **Engine Dev** | C/C++ core, FFmpeg, Live555, DirectX, performance | ✅ |
| **UI Dev** | WPF/Tauri/Electron frontend, UX, controls | ✅ |
| **QA/Tester** | Testing, bug report, performance benchmark | ✅ (bisa jadi Engine Dev) |
| **Documenter** | README, API docs, troubleshooting guide | ✅ (bisa jadi UI Dev) |

**Minimal tim**: 3 orang (Lead + Engine + UI/QA). Bisa dikerjakan.

## 📁 Struktur Folder
```
airplay-receiver/
├── README.md
├── LICENSE (Apache 2.0 / MIT)
├── CONTRIBUTING.md
├── CMakeLists.txt
├── docs/
│   ├── architecture.md
│   ├── api-reference.md
│   └── contributing.md
├── src/
│   ├── core/ (C/C++)
│   │   ├── rtsp_receiver/ (RTSP connection, RTP parser)
│   │   ├── mdns_discovery/ (mDNS browser, resolver)
│   │   ├── codec/ (video/audio decoder)
│   │   ├── renderer/ (DirectX video renderer)
│   │   ├── audio/ (WASAPI output, sync engine)
│   │   └── pipeline/ (stream pipeline, sync, buffer)
│   ├── ui/ (C# WPF / Tauri / Electron)
│   └── common/ (logger, config, utils)
├── tests/ (unit, integration, performance)
├── scripts/ (build, install, CI)
├── third_party/ (submodules: ffmpeg, live555, mdns)
└── dist/ (build output, gitignored)
```

## 🚀 Cara Kontribusi
1. **Fork** repo → clone → branch `feature/nama-fitur`
2. **Commit** dengan Conventional Commits: `feat:`, `fix:`, `docs:`, `test:`, `perf:`
3. **PR** → minimal 1 review dari core team
4. **Merge** ke `main` → auto-deploy beta (GitHub Actions)
5. **Release** → tag `vMAJOR.MINOR.PATCH` → installer auto-build

## ⚠️ Risiko & Mitigasi
| Risiko | Mitigasi |
|---|---|
| AirPlay encryption (AES-128) sulit di-reverse | Mulai dari unencrypted mode, gunakan pyatv/lib sebagai referensi |
| HD performance rendah | NVDEC hardware decode, profiling Sprint 2, optimization Sprint 4 |
| mDNS unreliable Windows | Fallback manual IP entry, install Bonjour requirement |
| Tim Arakatian kecil | Scope kecil (MVP 1080p/60fps, 4K nanti) |
| Windows API complexity | Abstraction layer di core/, unit-testable tanpa UI |

## 💡 Tips Grup Arakatian
- **MVP dulu**: window + frame → video → audio → UI → polish. Jangan sekaligus.
- **Test tiap sprint**: Jangan tunggu 8 minggu baru test — unit test tiap commit.
- **Dokumentasi penting**: README, API docs, troubleshooting = future contributor lifeline.
- **GitHub Project Board**: tracking sprint, assign per task.
- **Discord/Slack channel**: daily standup ringan (15 menit).

## Referensi
- FFmpeg: https://ffmpeg.org/
- Live555: http://www.live555.com/
- mDNSResponder: https://github.com/apple-oss-distributions/mDNSResponder
- WASAPI: https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi
- DirectX: https://learn.microsoft.com/en-us/windows/win32/directx
- AirPlay Spec: https://support.apple.com/en-us/HT204370
- pyatv (referensi impl): https://github.com/popcornmrt/pyatv

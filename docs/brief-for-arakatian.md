# 📡 BRIEF: AirPlay HD Receiver — Project Baru untuk Group Arakatian

> Kirim ini ke group ARAKATIAN. Copy paste aja, bro.

---

## 🚀 NEW PROJECT — AirPlay HD Receiver

**Owner:** Group Arakatian
**Status:** READY TO BUILD
**Workspace:** `C:/Users/REUMMM/airplay-receiver/`

### Apa ini?
Kita bikin **aplikasi open source** buat PC Windows supaya bisa **terima AirPlay HD dari MacBook**. Bukan pakai AirServer/Reflection — ini buatan kita sendiri, bisa kita kembangkan bareng.

### Kenapa?
- Tools gratis (AirParsec) sering lag & unstable
- Tools berbayar (AirServer) mahal & gak bisa dikustomisasi
- Kita butuh HD stable (1080p/60fps), audio sync, low latency
- Project ini bisa jadi unggulan Arakatian — open source, bermanfaat banyak orang

### Target Spesifikasi
| Item | Target |
|------|--------|
| Platform | Windows 10/11 (64-bit) |
| Resolusi | 1080p @ 60fps (min), 4K @ 30fps (ideal) |
| Audio | Sync video, stereo/spatial, latency < 50ms |
| Discovery | Otomatis deteksi MacBook (mDNS) |
| Installer | < 100MB, plug-and-play |
| Performance | CPU < 20%, RAM < 500MB |

### Tech Stack
- **Core Engine:** C/C++ (FFmpeg + Live555 + WASAPI + DirectX)
- **UI:** WPF (.NET 8) atau Tauri (Rust + web frontend)
- **Build:** CMake, CI/CD GitHub Actions
- **Installer:** WiX/Inno Setup

### Sprint Plan (4 × 2 minggu = 8 minggu)

**Sprint 1 — Foundation (Minggu 1-2)**
- Setup repo, CI/CD skeleton
- RTSP/RTP basic receiver (Live555 + FFmpeg)
- mDNS advertisement (PC jadi AirPlay receiver, Mac lihat di Control Center)
- Window minimal (WPF/Tauri)
- Render frame pertama
- 🎯 Deliverable: Mac lihat PC di AirPlay list, bisa connect, ada frame

**Sprint 2 — Video Pipeline (Minggu 3-4)**
- Video decoder H.264/H.265 (FFmpeg libavcodec)
- Video renderer DirectX 11 (NVDEC hardware decode)
- HD quality test (1080p/60fps)
- Resize/scaling pipeline
- 🎯 Deliverable: 1080p/60fps smooth, frame drop < 1%

**Sprint 3 — Audio + UI (Minggu 5-6)**
- Audio decoder + WASAPI output (sync video)
- UI Settings (resolusi, audio, network)
- Status display (FPS, latency, connection)
- System tray + hotkeys
- 🎯 Deliverable: Audio sync < 50ms, UI lengkap

**Sprint 4 — Polish + Release (Minggu 7-8)**
- Performance optimization
- Error handling & logging
- Installer (MSI/EXE)
- Documentation (README, API docs)
- Beta testing → Stable v1.0.0
- 🎯 Deliverable: v1.0.0 production-ready

### Peran yang Dibutuhkan
| Role | Tanggung Jawab | Min Orang |
|------|---------------|-----------|
| Project Lead | Koordinasi, roadmap, review | 1 |
| Engine Dev | C/C++ core, FFmpeg, streaming | 1 |
| UI Dev | Frontend, UX, controls | 1 |
| QA/Tester | Testing, bug report | 1 |

**Minimal tim:** 3 orang (1 Lead + 1 Engine + 1 UI/QA)

### Spec & Arahan Lengkap
File udah ada di vault:
- `Projects/AirPlay-PC/Arahan-AirPlay-App.md` — full spec + architecture + sprint breakdown
- `Projects/AirPlay-PC/Dev-Environment-AirPlay.md` — setup dev environment
- `Knowledge/AirPlay-App.md` — knowledge note

### Workflow (Studio Pipeline)
```
@foundry → @architect → @prd-maker → @reviewer → @code-executor → @reviewer → @deployer → @docs-writer
```

### Risiko & Mitigasi
| Risiko | Mitigasi |
|--------|---------|
| AirPlay encryption sulit | Mulai unencrypted, pakai UxPlay/pyatv referensi |
| HD performance rendah | NVDEC hardware decode, profiling sprint 2 |
| mDNS unreliable Windows | Fallback manual IP, Bonjour service |
| Tim kecil | Scope small (1080p MVP, 4K nanti) |

### Referensi
- [UxPlay](https://github.com/FDH2-UxPlay/UxPlay) — AirPlay receiver RPi (open source, GPL)
- [shairport-sync](https://github.com/mikebrady/shairport-sync) — AirPlay audio (open source, MIT)
- [pyatv](https://github.com/postlund/pyatv) — AirPlay protocol reference (open source, MIT)
- [FFmpeg](https://ffmpeg.org/) — multimedia framework
- [Live555](http://www.live555.com/) — RTSP/RTP library

---

**Gas @foundry! Mulai dari Sprint 1 (Foundation). 🏗️**

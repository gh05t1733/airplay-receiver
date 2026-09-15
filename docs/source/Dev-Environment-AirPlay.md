# 🛠️ Dev Environment Setup — AirPlay Receiver
> Untuk kontributor baru Group Arakatian
> Update: 30 Aug 2026

## Prasyarat (Wajib)
- Windows 10/11 (64-bit)
- **Visual Studio 2022** → Workload: *C++ desktop development*
- **CMake** 3.20+ (https://cmake.org)
- **Git** (https://git-scm.com)
- **NVIDIA GPU** (untuk hardware decode NVDEC)

## Prasyarat (UI — Sesuai Stack yang Dipilih)
- **WPF (C#)**: [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0)
- **Tauri (Rust)**: [Node.js 20+](https://nodejs.org) + [Rust](https://rustup.rs)
- **Electron (JS/TS)**: [Node.js 20+](https://nodejs.org)

## Setup Cepat (5 Menit)

1. **Clone repo** (ganti URL sesuai repo aktual Arakatian):
   ```bash
   git clone https://github.com/arakat/airplay-receiver.git
   cd airplay-receiver
   ```
2. **Init submodule** (FFmpeg, Live555, mDNS):
   ```bash
   git submodule update --init --recursive
   ```
3. **Buat build folder**:
   ```bash
   mkdir build && cd build
   ```
4. **CMake configure** (atau `cmake-gui` kalau mau GUI):
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```
5. **Build** (multi-core):
   ```bash
   cmake --build . --config Release --parallel
   ```
6. **Jalankan**:
   ```bash
   ../release/airplay-receiver.exe
   ```

## Verifikasi Setup
- [ ] Aplikasi muncul window (meski hitam/placeholder)
- [ ] MacBook terdeteksi (buka app → list device)
- [ ] Frame streaming berjalan (1080p)
- [ ] Audio sync (tidak desync parah)

## Troubleshooting Setup
| Masalah | Solusi |
|---|---|
| `C++ compiler not found` | Install "C++ desktop development" di Visual Studio Installer |
| FFmpeg not found / error | Download prebuilt FFmpeg, set env var `FFMPEG_PATH=C:\ffmpeg` |
| MacBook nggak terdeteksi | Install **Bonjour Print Services** di PC, pastikan same WiFi |
| DLL missing saat run | Copy DLL dari `third_party/` ke folder `release/` |
| NVIDIA NVDEC error | Update driver NVIDIA (GeForce Experience) |
| CMake error Live555 | Pastikan submodule berhasil: `ls third_party/live555` |
| Build gagal (Windows SDK) | Install "Windows SDK 10.x" via VS Installer |

## Variabel Lingkungan Penting
| Env Var | Default | Keterangan |
|---|---|---|
| `FFMPEG_PATH` | — | Path ke FFmpeg installasi |
| `LIVE555_PATH` | `third_party/live555` | Path ke Live555 source |
| `CMAKE_BUILD_TYPE` | `Release` | `Debug` / `Release` |

## Bermanfaat
- [CMake Docs](https://cmake.org/cmake/help/latest/)
- [FFmpeg Build Docs](https://ffmpeg.org/documentation.html)
- [Live555 Usage](http://www.live555.com/liveMedia/)
- [WPF Documentation](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/)
- [DirectX Docs](https://learn.microsoft.com/en-us/windows/win32/directx)
- [WASAPI Docs](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi)

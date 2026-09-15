# AirPlay HD Receiver — Knowledge
> Project: Aplikasi open source AirPlay HD receiver (PC ← MacBook)
> Arahan: [[Projects/AirPlay-PC/Arahan-AirPlay-App|Arahan-AirPlay-App]]

## Topik
- **Core Engine** — RTSP/RTP receiver (Live555 + FFmpeg), DirectX renderer, WASAPI audio
- **mDNS Discovery** — penemuan device MacBook di jaringan
- **Hardware Acceleration** — NVIDIA NVDEC untuk decode HD tanpa beban CPU
- **Audio Sync** — WASAPI output, sync engine latency < 50ms
- **Windows Integration** — installer, system tray, hotkey, driver NVIDIA
- **Dev Workflow** — CMake, CI/CD, PR review, sprint tracking

## Entitas (Semantica)
| Entitas | Type | Status |
|---|---|---|
| airplay-app-project | project | Active draft |
| airplay-core-engine | stack | C/C++ FFmpeg + Live555 + DirectX + WASAPI |
| airplay-ui-layer | stack | WPF (recommended) / Tauri / Electron |

## Related
- Blueprint: [[../Blueprints/BP-AirPlay-PC|BP-AirPlay-PC]]
- Project Arahan: [[../Projects/AirPlay-PC/Arahan-AirPlay-App|Arahan-AirPlay-App]]
- Setup Dev: [[../Projects/AirPlay-PC/Dev-Environment-AirPlay|Dev-Environment-AirPlay]]
- Hardware: [[../Knowledge/Hardware-GPU|Hardware-GPU]] (RTX 3060, NVDEC)

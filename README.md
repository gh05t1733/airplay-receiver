# AirPlay HD Receiver

A lightweight, open-source AirPlay receiver for Windows — receive HD screen mirroring from your MacBook directly to your PC.

## Status

| Milestone | Status |
|-----------|--------|
| Architecture & Design | ✅ Complete (brief v3.3, PRD Rev 4, DESIGN.md) |
| Toolchain (CMake, vcpkg, Ninja, MSVC) | ✅ Working on the dev machine |
| CI/CD Pipeline (GitHub Actions) | ❌ Red — first run failed on generator discovery |
| Sprint 1 — Discovery & Advertisement | ⏳ Not started |
| Sprint 2 — Video Pipeline (1080p/60fps) | ⏳ Planned |
| Sprint 3 — Audio + UI | ⏳ Planned |
| Sprint 4 — Polish + Release v1.0.0 | ⏳ Planned |

**Current state (commit `5d0b57d`):** specification and build scaffolding only. **No AirPlay code has been written or compiled yet**, so this app does not receive, decode or show anything today. The first CI run ([run 35007094126](https://github.com/gh05t1733/airplay-receiver/actions/runs/35007094126)) **failed** — CMake could not find a Visual Studio instance on the runner; the fix (switch CI to the Ninja generator, which the runner does have) is in progress. Sprint 1 gates 1.1 ("builds from a clean checkout") and 1.10 ("CI runs on push") are therefore **not met**. See [What is not done yet](#what-is-not-done-yet) and the [Architecture Brief](docs/architecture/ARCHITECTURE-BRIEF.md).

## What This Is

What it is aiming at — **none of this is implemented yet**, it is the target the sprints are gated against:

- **AirPlay receiver** — your PC advertises itself as an AirPlay target (`_airplay._tcp` + `_raop._tcp`); the MacBook finds it in Control Center
- **1080p @ 60fps** target, low-latency video with synchronised audio (first rendered frame is a **Sprint 2** gate, not Sprint 1)
- **Transient pairing** — a locked product decision (no PIN screen needed); the pairing implementation itself is Sprint 2 work
- Native C++20 / Win32 / DirectX 11 / WASAPI
- Targets: installer `< 100 MB`, CPU `< 20 %`, RAM `< 500 MB`

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
- MSVC C++ toolset with the C++ workload — Visual Studio Build Tools 2026 (VS 18) on this machine, VS 2022 on the CI runner
- CMake 3.25+ (4.4.3 verified in the dev environment)
- Ninja 1.13.2 — needed by the `ninja` preset; run from a Developer Command Prompt or call `vcvars64.bat` first
- vcpkg at `C:/dev/vcpkg` (**outside** this repo — the `base` preset hardcodes that path)

### Quick Start (CMake presets)

Run from a Developer Command Prompt, so `vcvars64.bat` has already put the MSVC toolset on `PATH`:

```bat
cmake --preset local-vs18
cmake --build --preset local-vs18-release
ctest --preset local-vs18-release
```

Available presets: `local-vs18` (VS 18 Build Tools 2026 — the dev machine), `ninja` (single-config, needs vcvars; `CMAKE_BUILD_TYPE` is meaningful here), `ci` (VS 17 2022 — CI only). Build output goes to `build/<presetName>/`, which is why the build and test steps above use `--preset` instead of a bare `build`.

> ⚠️ **Known issue — the presets cannot be read yet, so the commands above fail today.**
> `CMakePresets.json` declares `"version": 6` while CMake 4.x rejects a presets file whose `$schema` sits below version 8:
> `CMake Error: Could not read presets ... File version must be 8 or higher for $schema support`
> Verified by A/B on the same file: `"version": 8` lists the presets, `"version": 6` errors out. Either bump the version (the presets path then needs the newer CMake) or drop the `$schema` key. Owner: build tooling.
>
> Treat everything in this section as the project's **intended** build flow, not as verified steps: no build has ever produced a green CI run yet (see [What is not done yet](#what-is-not-done-yet)). When the two disagree, the CI run is the truth.

FFmpeg is **not** needed to configure, build or test Sprint 1 — it lives behind the `video` vcpkg feature (default off), and `mdnsresponder` behind `mdns`; enable them deliberately with `-DVCPKG_MANIFEST_FEATURES=video` (FFmpeg's vcpkg build trees are several GB).

## Architecture

See [docs/architecture/ARCHITECTURE-BRIEF.md](docs/architecture/ARCHITECTURE-BRIEF.md) for the full technical specification (987 lines, 10 sections, source-URLed).

Planned components — **none of these exist in code yet**, and the sprint in brackets is the gate they land in:

- **mDNS Advertiser** *(Sprint 1)* — announces `_airplay._tcp` + `_raop._tcp` via the Bonjour daemon's `DNSServiceRegister`; the process itself never binds port 5353
- **RTSP/RTP Receiver** *(Sprint 1)* — `/info` bplist + OPTIONS/ANNOUNCE/SETUP/RECORD handling; pairing (transient), AES-128-CBC and `fp-setup` follow in Sprint 2
- **Native window + status overlay** *(Sprint 1)* — Win32 + Direct3D 11, DirectWrite text
- **Video Decoder / Renderer** *(Sprint 2)* — H.264/H.265 via FFmpeg with NVDEC when available, D3D11 flip-model present
- **Audio Decoder / Output** *(Sprint 3)* — AAC/ALAC via FFmpeg → WASAPI, video-synchronised
- **IPC Bridge** *(Sprint 3)* — named-pipe JSON-lines between the engine and the UI, schema in the brief §5.8

## What is not done yet

Being explicit, because a spec-heavy repository invites wishful reading:

- **No AirPlay protocol code.** Total committed source is ~100 lines of scaffold: `src/common/version.*`, an empty `wWinMain` in `src/core/app/main.cpp`, and one unit test. No RTSP, no bplist, no pairing, no AES.
- **No advertisement** — the MacBook will not see this PC in Control Center (Sprint 1 gate 1.2/1.3).
- **No `/info` endpoint, no window** (Sprint 1 gates 1.4–1.8).
- **No video and no audio** — decoders, renderer and WASAPI output are Sprint 2/3.
- **No pairing of any kind** — transient pairing is a locked decision, not an implementation.
- **No installer, no release** — Sprint 4. 4K is a stretch goal after v1.0.
- **CI has never been green.** The first run failed before compiling anything.

House rule for this repo: a gate counts as met only with a **CI run URL or captured command output** attached. Unverified claims are not evidence — if this README, the brief or the PRD says something is done, there should be a link next to it.

## Documentation

| Document | What it holds |
|---|---|
| [Architecture Brief](docs/architecture/ARCHITECTURE-BRIEF.md) | Protocol reality check with source URLs, stack per layer, threading/IPC schema, risk register, sprint gates |
| [PRD — Sprint 1](docs/prd/PRD-AIRPLAY-SPRINT1.md) | Scope, task list, gate checklists, open decisions |
| [DESIGN.md](docs/design/DESIGN.md) | Window, overlay, pairing surfaces, theme tokens for the native UI |
| [HANDOFFS.md](docs/pipeline/HANDOFFS.md) | Pipeline handoff log: who handed what to whom, with evidence |
| [docs/source/](docs/source/) | Original briefs and environment notes the project started from |

## References

- [UxPlay](https://github.com/FD-/UxPlay) — AirPlay receiver for Raspberry Pi (GPL-3.0, protocol reference only)
- [shairport-sync](https://github.com/mikebrady/shairport-sync) — AirPlay audio receiver (protocol reference only)
- [FFmpeg](https://ffmpeg.org/) — multimedia framework (LGPL)
- [mDNSResponder](https://github.com/apple-oss-distributions/mDNSResponder) — mDNS/DNS-SD (Apache-2.0)

## License

[Apache License 2.0](LICENSE) — patent grant included.

Note: This project references AirPlay protocol behavior for interoperability purposes. AirPlay is a trademark of Apple Inc.

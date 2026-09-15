# PRD — AirPlay HD Receiver (Windows) — Sprint 1: Foundation

> STEP 3 of the studio pipeline. Author: @prd-maker. Date: 2026-09-16.
> Inputs: `docs/architecture/ARCHITECTURE-BRIEF.md` (875 lines, 10/10 sections), the pre-review
> findings from @reviewer (1 blocker + 4 advisory), and the toolchain audit from @deployer.
> Downstream: @ui-designer (DESIGN.md) → @code-executor (Task List §9) → @reviewer (gate §10).
> Licence target: Apache-2.0. Sprint 1 window: weeks 1–2 (express as a **gate sequence**, not a calendar — see §5.4).
> **Rev 3 (2026-09-16):** mDNS daemon contract added (§0.7) after three independent measurements; FFmpeg
> removed from the Sprint 1 critical path; gate 1.10 evidence now exists (pushed + CI triggered);
> the `.gitignore` trap that would silently break a fresh clone is documented in Task 1.
> **Rev 4 (2026-09-16):** CI run #1 came back **RED** and the root cause was read from the raw log — CMake's
> VS generator finds **no Visual Studio instance** on the runner, so **Path B (`-G Ninja`) becomes mandatory
> in CI** (§0.1) and the artifact path must change with it (§0.8). Gates 1.1/1.10 are **not met**.

---

## 0. Normative corrections to the Architecture Brief (BLOCKING — read this first)

These four items override the brief where they disagree. §0.1 is a **blocker**: without it gate 1.1 dies on the first push.

### 0.1 🔴 CI generator (brief §6.3 contradicts brief §3) — BLOCKER
The brief's `ci.yml` hardcodes `-G "Visual Studio 18 2026"`, but the brief's own §3 correctly says the
GitHub `windows-latest` runner ships **Visual Studio 2022 (17.14)**. `actions/runner-images` confirms
the runner has VS 2022 Enterprise 17.14.37614.0, CMake 3.31.6, Ninja 1.13.2, InnoSetup 6.7.1 and
`VCPKG_INSTALLATION_ROOT=C:\vcpkg`. **No runner has a "Visual Studio 18 2026" generator**, and this
machine's VS 18 (v14.51) is *local only*.

| Context | Generator (normative) |
|---|---|
| **CI (`ci.yml`)** | `-G "Visual Studio 17 2022"` — or `-G Ninja` (Ninja is preinstalled on the runner) |
| **Local dev (this box)** | `-G "Visual Studio 18 2026"` — confirm the exact string with `cmake --help` **after** CMake is installed; never hardcode from memory |

**STATUS: REVISED AGAIN 2026-09-16 — Path B (Ninja) is now REQUIRED in CI, because Path A was tried and
failed empirically.** The brief patch itself was correct and is landed; what was wrong is the *default choice*
inside it. CI run #1 (§0.8) died with:

```
CMake Error at CMakeLists.txt:3 (project):
  Generator  Visual Studio 17 2022  could not find any instance of Visual Studio.
```

So "the runner has VS 2022" is not the operative fact — CMake's VS generator resolves a *VS instance* through
vswhere and finds **none** on this runner. Meanwhile `ilammy/msvc-dev-cmd@v1` had already produced a working
MSVC environment in that same job, which is precisely the precondition Path B needs. **Do not re-litigate
this with documentation; the log is the evidence.**

| Path | Configure | Verdict |
|---|---|---|
| **B — REQUIRED in CI** | `-G Ninja` + `-DCMAKE_BUILD_TYPE=Release` | Single-config, so `CMAKE_BUILD_TYPE` is **required and meaningful**; needs an MSVC environment, which `ilammy/msvc-dev-cmd@v1` supplies (already a step) |
| **A — do NOT use on this runner** | `-G "Visual Studio 17 2022" -A x64` | Multi-config; **empirically unavailable** on `windows-latest` (§0.8). Also `-DCMAKE_BUILD_TYPE` is a no-op under a VS generator |
| Local dev only | `-G "Visual Studio 18 2026"` | Exists **only** on this box — proved by @deployer with `cmake --help` |

**Correction folded in (@reviewer nit, proven by @code-executor):** the brief's `ci.yml` passes
`-DCMAKE_BUILD_TYPE=Release` while using a **multi-config** generator, where that variable is a **no-op**.
This was measured, not assumed — with the VS generator `CMAKE_BUILD_TYPE` is absent from `CMakeCache.txt`
entirely; with Ninja it is present and authoritative. **Do one or the other, never both:** on path A drop
`-DCMAKE_BUILD_TYPE` and rely on `--config Release`; on path B keep it (Ninja is single-config).
Two more pitfalls that cost real time: Ninja+MSVC **requires** vcvars while the VS generator finds the
toolchain unaided; and **never** put the build directory under `%TEMP%` (MSBuild raises `MSB8029` and
incremental builds break) — use in-repo `build/`.

### 0.2 §9 open-decision items — RESOLVED
**STATUS: RESOLVED 2026-09-16.** Brief §9.1 now records the user's locked decisions **D1–D4** and §9.2 carries
the single remaining Sprint 1 decision (transient vs PIN pairing); brief §8 now says "gate order, not calendar".
Both previously-missing items are present:
1. **Pairing profile (transient vs PIN)** — decides which `features` bits we advertise in Sprint 1 and whether
   Sprint 2 owes a PIN surface. See §11.4 — **reconciled, my earlier call was wrong**.
2. **"First frame is a Sprint 2 gate, not Sprint 1"** — the original user brief said Sprint 1 = "ada frame".
   That changes a user-facing expectation, so the user confirmed it explicitly (D3), not as a silent edit.

### 0.3 User decisions are recorded, not assumed
**STATUS: CONFIRMED by @user on 2026-09-16 ("gas aja")** — (a) UI stack = **Win32 + D3D11**, (b) **4K cut** from
v1.0, (c) **first frame = Sprint 2**. Recorded as D1–D3 in brief §9.1 and restated in §11. One decision is still
open and it gates Sprint 1: the pairing profile (§11.4). A second, smaller one is flagged there too: the UI
language (§11.5).

### 0.4 A green CI is the only definition of "it builds"
The brief already says this (§6.3) and it is the single most load-bearing rule in the project: a laptop may
lack CMake or a GPU, but a red CI on `windows-latest` is never explainable away. Gate 1.1 is evaluated **only**
on a clean-checkout CI run with an uploaded artifact.

### 0.5 Task 0 status — the toolchain is PARTIALLY done (verified on disk, not claimed)
@deployer executed the day-1 installs and @reviewer independently confirmed the versions on this machine:

| Item | Status | Evidence |
|---|---|---|
| CMake | ✅ done | `cmake version 4.4.3` at `C:\Program Files\CMake\bin\cmake.exe` (winget `Kitware.CMake`) |
| Ninja | ✅ done | `1.13.2` (winget `Ninja-build.Ninja`) |
| vcpkg | ✅ done | `2026-07-27-98d7cb0c` at **`C:/dev/vcpkg`** — deliberately **outside** the repo |
| Disk guard | ✅ set | `VCPKG_BINARY_SOURCES=clear;files,%LOCALAPPDATA%\vcpkg\archives,readwrite` + `VCPKG_DISABLE_METRICS=1` in `HKCU\Environment` |
| MSVC + Windows SDK | ✅ present | `cl.exe` 14.51.36231, SDK 10.0.26100.0 — never was the blocker |
| **Bonjour / mDNS** | ✅ **installed AND RUNNING** | `Bonjour Service` is up; `mDNSResponder.exe` owns `192.168.1.14:5353` and three further PIDs hold `0.0.0.0:5353`; `dns-sd.exe` + `dnssd.dll` are in `System32`; `dns_sd.h` is **absent** and must be vendored — see §0.7 |
| **FFmpeg dev (headers + libs)** | ⏸️ **NOT a Sprint 1 blocker** — my Rev-2 call was wrong | the installed gyan build is bin-only, but @architect gated FFmpeg behind the opt-in vcpkg feature `video`, so Sprint 1's default set is only `openssl + spdlog + catch2`. Gate 1.1 no longer waits on an FFmpeg build. The RAM/disk cost is **deferred, not solved** — gate 2.0 meets it |
| First commit / remote / `ci.yml` | ✅ pushed, 🔴 **run RED** | pushed to `github.com/gh05t1733/airplay-receiver`, branch `main`, commit `5d0b57d5`, 26 files, no junk. `ci.yml` triggered and **failed at configure** — root cause and the exact 5-line fix are in §0.8 |

**Correction to Task 0 below:** the vcpkg root is **`C:/dev/vcpkg`**, not `C:/vcpkg` — keeping vcpkg out of the
repo means `.gitignore` needs no special-casing.
**Remaining Task 0 work:** nothing that blocks. First commit + push are **done**; FFmpeg dev is deferred to
Sprint 2 behind the `video` feature.
**Gates 1.1 and 1.10 are now evaluable — and the first evaluation came back RED** (§0.8): the vcpkg dependency
half **passed** (11 min, full default set), and configure then failed on VS-generator discovery. A *local*
`vcpkg` build on this box was killed three times by the harness (`exit_code -15`), so **no local result counts
as evidence** (§0.4) and the CI log is the sole authority.

### 0.6 Unverified claims are not evidence
Nothing in Sprint 1 is "done" until its gate names a concrete artifact: a CI run URL, a `dns-sd` transcript,
a captured request/response, a `RenderStats` sample, a file path with content, or an exit code. Every handoff
must separate **proven** from **not yet proven**.

### 0.7 mDNS reality on this machine — measured, and it changes the design (Rev 3)
Measured independently three times (by @foundry, @architect and @reviewer — and they agree):

| Fact | Consequence |
|---|---|
| `Bonjour Service` **RUNNING**; `mDNSResponder.exe` owns `192.168.1.14:5353` and **three further PIDs** hold `0.0.0.0:5353` | **UDP 5353 is already owned.** A responder of ours that binds it picks a fight with a live system daemon — and gate 1.3 lives or dies on that |
| `dns-sd.exe` (96,104 B) and `dnssd.dll` (85,864 B) exist in `System32` | The gate 1.2 / 1.11 verifier is **free**. Do **not** build the vcpkg `dns-sd` CLI — it fails `RC1015 afxres.h` because VS 18 BuildTools ships no `atlmfc` (measured by @architect, confirmed by @reviewer) |
| `dns_sd.h` is **absent**; no Bonjour SDK installed | The header must be vendored into `third_party/mDNSResponder/include/`. It is **Apache-2.0**, so it may be committed — subject to the `.gitignore` rule in Task 1 |
| Bonjour is a **runtime dependency** of the shipped app | Sprint 4's installer owes an end-user story (detect + instruct, or bundle **after** checking Apple's redistribution terms). Gate 4.3 / C19 — not a Sprint 1 concern |

**Normative contract — @code-executor must not improvise here:**
1. **Advertise through a daemon client API** (`DNSServiceRegister` family). Never bind 5353 while a daemon
   holds it. Our process must be able to show at gate 1.3 that **its own PID does not own `:5353`**.
2. **Header and library come from the same source.** Prefer the vcpkg-built `mdnsresponder` pair (matching
   header + `dnssd.lib`). The system `dnssd.dll` is a **2011 Bonjour 3.x** build while the vendored header is
   mDNSResponder **1557.x** — different eras. `DNSServiceRegister` is stable across that gap, but a header↔DLL
   mismatch is precisely how TXT/flags bugs go silent (R15). If dynamic-loading the system DLL is chosen
   anyway, the chosen header version must be recorded **and** gate 1.2 must prove a real registration
   (`dns-sd -B _airplay._tcp`) — not merely a successful `LoadLibrary`.
3. **The hand-rolled responder is a daemon-less-host fallback only** — never a way to dodge a library that is
   inconvenient. Where a daemon exists it is **forbidden**; where none exists, it **must** be the owner of 5353.

### 0.8 CI run #1 — RED. Root cause from the raw log, and the exact fix
Read by @prd-maker with `gh run view 35007094126 --log-failed` (raw log, not a summary). Run URL:
https://github.com/gh05t1733/airplay-receiver/actions/runs/35007094126 · `headSha 5d0b57d51fe2df7870127e30cad7d9c1d04e61cb` · workflow `ci` · 11m53s · conclusion **failure**.

| Step | Result |
|---|---|
| `Set up job`, `actions/checkout@v4`, `ilammy/msvc-dev-cmd@v1` | ✅ |
| vcpkg install — the Sprint 1 default set (`openssl` + `spdlog` + `catch2`) | ✅ **succeeded in 11 min** on a cold cache; FFmpeg was never built, so the feature gating worked exactly as intended |
| **`cmake … -G "Visual Studio 17 2022"`** | ❌ **exit 1** — `Generator "Visual Studio 17 2022" could not find any instance of Visual Studio.` |
| build / ctest / upload-artifact | ⏭️ never ran |

**What this proves, so nobody has to guess again:**
1. **The dependency half of gate 1.1 is already satisfied** — vcpkg resolved and built the entire Sprint 1
   default dependency set on the runner, cold, in 11 minutes.
2. **The failure is toolchain discovery, not code.** Nothing has been compiled yet, so no source-level defect
   is implicated. The `LNK2019`/`ctest`/Catch2 concerns raised earlier are still untested, not disproven.
3. **`windows-latest` + CMake's VS generator = unavailable.** Treat any future claim to the contrary as
   requiring a raw log.

**The fix — `.github/workflows/ci.yml`, and only these lines:**

| Line | Change |
|---|---|
| 20 | `-G "Visual Studio 17 2022" -A x64` → **`-G Ninja`**, and add **`-DCMAKE_BUILD_TYPE=Release`** |
| 22–25 | Replace the multi-config comment: Ninja is single-config, so `CMAKE_BUILD_TYPE` is required and meaningful; vcvars is supplied by `ilammy/msvc-dev-cmd@v1` (already a step) |
| 27 | `cmake --build build --config Release --parallel` → `cmake --build build --parallel` |
| 28 | `ctest --test-dir build -C Release --output-on-failure` → `ctest --test-dir build --output-on-failure` |
| 30 | **`path: build/Release/*.exe` → `build/*.exe`** — ⚠️ under a single-config generator the exe does **not** land in `Release/`. Leaving this line alone turns a green build into a **silently missing artifact**, and gate 1.1 requires the artifact to be uploaded |

**Gates 1.1 and 1.10: NOT MET.** A red run is red. The "push + trigger" half of 1.10 is satisfied; the
"clean-checkout CI green **with artifact**" half is not. Re-run after the 5-line change and quote the new URL.

---

## 1. Problem

There is no free, stable, HD-capable AirPlay **receiver** for Windows that a small team can own and extend.
The two available classes both fail:

- **Free tools (AirParsec-class)** are laggy and unstable — dropped frames, A/V drift, sessions that die.
- **Paid tools (AirServer-class)** are expensive, closed, and unmodifiable — no way to fix latency, no way
  to add a diagnostic surface, no way to audit what the binary does on the network.

A macOS user who wants to mirror a MacBook screen onto a Windows PC today must choose between "cheap and
unreliable" and "reliable and unownable". The Arakatian team needs this capability in-house, under a licence
that permits embedding into future tools.

**The problem statement in one line:** *build the receiver ourselves, so we control the latency, the
diagnostics, and the licence — and so the result is useful to everyone else with the same need.*

## 2. Users & Context

| | |
|---|---|
| **Primary user** | A person mirroring a MacBook screen to a Windows 10/11 PC (presentations, second screen, shared viewing). Non-technical. Expects **install → it appears in Control Center → connect**. |
| **Secondary user** | The Arakatian engineering team itself (1 lead + 1 engine dev + 1 UI/QA) — wants a codebase where a module can be replaced behind an interface without touching the pipeline. |
| **Tertiary user** | Open-source contributors — need a repo that **builds green on CI from a clean checkout**, a licence that permits reuse, and no GPL contamination. |
| **Context of use** | Home/office LAN over infrastructure Wi-Fi. No AWDL peer-to-peer, no multi-room, no Apple hardware. The PC is a plain Windows box — possibly no NVIDIA GPU at all. |
| **Explicitly not our user (v1)** | Anyone wanting multi-room audio, HomeKit pairing, HLS/YouTube relay, or 4K. |

## 3. Goals & Success Metrics (Sprint 1)

Sprint 1 is a **foundation** sprint. Its goal is that the project is *real*: it builds in CI, it advertises
itself correctly on the network, it answers the sender's first control exchange correctly, and it has a
window with a measurable frame loop and a clean shutdown. **Sprint 1 does not produce a picture** (§0.2.2).

| # | Goal | Metric (the gate) |
|---|---|---|
| G1 | The project builds reproducibly anywhere | clean-checkout `ci.yml` run green on `windows-latest`, artifact uploaded (**gate 1.1**) |
| G2 | A MacBook discovers the PC as an AirPlay target | `dns-sd` browse+resolve from the Mac **and** a `python-zeroconf` browse on Windows both see `_airplay._tcp` + `_raop._tcp` with the §1.2 TXT keys (**1.2**); PC visible in Control Center (**1.3**) |
| G3 | The control plane answers correctly | `GET /info` → 200 with a valid bplist carrying `name`, `deviceID`, `macAddress`, `model`, `sourceVersion`, `features`, `statusFlags` (**1.4**) |
| G4 | Failure is graceful, never a crash | `/pair-setup`, `/pair-verify`, `/fp-setup`, and malformed requests return documented errors; process survives all four (**1.5**) |
| G5 | There is a real output surface | Win32+D3D11 window presents a solid colour + FPS/status overlay at ≥60 Hz; ESC/Alt+F4 quits cleanly (**1.6**) |
| G6 | The app is observable and configurable | `config.json` under `%APPDATA%`, rotating log under `%LOCALAPPDATA%`, log level respected (**1.7**) |
| G7 | Misconfiguration is diagnosable | bind failure (port in use / no interface) → actionable log line + non-zero exit code (**1.8**) |
| G8 | No leaks, no hang on exit | §4.4 shutdown order implemented; 10 connect/disconnect cycles → zero leaked handles; 3 s watchdog (**1.9**) |

**Anti-goal (measured, not aspirational):** no claim of "we receive AirPlay video", "mDNS browsing", or
"WPF UI" may appear anywhere in Sprint 1 output (brief Appendix A).

## 4. Scope

### 4.1 MUST (Sprint 1)
1. Toolchain bootstrap as **day-1 blocking tasks**: CMake, vcpkg (clone+bootstrap), Ninja, FFmpeg **dev** via the vcpkg manifest.
2. Repo baseline: first commit, remote, `.gitignore` audit, `CMakeLists.txt` + `CMakePresets.json` + `vcpkg.json` + `vcpkg-configuration.json`.
3. CI: `ci.yml` (correct generator, per §0.1) + `license-scan.yml` (GPL/AGPL gate).
4. `arak_common`: `CoreStatus`, `FrameRef`, `ILogger`, `IConfig`, ring buffer, JSON reader.
5. `arak_rtsp`: listener, RTSP/1.0 request parser, router, CSeq/session state, `GET /info`, documented error path.
6. `Bplist` codec: reader + writer for dict/array/string/int/bool/real/data/date/UID, round-trip tested.
7. `arak_discovery`: `IAdvertiser` over mDNSResponder, TXT/SRV builders for both services, interface selection + address logging.
8. `ReceiverIdentity`: deviceid/model/srcvers/features/flags + **persisted Ed25519 identity generated in Sprint 1** (§8.2).
9. `ui-win32`: window, D3D11 device, flip-model swap chain, present loop ≥60 Hz, status/FPS overlay, clean quit.
10. `arak_core` app wiring: thread inventory T1–T11, startup order, §4.4 shutdown order, 3 s watchdog, non-zero exit on bind failure.
11. Config + logging wired to the §5.8-adjacent paths (`config.json`, rotating logs).

### 4.2 WON'T (Sprint 1) — but MUST be visible as planned
- **Pairing / FairPlay** (`pair-setup`, `pair-verify`, `fp-setup`), key derivation, decryption → Sprint 2. Sprint 1 returns documented errors.
- **RTP receipt, jitter buffer, decoding, video rendering of a real frame** → Sprint 2. A frame is a **Sprint 2 gate**.
- **`CoreBridge` named-pipe client** → interface + schema exist; no UI client in Sprint 1.
- **WPF/Tauri client** → Sprint 3+, and only if the .NET SDK decision is funded (no SDK on this box).
- **Tray + hotkeys, settings UI** → Sprint 3.
- **Installer (Inno Setup), firewall rules, release job** → Sprint 4 — plus a new item: the **Bonjour runtime
  dependency for end users** (detect + instruct, or bundle **after** checking Apple's redistribution terms;
  gate 4.3 / C19). Whether to bundle is an installer decision, not a detail.
- **4K, multi-room, PTP timing, buffered audio, HomeKit, HLS relay** → out of v1.0.

### 4.3 Non-Goals
- Not a general RTSP/RTP client library. We implement the **AirPlay receiver subset** only.
- Not a media player. No file playback, no transcoding, no recording.
- Not a multi-user/multi-session service. `maxSessions = 1` (brief §5.3).
- Not a browsing/diagnostics scanner. The PC **advertises**; it does not browse (brief §1.1).
- Not a cross-platform product. Windows 10/11 x64 is the only target for v1.

### 4.4 Timeline framing (advisory 4: rebaseline)
The 8-week calendar is **not** the contract — the **gate sequence** is. R11 (team of 1–3 vs 8 weeks) is
rated High/High in the brief's own register, so date-driven planning would hide the real risk. Every task
below is stated as *prerequisite → task → measured done-criterion*, and each sprint ends with a demoable
artifact. If a gate slips, the schedule moves; the gate does not.

## 5. Assumptions & Risks

### 5.1 Assumptions
- **A1** MSVC 14.51.36231 + Windows SDK 10.0.26100.0 are present and sufficient (verified by @deployer; brief §0).
- **A2** vcpkg port `mdnsresponder` (1557.140.5.0.1, Apache-2.0) works on `windows & !uwp`. It was last updated 2023 and depends on `vcpkg-msbuild`; the brief's hand-rolled responder remains the stated plan B and is **not** discarded.
- **A3** The Mac used for gate 1.3 is available on the same L2 network as the PC, with no client isolation.
- **A4** The installed gyan FFmpeg (bin-only) is never linked. FFmpeg arrives from vcpkg (or the BtbN fallback for **local dev only**).
- **A5** Network access to GitHub works on this box (needed for vcpkg clone and the vcpkg port tree).

### 5.2 Risks carried from the brief that shape Sprint 1
| ID | Risk | Sprint 1 consequence |
|---|---|---|
| R9 | Toolchain absent → sprint dies day 1 | Task 0 makes all four installs explicit blocking tasks with verification commands |
| R10 | Licence contamination | `license-scan.yml` in Task 3; `docs/architecture/protocol-notes.md` required as the provenance record |
| R4 | mDNS on multi-homed Windows (VPN/Hyper-V vSwitch) | interface selection + advertised-address logging are in-scope; plan B kept |
| R3 | Firewall blocks SRV port / 5353 / RTP window | Sprint 1 **logs** the bind/listen result and documents the required rules (installer work is Sprint 4) |
| R11 | Small team vs scope | gate-sequence framing (§4.4), no calendar contract |

### 5.3 Risk we are adding to the register
| ID | Risk | Impact | Mitigation |
|---|---|---|---|
| **R14** | Advertising `pk`/`pi` before pairing exists makes the sender attempt a pairing we cannot complete, producing a misleading failure that looks like a bug | Medium | Identity is generated + persisted in Sprint 1, but **`advertise_pk` defaults OFF**; flipping it on is a Sprint 2 task coupled to `IPairingManager` (§8.2) |
| **R15** | **Header↔DLL era mismatch:** the system `dnssd.dll` is a 2011 Bonjour 3.x build while the vendored `dns_sd.h` is mDNSResponder 1557.x. The API is stable, but TXT/flags bugs across that seam go **silent** | Medium | Header and library from the same source (prefer the vcpkg pair). If the system DLL is dynamic-loaded anyway, record the header version **and** require a real registration at gate 1.2 (§0.7) |
| **R16** | Our process binding UDP 5353 while the resident Bonjour daemon owns it → flaky or failed advertisement (gate 1.3) and a fight with a system service | **High** | Advertise via the daemon client API; never bind 5353 while a daemon holds it (§0.7 contract 1). The hand-rolled responder is permitted **only** on a daemon-less host |

### 5.4 Open decisions (user)
See §11 — four items, two of which are already recommendations from @architect/@reviewer.

## 6. Core Features (summary)

| Feature | Why it exists | Gate |
|---|---|---|
| F0 Toolchain + CI foundation | Nothing else can be verified without a green build | 1.1 |
| F1 Discovery / mDNS advertise | The Mac cannot see the PC otherwise — this is the product's front door | 1.2, 1.3 |
| F2 Receiver identity | TXT/SRV values must be correct and stable or the sender rejects/forgets us | 1.2, 1.3 |
| F3 Control plane (RTSP + bplist + `/info`) | This is the first real protocol exchange; it proves our understanding is right | 1.4, 1.5 |
| F4 Window + present loop | Proves the output path and gives the user something visible | 1.6 |
| F5 Config + logging + stats | Makes every later sprint measurable instead of anecdotal | 1.7, 1.8 |
| F6 Lifecycle / shutdown ordering | The difference between a shippable app and a crash-on-exit demo | 1.9 |

---

## 7. Feature Tree (Sprint 1)

```
AIRPLAY HD RECEIVER (Windows) — Sprint 1: Foundation
│
├─ FASE 0: Toolchain & CI Foundation  (blocker hari-1)  → gate 1.1
│  ├─ F0.1: Toolchain lokal
│  │  ├─ SUB 0.1.1: Install CMake            (winget install --id Kitware.CMake -e)
│  │  ├─ SUB 0.1.2: Clone + bootstrap vcpkg  (C:\vcpkg, bootstrap-vcpkg.bat)
│  │  ├─ SUB 0.1.3: Install Ninja            (generator fallback + CI parity)
│  │  └─ SUB 0.1.4: FFmpeg DEV via vcpkg manifest — never the bin-only gyan build
│  └─ F0.2: Repo & CI pertama
│     ├─ SUB 0.2.1: First commit + remote + .gitignore audit
│     ├─ SUB 0.2.2: ci.yml — GENERATOR BENAR (§0.1: VS17 2022 / Ninja, bukan VS18)
│     └─ SUB 0.2.3: license-scan.yml — gate GPL/AGPL (§1.5 brief)
│
├─ FASE 1: Discovery & Identity  → gate 1.2, 1.3
│  ├─ F1.1: mDNS Advertiser (C1)
│  │  ├─ SUB 1.1.1: IAdvertiser + mDNSResponder wrapper (start/update/stop)
│  │  ├─ SUB 1.1.2: TXT/SRV builder — _airplay._tcp  (features/flags/model/srcvers/deviceid)
│  │  ├─ SUB 1.1.3: TXT/SRV builder — _raop._tcp     (instance "<MAC-UPPER>@<Name>", txtvers/ch/cn/et/…)
│  │  └─ SUB 1.1.4: Interface picker + advertised-address logging (R4 mitigation)
│  └─ F1.2: Receiver Identity (C1 data)
│     ├─ SUB 1.2.1: deviceid (MAC), name, model, sourceVersion, features, flags
│     └─ SUB 1.2.2: Ed25519 identity (pk/pi/psi) digenerate + dipersist di Sprint 1 (§8.2)
│
├─ FASE 2: Control Plane  → gate 1.4, 1.5, 1.8
│  ├─ F2.1: RTSP Server (C2)
│  │  ├─ SUB 2.1.1: TCP listener + RTSP/1.0 request parser (request line, headers, bplist body)
│  │  ├─ SUB 2.1.2: Request router + CSeq + per-session state machine
│  │  └─ SUB 2.1.3: Bind failure → log actionable + exit non-zero (gate 1.8)
│  ├─ F2.2: Bplist Codec (C3)
│  │  └─ SUB 2.2.1: Reader + writer (dict/array/string/int/bool/real/data/date/UID) — round-trip tested
│  └─ F2.3: /info + clean refusal
│     ├─ SUB 2.3.1: GET /info → 200 bplist lengkap (name/deviceID/macAddress/model/sourceVersion/features/statusFlags)
│     └─ SUB 2.3.2: /pair-setup,/pair-verify,/fp-setup, malformed → error terdokumentasi, TIDAK crash
│
├─ FASE 3: Window, Runtime & Lifecycle  → gate 1.6, 1.7, 1.9
│  ├─ F3.1: Win32 + D3D11 Window (C16)
│  │  ├─ SUB 3.1.1: Window + D3D11 device + DXGI flip-model swap chain
│  │  ├─ SUB 3.1.2: Present loop — solid colour ≥60 Hz (RenderStats)
│  │  ├─ SUB 3.1.3: Status/FPS overlay (thread T9 pump terpisah dari T7 present)
│  │  └─ SUB 3.1.4: ESC / Alt+F4 → quit bersih
│  ├─ F3.2: Config, Logging & Stats (C13, C14, C18)
│  │  ├─ SUB 3.2.1: config.json di %APPDATA%\Arakatian\AirPlayReceiver\ + hot-reload
│  │  ├─ SUB 3.2.2: spdlog rotating di %LOCALAPPDATA%\...\logs\ + level filtering
│  │  └─ SUB 3.2.3: StatsCollector 1 Hz (fps, presented, dropped, cpu, ram) → log
│  └─ F3.3: Lifecycle & Shutdown (C…/main)
│     ├─ SUB 3.3.1: Thread inventory T1–T11 + startup order
│     ├─ SUB 3.3.2: Shutdown order §4.4 + 3 s watchdog → ExitProcess(2)
│     └─ SUB 3.3.3: 10× connect/disconnect tanpa leak (handle count + Debug CRT check)
│
└─ DIRENCANAKAN — TIDAK dibangun di Sprint 1
   ├─ Sprint 2: pairing/FairPlay + FRAME PERTAMA (gerbang produk yang sesungguhnya)
   ├─ Sprint 3: audio WASAPI + A/V sync <50ms + UI settings/tray (WPF opsional, butuh .NET SDK)
   ├─ Sprint 4: installer Inno Setup + firewall rules + soak + v1.0.0
   └─ Post-1.0: 4K@30, multi-room, PTP, HLS relay
```

---

## 8. Feature Specs (Sprint 1)

### 8.1 F1.1 — mDNS Advertiser + TXT/SRV (gate 1.2, 1.3)

**User story.** As a MacBook user, I open Control Center → Screen Mirroring and I see "Arakatian PC"
in the list, so I can pick it without knowing an IP address.

**Input/output contract.**
```cpp
struct ReceiverIdentity {                 // single source of truth for all advertised values
  std::string deviceId;                   // "aa:bb:cc:dd:ee:ff"  (from the chosen adapter's MAC)
  std::string name;                       // display name, e.g. "Arakatian PC"
  std::string model;                      // "AppleTV3,2"
  std::string sourceVersion;              // "220.68"
  uint64_t    features;                   // 64-bit mask
  uint64_t    flags;                      // status flags
  std::string publicKeyHex;               // 64 hex chars; advertised only when advertise_pk=true
  std::string pairingId, systemPairingId; // UUID strings (§8.2)
  uint16_t    rtspPort;                   // actual bound port — the SRV target, never hardcoded by the sender
};
class IAdvertiser {
  CoreStatus start(const ReceiverIdentity&, std::string_view ifaceHint);
  CoreStatus update(const ReceiverIdentity&);   // re-register TXT on config change
  CoreStatus stop() noexcept;                   // must deregister cleanly
  std::vector<std::string> localAddresses() const;
};
```
**Encoding rule (computed, not remembered).** `features` is emitted as `"0x%X,0x%X"` = `0x<lower32>,0x<upper32>`:

| Profile | Bits set | Emitted string |
|---|---|---|
| Sprint 1 default (mirroring + audio + RAOP + screen multi-codec) | 7, 9, 30, 42 | `0x40000280,0x400` |
| If the pairing decision (§11.4) settles on the legacy/PIN profile (bit 27) | 7, 9, 27, 30, 42 | `0x48000280,0x400` |
| **If §11.4 lands on TRANSIENT (recommended)** | 7, 9, 30, 42, **48** | **`0x40000280,0x10400`** |

`flags = 0x4`. `_raop._tcp` instance name **must** be `<MAC-UPPERCASE>@<Display Name>` (e.g. `AABBCCDDEEFF@Arakatian PC`).

**Given/When/Then.**
- **G** the app is running and advertising; **W** a `dns-sd -B _airplay._tcp` on the Mac runs; **T** the PC's instance appears, and `dns-sd -L` resolves SRV port + the TXT keys of brief §1.2.
- **G** the same state; **W** a `python-zeroconf` browse runs on Windows; **T** both services are seen with matching TXT values (two independent observers, not one).
- **G** config changes the display name; **W** `update()` is called; **T** the new name is resolvable within 5 s and the old instance is gone.
- **G** the app is shutting down; **W** `stop()` runs; **T** a re-browse finds nothing (clean deregistration, no ghost entry).
- **Edge** the host has a VPN/Hyper-V vSwitch; **W** the interface hint selects the LAN adapter; **T** the log lists exactly which addresses were advertised, and the Mac sees only the LAN one.
- **G** the app is advertising on a box where Bonjour already runs; **W** `netstat -ano` is filtered for `:5353`; **T** **our PID is not among the owners** — we registered through the daemon instead (§0.7; this is gate 1.3 evidence).

**Dependencies.** Task 0 (vcpkg), F1.2 identity.
**Implementation note — superseded by §0.7 (Rev 3).** `IAdvertiser` registers **through the Bonjour daemon's
client API**; the process must never bind 5353 while a daemon holds it. The hand-rolled RFC 6762/6763 responder
is the fallback **only** on a host with no daemon, and there it owns 5353. Wrap whichever implementation is used
behind `IAdvertiser` so the swap touches nothing else — that abstraction is what keeps R4 survivable.
`update()` is the only path that mutates registration; no ad-hoc re-registration from config code.
Verification uses the **system** `C:\Windows\System32\dns-sd.exe` (§0.7).

### 8.2 F1.2 — Receiver identity, incl. Ed25519 generated in Sprint 1 (advisory 2)

**User story.** As the receiver, I have a **stable** identity from the first run, so a Mac that has seen me
does not have to rediscover me when pairing lands in Sprint 2 — and so our discovery test never has to be re-run.

**Contract.**
- On first run, generate an **Ed25519 keypair**; store it under `%LOCALAPPDATA%\Arakatian\AirPlayReceiver\identity\` (private key never logged, never advertised).
- Derive and persist: `publicKeyHex` (32-byte public key, lowercase hex), `pairingId` (stable UUID), `systemPairingId` (stable UUID).
- `deviceId` is derived from the selected adapter's MAC.
- **`advertise_pk` is a config flag, default `false` in Sprint 1** (see R14). When `true`, `pk`/`pi`/`psi` are added to the `_airplay._tcp` TXT.

**Given/When/Then.**
- **G** a fresh install; **W** the app starts twice; **T** `publicKeyHex`, `pairingId`, `systemPairingId` are **identical** across both runs (persisted, not regenerated) and the private key file is not world-readable.
- **G** `advertise_pk=false` (default); **W** the TXT is inspected; **T** `pk` is absent and the advert matches Sprint 1 capability (we cannot yet pair — an advert that promises pairing we cannot perform is a lie, R14).
- **G** `advertise_pk=true`; **W** the TXT is inspected; **T** `pk`/`pi`/`psi` are present and well-formed.

**Dependencies.** OpenSSL 3.x (vcpkg), F1.1.
**Do NOT build here.** No `pair-setup`/`pair-verify`/SRP/Curve25519 handshake logic — that is Sprint 2 behind `IPairingManager`.

### 8.3 F2.1 — RTSP listener + parser + bind failure (gate 1.4, 1.8)

**User story.** As a Mac sender, my RTSP request reaches the receiver and gets a correct, well-formed answer —
and if the receiver could not start, I find out from a clear log instead of a silent no-show.

**Contract.** TCP listener bound to the SRV port. Parser handles: request line (`METHOD SP URI SP RTSP/1.0`),
headers (case-insensitive names, `CSeq` mandatory, `Content-Length`, `Content-Type: application/x-apple-binary-plist`),
and a body up to a bounded size (reject oversize with a documented error).
Routes in Sprint 1: `GET /info` (200), `POST /pair-setup|/pair-verify|/fp-setup` (documented error), anything
else → documented error. `maxSessions = 1`, `keepAliveTimeoutMs = 15000`.

**Given/When/Then.**
- **G** the app is listening; **W** a canned `GET /info` RTSP/1.0 request with `CSeq: 1` is posted; **T** the response is `200 OK` with `CSeq: 1` echoed and a `Content-Type: application/x-apple-binary-plist` body that `Bplist` decodes.
- **G** the same state; **W** requests missing `CSeq`, with a malformed request line, or with a body over the size cap are posted; **T** each gets a documented 4xx/5xx and the **process survives**.
- **G** the port is already in use; **W** the app starts; **T** the log names the port and the OS error, and the exit code is non-zero.
- **G** no usable interface; **W** the app starts; **T** the log names the missing interface and the exit code is non-zero.

**Dependencies.** F2.2 codec, Task 0.
**Do NOT build here.** No `SETUP`/`RECORD`/`TEARDOWN` stream handling, no ports negotiation, no `StreamSetup` plumbing to RTP — Sprint 2. The router returns "not implemented in this build" and that string is part of the documented contract.

### 8.4 F2.2 — Bplist codec (gate 1.4)

**User story.** As the receiver, I can read the sender's `qualifier:txtAirPlay` body and emit a response body
the sender accepts, so `/info` is a real protocol exchange and not a stub.

**Contract.** Reader + writer for: dict, array, string (ASCII + UTF-16BE), int (1/2/4/8 bytes), bool, real,
data, date, UID. Trailing bytes after the top-level object are an error. All lengths bounds-checked
(a malformed length must never allocate unbounded memory).

**Given/When/Then.**
- **G** a fixture binary plist; **W** it is parsed and re-serialised; **T** the round-trip is byte-identical for canonical fixtures and semantically identical for all others.
- **G** a truncated/oversize-length fixture; **W** it is parsed; **T** a `ProtocolError` is returned, no crash, no unbounded allocation.
- **G** a UTF-16BE string fixture; **W** it is parsed; **T** the string is correct.

**Dependencies.** none (pure code) — this is the first testable module and can be written before any install completes.
**Fixtures.** Sanitised, checked into `tests/fixtures/` as binary + hex; provenance recorded in `docs/architecture/protocol-notes.md`. **No GPL-derived file may be copied** (brief §1.5).

### 8.5 F3.1 — Win32 + D3D11 window (gate 1.6)

**User story.** As the user, when I launch the app I see a window that proves the render path works and tells me
what the app is doing (status + FPS) — so I trust it before any video exists.

**Contract.** `IVideoRenderer::create(hwnd, VideoFormat)` / `present(FrameRef, presentTimeNs)` / `stats()` per brief §5.5.
Sprint 1 presents a solid colour (no video). `RenderStats{fps, presented, dropped, presentLatencyUs}` is the evidence.
Window procedure lives on thread T9; **only T7 calls `Present`**.

**Given/When/Then.**
- **G** the app is running; **W** `stats()` is sampled for 5 s; **T** `fps ≥ 60` on a ≥60 Hz display and `presented` grows monotonically.
- **G** the window has focus; **W** ESC is pressed; **T** the app exits through the §4.4 shutdown sequence with exit code 0.
- **G** fullscreen is toggled; **W** `setFullscreen(true)`; **T** the swap chain resizes without a device-removed error and the present loop never stalls > 100 ms.
- **Edge** the device is removed (GPU reset / RDP session); **W** `Present` returns `DXGI_ERROR_DEVICE_REMOVED`; **T** the app logs it and exits non-zero rather than spinning.

**Dependencies.** Task 2 (CMake target graph).
**Do NOT build here.** No NV12 shader, no video texture path, no frame pacing against the audio clock — that is Sprint 2 with real frames.

### 8.6 F3.2 / F3.3 — Config, logging, lifecycle (gate 1.7, 1.8, 1.9)

**Contract.**
- `config.json` at `%APPDATA%\Arakatian\AirPlayReceiver\config.json`: `deviceName`, `ifaceHint`, `rtspPort`, `advertise_pk`, `logLevel`, `uiScale`. Missing keys fall back to defaults; unknown keys are ignored, not fatal.
- Rotating logs at `%LOCALAPPDATA%\Arakatian\AirPlayReceiver\logs\`, level-filtered.
- §4.4 shutdown order, stage-by-stage, with a 3 s watchdog → log the hung stage + `ExitProcess(2)`.

**Given/When/Then.**
- **G** a config with an unknown key and a missing key; **W** the app loads it; **T** the app starts with documented defaults and the JSON is not rewritten destructively unless a `set()` happened.
- **G** `logLevel=debug`; **W** the app runs; **T** the log contains debug lines; with `logLevel=warn`, it does not.
- **G** a running app; **W** 10 connect/disconnect cycles are driven against the RTSP port; **T** the handle count after equals the count before (±0 leaked handles) and Debug-CRT reports no leak.
- **G** a deliberately hung stage (test hook); **W** shutdown runs; **T** the watchdog fires at 3 s, logs the stage name, and exits 2.

**Dependencies.** `arak_common`, F2.1.

---

## 9. Task List (ordered; each task starts only after its predecessor's done-criterion is met)

> **Rules for @code-executor.** Each task names exact paths, states the imperative, and gives a **measured**
> done-criterion. "It builds" means CI green. Do not start Task N+1 while Task N's criterion is unmet.
> **Do NOT add anything from §4.2** — a P2/deferred feature appearing in Sprint 1 is an automatic REQUEST CHANGES.

### Task 0 — Toolchain bootstrap (**blocker, day 1**) — gates 1.1, 1.10 · 🔶 **PARTIALLY DONE**
Prereq: none. Steps 1–3 are **already executed and verified** (§0.5) — do not redo them, and do not re-install.
1. ~~CMake~~ ✅ done — `cmake version 4.4.3`.
2. ~~vcpkg~~ ✅ done at **`C:/dev/vcpkg`** (correction: **not** `C:/vcpkg`).
3. ~~Ninja~~ ✅ done — `1.13.2`.
4. **Dependencies — CORRECTED (Rev 3).** Sprint 1's **default** set is only `openssl + spdlog + catch2`. FFmpeg sits behind the **opt-in feature `video`** and mDNSResponder behind **`mdns`** (both default OFF), with `AIRPLAY_ENABLE_FFMPEG=OFF`. Run `C:/dev/vcpkg/vcpkg.exe install --triplet x64-windows --clean-after-build`. Do **not** install a binary-only FFmpeg ever; when the `video` feature is actually used it comes from the manifest or the BtbN **lgpl**-shared fallback.
5. Local generator string is **already confirmed by probe** (`cmake --help`): `Visual Studio 18 2026` is the local default; the runner has VS 17 2022 only. No guessing required.

**DONE:** `cmake --version` / `vcpkg version` / `ninja --version` exit 0 (✅ already true); then the vcpkg install completes and `vcpkg list` shows `openssl` + `spdlog` + `catch2` — the Sprint 1 default set. FFmpeg/mdnsresponder appear only when their features are enabled.
**DISK GUARD (mandatory — C: had ~38 GB free):** keep `VCPKG_BINARY_SOURCES=clear;files,%LOCALAPPDATA%\vcpkg\archives,readwrite` set and build with `--clean-after-build`. A source build of `ffmpeg[nvcodec]` + `openssl` + `mdnsresponder` can otherwise eat 8–15 GB of buildtrees.
**PLAN B if vcpkg FFmpeg-dev is too slow or too heavy:** `BtbN ffmpeg-master-latest-win64-**lgpl**-shared` into `third_party/ffmpeg/` (gitignored, fetched by `scripts/`), wired via `-DAIRPLAY_FFMPEG_ROOT=`. Use **lgpl, not gpl** — a gpl build cannot be redistributed in our Apache-2.0 installer.
**DO NOT:** try to link the installed gyan FFmpeg; commit `build/`, `dist/`, `third_party/ffmpeg/`, or `vcpkg_installed/`.

### Task 1 — Repo baseline + first commit (advisory 4 / gates 1.1, **1.10**)
Prereq: Task 0.
1. Audit `.gitignore`: must cover `build/`, `dist/`, `third_party/ffmpeg/`, `*.user`, `vcpkg_installed/`.
   **⚠️ NEVER use a blanket `third_party/` — measured with git by @ui-designer and it breaks a fresh clone.**
   `third_party/mDNSResponder/include/dns_sd.h` is **vendored and MUST be committed** (§0.7), while
   `third_party/ffmpeg/` is fetched and **MUST be ignored**. A blanket `third_party/` silently drops the
   vendored header, and the obvious rescue `!third_party/README.md` **does not work** — git cannot re-include
   anything while the *parent directory* is excluded. Net effect: a clean checkout that cannot configure,
   i.e. gate 1.1 failing for a reason nobody would think to look for. **Keep the scoped `third_party/ffmpeg/`
   form** and leave a comment in `.gitignore` saying why, so this is not "fixed" a fourth time.
2. Create `LICENSE` (Apache-2.0) and `NOTICE` (third-party licences + provenance, listing vcpkg deps and the "reference-only, no GPL code" rule).
3. First commit of the docs + skeleton; add the remote.
**DONE:** `git log --oneline` shows ≥1 commit; `git status` clean; `git ls-files` contains `docs/architecture/ARCHITECTURE-BRIEF.md` and `docs/prd/PRD-AIRPLAY-SPRINT1.md`; a remote is configured and pushed — **this is gate 1.10**, and gate 1.1 is unevaluable without it.
**DO NOT:** commit anything from `third_party/`, `build/`, or any generated artifact.

### Task 2 — CMake skeleton + vcpkg manifest + presets (gate 1.1)
Prereq: Task 1.
1. `CMakeLists.txt`: C++20, `/W4 /permissive-`, options `BUILD_TESTS` (default ON), `BUILD_WPF_UI` (default OFF).
2. `vcpkg.json`: manifest per Task 0.4. `vcpkg-configuration.json`: baseline pin.
3. `CMakePresets.json`: `msvc-debug`, `msvc-release` (local VS18 string from Task 0.5), `ci` (**VS17 2022 / Ninja** per §0.1).
4. `src/common/`, `src/core/…`, `src/ui/win32/` directories with a trivial `arak_common` static target and a `main()` that logs one line and exits 0.
5. `scripts/bootstrap-deps.ps1` + `scripts/build.ps1` + `scripts/run.ps1` implementing Task 0 + build + run.
**DONE:** `cmake --preset msvc-release` then `cmake --build --preset msvc-release` succeed locally; the exe runs and exits 0.
**DO NOT:** create the WPF client project; wire FFmpeg or D3D11 yet (they are not needed for a green skeleton).

### Task 3 — CI + licence gate (gate 1.1, blocker §0.1)
Prereq: Task 2.
1. `.github/workflows/ci.yml`: `windows-latest`, vcpkg binary cache, `ilammy/msvc-dev-cmd@v1`, use the **runner's preinstalled vcpkg** (`$env:VCPKG_INSTALLATION_ROOT`) instead of cloning a second copy, configure with **`-G "Visual Studio 17 2022"`** — or `-G Ninja` + `-DCMAKE_BUILD_TYPE=Release` — **never** `"Visual Studio 18 2026"` — then build, `ctest`, upload artifact. **On the VS generator do NOT pass `-DCMAKE_BUILD_TYPE`** (§0.1): it is a no-op there and its presence implies a setting that never applied.
2. `.github/workflows/license-scan.yml`: fail if a GPL/AGPL header appears under `src/` or `third_party/`.
3. `tests/unit/` with one passing Catch2 test registered in `ctest` (so `ctest` is not a no-op).
**DONE:** a clean-checkout CI run is green and the artifact is uploaded. This is gate 1.1 — paste the run URL into your handoff.
**DO NOT:** hardcode a generator you have not seen on the runner; do not add a release/installer job (Sprint 4).

### Task 4 — `arak_common` (gates 1.4, 1.7, 1.9)
Prereq: Task 3.
1. `src/common/include/`: `CoreStatus`, `FrameRef`, `ILogger` (spdlog impl), `IConfig` (JSON, hot-reload hook), a bounded SPSC ring buffer, small JSON helpers.
2. Unit tests: status→string mapping, config defaults/unknown-key tolerance, ring-buffer wrap + overflow policy counters.
**DONE:** `ctest` green; config test proves defaults + non-destructive load.
**DO NOT:** add a second logging framework; do not invent a config schema library.

### Task 5 — Bplist codec (gate 1.4)
Prereq: Task 4.
1. `src/core/rtsp/` or `src/common/` per brief §6.1: reader + writer for dict/array/string/int/bool/real/data/date/UID.
2. Bounds-check every length; reject trailing bytes; reject oversize bodies.
3. `tests/fixtures/` canonical plists (binary + hex) + `tests/unit/test_bplist.cpp` round-trip + malformed cases.
**DONE:** round-trip tests green including UTF-16BE and a truncated-input case that returns `ProtocolError` without crashing.
**DO NOT:** vendor any GPL plist implementation; record fixture provenance in `docs/architecture/protocol-notes.md`.

### Task 6 — RTSP server + `/info` + documented refusals (gate 1.4, 1.5, 1.8)
Prereq: Task 5.
1. Listener + parser + router + CSeq handling + session state (`SessionState` per brief §5.3); `maxSessions=1`.
2. `GET /info` → 200, bplist body with `name, deviceID, macAddress, model, sourceVersion, features, statusFlags`.
3. `/pair-setup`, `/pair-verify`, `/fp-setup`, unknown method, malformed request → documented error responses.
4. Bind-failure path → actionable log + non-zero exit.
5. `tests/integration/`: loopback server driven by a script posting all of the above; assert the process survives and exit codes are correct.
**DONE:** integration tests green; a canned `/info` reply decodes with the Task 5 codec; the four unimplemented routes return documented errors and the process is still alive afterwards.
**DO NOT:** implement `SETUP`/`RECORD`/`TEARDOWN` stream handling, AES keys, or RTP port negotiation — Sprint 2.

### Task 7 — Advertiser + identity (gates 1.2, 1.3, **1.11**, **1.12**)
Prereq: Task 6 (so the advertised SRV port is the real bound port).
1. `src/core/discovery/`: `IAdvertiser` — register **through the Bonjour daemon client API** (`DNSServiceRegister` family). **Never bind 5353 while a daemon holds it** (§0.7). Vendor `dns_sd.h` (Apache-2.0) into `third_party/mDNSResponder/include/` and **commit it** (Task 1 rule). Prefer the **vcpkg-built header + `dnssd.lib` pair**; if you dynamic-load the system `dnssd.dll` instead, record the header version and prove a real registration at gate 1.2 (R15).
2. TXT/SRV builders for both services; `_raop` instance name `<MAC-UPPER>@<Name>`; `update()` as the only mutation path.
3. Identity: `deviceid` from the adapter MAC; Ed25519 keypair + `pairingId`/`systemPairingId` generated and persisted (§8.2); `advertise_pk` default `false`. The advertised pairing bits must match §11.4 — that is gate 1.11.
4. Interface selection + advertised-address logging.
**DONE:** (a) `dns-sd -B _airplay._tcp` from the Mac **and** a `python-zeroconf` browse on Windows both see both services with the §1.2 TXT keys — plus an equality check of TXT vs the code constants using the **system** `C:\Windows\System32\dns-sd.exe` (no vcpkg CLI build); (b) gate 1.3 screenshot + a `netstat -ano` capture proving **our PID is not a `:5353` owner**; (c) identity identical across two runs.
**DO NOT:** implement pairing handshake; do not add mDNS *browsing* (brief §1.1 says we advertise, not browse).
**Gate 1.11 is a consistency gate, not a feature gate:** the advertised pairing bits must match what Sprint 2 will actually implement — an advert that promises a capability we do not implement is a lie, and that failure mode looks like a bug to the user (R14). **Gate 1.12:** the Ed25519 identity must be byte-identical across two runs.

### Task 8 — Win32 + D3D11 window (gate 1.6)
Prereq: Task 4.
1. `src/ui/win32/`: window, D3D11 device, DXGI flip-model swap chain, solid-colour present loop, FPS/status overlay, ESC/Alt+F4 quit.
2. `IVideoRenderer` per brief §5.5; thread T9 (wndproc) separate from T7 (present, owns `Present`).
3. `RenderStats` logged once per second by T10.
**DONE:** 5-second sample shows `fps ≥ 60`; presented count monotonic; ESC and Alt+F4 both exit 0 through the shutdown order; measured numbers recorded.
**DO NOT:** add the NV12 shader/video texture path or audio-clock pacing (Sprint 2/3).

### Task 9 — App wiring + shutdown ordering (gate 1.8, 1.9)
Prereq: Tasks 6, 7, 8.
1. `src/core/app/main.cpp`: create threads T1–T11 in the brief's order; wire listener → router, advertiser → listener port, stats → logger.
2. Implement §4.4 shutdown order exactly, plus the 3 s watchdog → log stage + `ExitProcess(2)`.
3. Debug-CRT leak flags in Debug builds.
**DONE:** 10 connect/disconnect cycles → handle count unchanged; Debug run reports no leaks; a forced hung stage fires the watchdog at 3 s with the stage name logged.
**DO NOT:** add the CoreBridge named-pipe *server* beyond the interface stub if it risks the sprint; no tray/hotkeys.

### Task 10 — Verification bundle (evidence for @reviewer)
Prereq: all above.
1. Run and paste: the green clean-checkout **CI run URL** (gate 1.1) + artifact name.
2. Capture `dns-sd` output from the Mac + `python-zeroconf` output from Windows (gate 1.2) + the Control Center screenshot (gate 1.3).
3. Capture the `/info` request/response (gate 1.4) and the four documented-error responses with a liveness proof (gate 1.5).
4. Record the 5 s `RenderStats` sample (gate 1.6), the config/log file paths with content (gate 1.7), the bind-failure exit code + log line (gate 1.8), and the handle-count-before/after table for 10 cycles (gate 1.9).
5. Write `docs/pipeline/logs-03-code-executor.txt` and append your handoff row to `docs/pipeline/HANDOFFS.md`.
**DONE:** every gate 1.1–1.9 has a concrete artifact reference; anything not proven is reported as **not proven**, not as "works".

---

## 10. Gate checklist (@reviewer, Sprint 1)

- [ ] **1.1** Clean-checkout CI green on `windows-latest`, artifact uploaded — with the **VS17 2022/Ninja** generator (§0.1), not VS18.
- [ ] **1.2** `_airplay._tcp` + `_raop._tcp` advertised, TXT keys per brief §1.2, seen by **two independent observers**; verified with the **system** `dns-sd.exe` (§0.7); a real registration is proven, not a successful `LoadLibrary`.
- [ ] **1.3** MacBook sees the PC as a screen-mirroring target (screenshot + protocol-notes entry) **and** `netstat -ano` proves our PID does **not** own `:5353` (§0.7).
- [ ] **1.4** `GET /info` → 200 + valid bplist (`name, deviceID, macAddress, model, sourceVersion, features, statusFlags`); canned-request unit test exists.
- [ ] **1.5** `/pair-setup`, `/pair-verify`, `/fp-setup`, malformed requests → documented errors; process survives (integration test).
- [ ] **1.6** Window `fps ≥ 60`, overlay present, ESC/Alt+F4 exit 0.
- [ ] **1.7** `config.json` + rotating log at the documented paths; log level respected.
- [ ] **1.8** Bind failure → actionable log + non-zero exit code.
- [ ] **1.9** §4.4 shutdown order; 10 cycles → zero leaked handles; 3 s watchdog proven.
- [ ] **1.10** First commit + remote + push exist; a clean-checkout `ci.yml` run is green and its URL is quoted (repo was at 0 commits — this gate is unevaluable until it does).
- [ ] **1.11** The advertised pairing bits match the implemented profile (§11.4) — mask, `advertise_pk`, and TXT agree; no capability is promised that Sprint 2 will not deliver.
- [ ] **1.12** The Ed25519 identity (`pk`/`pi`/`psi`) is persisted and identical across two runs.
- [ ] **§0.1** `ci.yml` uses the correct generator; brief §6.3 patched by @architect (or PRD normatively overrides it).
- [ ] **§4.2** Nothing from the deferred list is present in the diff.
- [ ] **Apache-2.0** `license-scan.yml` green; no GPL/AGPL file in `src/` or `third_party/`; `protocol-notes.md` records every derivation URL.
- [ ] **Appendix A** No Sprint 1 output claims video reception, mDNS browsing, or a WPF UI.

---

## 11. Open Decisions — @user (4 items, 3 with recommendations)

| # | Decision | Recommendation | Why it must be now |
|---|---|---|---|
| **11.1** | **UI stack for Sprint 1** | **Win32 + D3D11 window inside the C++ core.** WPF only in Sprint 3, and only if the team accepts `winget install Microsoft.DotNet.SDK.8` | No .NET SDK on this box → choosing WPF today converts Sprint 1 into toolchain setup. Zero extra installs keeps the installer small. |
| **11.2** | **4K scope** | **Cut 4K from v1.0.** 1080p60 is the supported target; 4K is a measured stretch goal after 1.0 | 4K inside 8 weeks is the single most likely way this project misses its date (R7, rated High likelihood). |
| **11.3** | **First frame = Sprint 2, not Sprint 1** | ✅ **CONFIRMED by @user** ("gas aja", 2026-09-16) — recorded as D3 in brief §9.1 | The original brief said Sprint 1 delivers "ada frame". Not achievable without pairing; a Sprint 1 frame claim would be false (Appendix A). Expectation change made explicit rather than silent. |
| **11.4** | **Pairing profile (transient vs PIN)** | ⚠️ **RECONCILED — this supersedes my earlier "legacy + static PIN" call.** Go **TRANSIENT**: advertise `0x40000280,0x10400` (bits 7, 9, 30, 42, **48** `SupportsTransientPairing`). Keep the PIN surface **specced but unbuilt** — @ui-designer already wrote it in DESIGN.md §8.2, so deferring loses nothing | My §11.4 v1 conflicted with brief §9.2 and with @reviewer's spec read; @ui-designer caught it. Per `airplay2-rs`, the transient flow (`X-Apple-HKP: 4`) **completes at M4** — the SRP shared secret *is* the key — so it **skips M5/M6 Ed25519 and skips pair-verify entirely**: no pairing store, no PIN UI, shortest path to the Sprint 2 frame gate. **This flips the advertised mask** away from the bit-27 legacy profile. @reviewer gates the mask under gate 1.11. |
| **11.5** | **UI language** (raised by @ui-designer) | Default **Bahasa Indonesia**, with every user-facing string in a single `strings.h` so a locale switch is a data change, not a refactor | The target user is an Indonesian-speaking owner, but this project is open source and international contributors may want English. Cheap to decide now, expensive to retrofit after strings are scattered. |

**Note for @pm/@foundry:** items 11.1–11.3 were already raised by @architect and @reviewer; 11.4 is new
(it closes the reviewer's advisory 1). Nothing below Task 0 is blocked by these — only Task 7's advertised
`features` value depends on 11.4, and Sprint 2 is gated by 11.3.

---

## Appendix — What Sprint 1 must NOT claim (carried from brief Appendix A)

- ❌ "We receive AirPlay video" — false until Sprint 2's pairing + decryption exists.
- ❌ "mDNS discovery works" as a *browse* feature — we **advertise**; browsing was the inverted direction.
- ❌ "WPF UI" — not buildable here today.
- ❌ "Links FFmpeg" — the installed FFmpeg is bin-only; a linkable build comes from vcpkg.
- ❌ "shairport-sync is MIT" — the licence picture is mixed (Fedora: `MIT AND BSD AND GPL-3.0`; Arch: `GPL-2.0-only`). Reference only.

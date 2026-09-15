# DESIGN.md — AirPlay HD Receiver (Windows) — UI Specification

> STEP 4 of the studio pipeline. Author: @ui-designer. Date: 2026-09-16 (**rev 2** — i18n (D6) + D5 de-scope).
> Inputs: `docs/architecture/ARCHITECTURE-BRIEF.md` v3.1 (§2 component map C13/C15/C16/C17, §5.5 renderer,
> §5.8 IPC schema v1 incl. `set_locale`, §2 thread table T7/T9/T10, gates 1.6/1.7/1.11/2.1/3.6) and
> `docs/prd/PRD-AIRPLAY-SPRINT1.md`.
> Locked decisions reflected here: **D1** Win32+D3D11 UI · **D4** Apache-2.0 · **D5** pairing = **transient**
> (PIN path stays specified but **not built**) · **D6** UI language = **English default + multi-language
> (`id`) from day one**, strings live in `assets/i18n/<locale>.json`.
> Downstream: @code-executor (Task 8 = Sprint 1 window; Sprint 2/3 surfaces *specified now, built later*),
> @reviewer (gates 1.6/1.7 visual criteria), @docs-writer (README screenshots).

**Medium note:** this is a **native Win32 + D3D11** desktop app owned by the C++ core (C16). There is no
web/CSS layer, no HTML, no npm. "Design tokens" below ship as **C++ constants** (`ui_theme.h`), and all
user-visible copy ships as **JSON locale files** (§7) — never as literals in code.

**Companion artifacts authored with this spec:** `assets/i18n/en.json` (default) and `assets/i18n/id.json`.
They are **content**, owned by design; the loader is code, owned by @code-executor.

---

## 1. Scope of this document

| Sprint | UI surface | Status in this doc |
|---|---|---|
| **1** | Win32 + D3D11 window + status/FPS overlay + **2 locales** + clean quit (**gates 1.6, 1.7**) | **SPECIFIED — build now** |
| 2 | Pairing surface on the same window — **transient** (D5): state-chip only, no PIN card | Specified, build in Sprint 2 |
| 3 | Settings panel (incl. language), diagnostics pane, system tray, hotkeys (C17) | Specified, build in Sprint 3 |
| 4 | Installer icon/branding only | Out of scope here |

**Deliberately not designed here:** the PIN card is specified in §9.4 but **not built** (D5 chose transient);
WPF/Tauri client; installer wizard; 4K affordances; multi-session UI (`maxSessions = 1`).

---

## 2. Design principles (these outrank any aesthetic preference)

1. **It is a surface, not a control panel.** The window's job is to *show state truthfully* and get out of
   the way. Config lives in `config.json` (C13); every later control must earn its place.
2. **Never imply a picture that does not exist.** Until Sprint 2's pairing + decryption lands, nothing may
   resemble a video frame, thumbnail, or "connecting to video…" promise (PRD Appendix A, as a UI rule).
   Idle = an explicit "ready / waiting for a Mac" state.
3. **Glanceable in one second.** Status is readable from 2 m: state word, then numbers. Colour is
   *reinforcement*, never the only carrier of meaning (§12).
4. **Failure is visible in-window, not only in the log** (gate 1.8).
5. **Never steal focus, never fight the user.** Opens non-topmost and centred; fullscreen/always-on-top are
   opt-in (Sprint 3). Launching must not interrupt what the user is doing.
6. **One view model, two renderers.** The Sprint 1 overlay and the Sprint 3 WPF client are fed by the *same*
   `StatusSnapshot` (brief §5.8). No renderer-only state.
7. **No hardcoded user-visible text.** Every string a human can read comes from the locale file (§7). A
   literal in a `.cpp` is a bug, not a shortcut — it is invisible to translators and untestable.

---

## 3. Design tokens

Ship as `src/ui/win32/ui_theme.h`. All geometric values are **DIPs**, scaled by `dpi / 96.0` at runtime (§4).

### 3.1 Colour (dark surface — a video app is a dark app)

```cpp
namespace arak::ui::theme {
// Surfaces
constexpr Rgba kBgVideo      = {0x10,0x12,0x16,0xFF}; // window / letterbox background (idle slate)
constexpr Rgba kOverlayPanel = {0x10,0x12,0x16,0xC7}; // 78% — card behind overlay text
constexpr Rgba kOverlayEdge  = {0xFF,0xFF,0xFF,0x1F}; // 12% hairline border
// Text
constexpr Rgba kTextPrimary  = {0xF2,0xF4,0xF7,0xFF}; // 16.9:1 on kBgVideo (AAA)
constexpr Rgba kTextSecondary= {0xA8,0xB0,0xBD,0xFF}; //  7.4:1 on kBgVideo (AAA)
constexpr Rgba kTextMuted    = {0x7A,0x82,0x8F,0xFF}; //  4.1:1 (AA large only)
// State — the only saturated colours in the app; always paired with a word (§12)
constexpr Rgba kStateOk      = {0x3D,0xD6,0x8C,0xFF}; //  9.6:1 — ready / receiving
constexpr Rgba kStateWarn    = {0xF5,0xA5,0x24,0xFF}; //  8.7:1 — degraded (fps < target)
constexpr Rgba kStateError   = {0xF0,0x4E,0x4E,0xFF}; //  5.9:1 — refused / failed
constexpr Rgba kStateInfo    = {0x58,0xA6,0xFF,0xFF}; //  7.3:1 — neutral activity (starting, pairing)
// Interaction (Sprint 3 only)
constexpr Rgba kAccent       = {0x6E,0x8B,0xFF,0xFF}; // focus ring, active toggle (≥3:1)
}
```

Rule: **at most one saturated colour on screen at a time** — the current state. Two competing state colours
means the design is wrong.

### 3.2 Type

| Token | Family | Size (DIP) | Weight | Used for |
|---|---|---|---|---|
| `kFontTitle` | Segoe UI | 20 | SemiBold | state label |
| `kFontBody` | Segoe UI | 15 | Regular | receiver name, addresses, detail line |
| `kFontLabel` | Segoe UI | 13 | Medium | metric labels (`fps`, `cpu`, `port`) |
| `kFontMetric` | **Consolas** | 15 | Regular | all numbers — tabular, no width jitter |
| `kFontPin` | Segoe UI | 72 | Bold | pairing code (specced-unbuilt, §9.4) |

Both families ship with Windows 10/11 — **no font files bundled** (installer < 100 MB, PRD §4.1). Both cover
Latin script; no non-Latin script support is claimed for v1.

### 3.3 Space, radius

```cpp
constexpr int kSp1=4, kSp2=8, kSp3=12, kSp4=16, kSp5=24, kSp6=32;
constexpr int kRadiusCard = 8;      // overlay card
constexpr int kRadiusChip = 999;    // state chip = pill
constexpr int kOverlayMargin = 16;  // inset from the window edge
constexpr int kHairline = 1;
```

### 3.4 Layout tolerance for i18n (mandatory)

Translated strings are **not** the same length as English (`READY` → `SIAP`, and a future locale may be
longer than both). Therefore:

- Every text box is laid out with **measured advance width**, never a fixed pixel width.
- The overlay card grows **rightward and downward** to fit; it never truncates a state label.
- If a detail line exceeds **60 % of the window width**, it wraps to a second line (max 2), then ellipsises.
  A truncated *label* is a defect; a truncated long *detail* is acceptable.
- Never hardcode a string's pixel width into the overlay layout — this is the single most common i18n bug.

---

## 4. Window specification (C16, Sprint 1 — gate 1.6)

| Property | Value | Rationale |
|---|---|---|
| Class | `ArakatianAirPlayWnd` | — |
| Title | `i18n::get("app.title")` + `" — "` + `receiver.name` | Title is localised; the receiver name is data, never translated |
| Default size | 1280 × 720 DIP, centred on the monitor under the cursor | 16:9 matches the 1080p target |
| Minimum size | 640 × 360 DIP | Below this the overlay stops being glanceable; clamp, don't reflow |
| Resizable | Yes (border + caption) | Sprint 1 is a normal window |
| Topmost | **No** by default (opt-in Sprint 3) | Principle 5 |
| Background | `kBgVideo` presented every frame | Proves the present loop is alive **without** faking video |
| DPI | Per-Monitor v2 (`SetProcessDpiAwarenessContext`), live on `WM_DPICHANGED` | Fractional scaling is the common laptop case |
| Cursor | Hidden after 3 s idle in fullscreen only (Sprint 3) | — |
| Startup | Visible, activated **once**, never re-focused | Principle 5 |

### 4.1 Input contract (Sprint 1)

| Input | Behaviour | Note |
|---|---|---|
| `ESC` | Quit via the §4.4 shutdown order, exit 0 | Gate 1.6 |
| `Alt+F4` | Quit via the §4.4 shutdown order, exit 0 | Gate 1.6 |
| `WM_CLOSE` | Same path as `Alt+F4` (no tray yet) | — |
| `F1` | Toggle overlay **compact ↔ full** | Free; helps QA capture gate evidence |
| *(Sprint 3)* `Ctrl+Alt+L` | Cycle locale | Specified in §10.1, not built now |

**Forward-compat rule (do not hard-code):** in Sprint 3 fullscreen, `ESC` must mean *exit fullscreen* and
only quit when already windowed. Route both through one `OnEscape()` handler now, so Sprint 3 changes the
handler body instead of rewriting the wndproc (T9).

### 4.2 Threading contract (brief §2)

- **T9 `ui`** owns `GetMessage`/`DispatchMessage` and all window + locale state.
- **T7 `render`** is the **only** thread that calls `Present`, and it draws the overlay (§5).
- T9 never blocks on T7: state crosses as an atomically-swapped immutable snapshot (double-buffered
  `StatusSnapshot` + release/acquire), never a mutex held across `Present`.
- **Locale change is a T9 event.** T9 resolves all strings into the `OverlayModel` (already-localised plain
  strings) and hands that model to T7. **T7 never calls `i18n::get()`** — the render thread holds no locale
  state and no file handles, so a locale switch can never stall a frame.

---

## 5. Status / FPS overlay (C16, drawn by T7)

### 5.1 Mechanism (recommendation, to avoid a Sprint 1 dependency trap)

The overlay updates at **1 Hz** (fed by `StatsCollector`, C18/T10), so rasterisation cost is irrelevant.

**Invariant (not negotiable).** T9 owns the data and the locale; T7 is the only thread that calls `Present`,
and the overlay is composited **into the swap chain as a texture**. Whatever rasterises the glyphs, the overlay
must end up as a D3D11 resource drawn by T7 — never as GDI output sitting on the window.

**Two accepted mechanisms (pick one — both run at 1 Hz, so cost is irrelevant):**

- **A — DirectWrite → A8 coverage bitmap → D3D11 texture** (original recommendation). DirectWrite ships with
  Windows, zero installs; slightly more wiring (text format + A8 texture + coverage sampling).
- **B — GDI → offscreen DIB section (`CreateDIBSection`) → `UpdateSubresource` into a D3D11 texture** ✅
  **Accepted as the Sprint 1 path** (decided 2026-09-16, after the implementation used `TextOutW`). It reuses
  GDI text code already written, needs one texture upload per second, and adds no shader work.

**Forbidden — cannot work, do not try:** rasterising GDI text **directly onto the flip-model swap chain or the
HWND client area**. `IDXGISurface1::GetDC` is not supported on flip-model swap chains, and the brief mandates
flip-model (§3). GDI-on-HWND + flip-model yields text that flickers, is wiped by `Present`, or never
composites at all. Mechanism **B** exists precisely to avoid that.

**Antialiasing rule (applies to both A and B).** The overlay card is **78 % translucent**, and subpixel AA
(ClearType) assumes an opaque background — it would produce colour fringing when alpha-blended over video. Text
must use **grayscale AA**: GDI `SetTextRenderingHint(..., GGO_GRAY8_BITMAP)`; DirectWrite
`DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE`. Consequence for mechanism B: the DIB must be **32-bit BGRA with a real
alpha channel**, not a 24-bit opaque DIB.

**Gate impact: none.** Gate 1.6 tests observable output (overlay present, `fps ≥ 60`, `ESC`/`Alt+F4` exit 0); it
does not name a rasteriser, and §5.1 named one as a *recommendation*. The gate wording stays as written — this
is a specification amendment, not a gate change.

Rejected: hand-baked glyph atlas (extra asset pipeline for zero benefit) · separate layered window (extra HWND,
z-order fights, breaks "T7 presents everything").

**Caching note (A and B).** Text format / DIB + texture are created per (font, size, dpi) and **cached** — never
per frame. Re-rasterise on `WM_DPICHANGED` and on locale change. Indonesian is Latin script, so no font-fallback
work is required today; this is future-proofing only.

### 5.2 Layout — two densities, same data

Overlay is anchored **top-left**, inset `kOverlayMargin`. It is a card, not a full-width bar, so the window
still reads as "the video surface is here".

**Compact** (default):

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  ┌──────────────────────────────────────┐                            │
│  │ ● READY · waiting for a Mac          │  ← state chip (pill, kStateOk)
│  │ Arakatian PC · 192.168.1.20:7000     │  ← name + address:port
│  │ fps 60.0   present 17942             │  ← metrics (Consolas, tabular)
│  └──────────────────────────────────────┘                            │
│                                                                      │
│                        (kBgVideo everywhere else)                     │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

**Full** (`F1`) adds: CPU/RAM, interface + bind result, active locale, log path, and (Sprint 2+) peer +
decode mode + `av_delta_ms`. This is the QA evidence surface — the verification bundle can screenshot it.

### 5.3 Overlay content contract (maps 1:1 to §5.8 `status`)

| Overlay row | i18n keys | Source (brief §5.8) | Sprint |
|---|---|---|---|
| state chip label | `state.<s>.label` | `state` (`idle｜advertising｜connected｜streaming｜error`) | 1 |
| state detail | `state.<s>.detail` | derived (§6) | 1 |
| receiver name | *(data — never translated)* | `receiver.name` | 1 |
| `address:port` | *(data)* | `receiver.addresses[0]` + `receiver.rtspPort` | 1 |
| fps / present | `metric.fps`, `metric.present` | `video.fps`, `video.presented` | 1 |
| drop | `metric.drop` | `video.dropped` | 2 |
| cpu / ram | `metric.cpu`, `metric.ram` | `sys.cpuPct`, `sys.ramMb` | 1 |
| peer name / address | `metric.peer` | `peer.deviceName`, `peer.address` | 2 |
| codec / resolution / decode | `metric.source`, `metric.decode` | `video.codec`, `video.width×height`, `video.decodeMode` | 2 |
| av delta | `metric.avdelta` | `sync.avDeltaMs` | 3 |
| audio latency / underruns | `metric.audio` | `audio.latencyMs`, `audio.underruns` | 3 |

**Rule:** the overlay renders only fields the schema actually carries. An absent field's **row is omitted** —
never rendered as `0`, `--`, or a placeholder that could be read as a measurement.

### 5.4 Frame-loop truthfulness (gate 1.6 evidence)

- `present` is a **monotonic** count of `Present()` calls; it must visibly increment every second.
- `fps` is a 1 s rolling average from that counter, one decimal, `Consolas`.
- `fps < 55` for 3 consecutive seconds → chip becomes `kStateWarn` with `state.degraded.label`
  (**degraded**, not error). A slow loop is *shown*, never averaged away.
- `drop` does not exist in Sprint 1 (no decoder) — the row is omitted (§5.3). A Sprint 1 drop counter would
  imply frames exist.

---

## 6. State model + copy deck

States are the brief's `SessionState`. The UI never invents a state the protocol does not have.
All strings below live in `assets/i18n/*.json` (§7), never in code.

| State | Chip colour | `state.<s>.label` (en / id) | `state.<s>.detail` (en / id) | Sprint |
|---|---|---|---|---|
| `idle` | `kStateInfo` | `STARTING` / `MENYALA` | `preparing network advertisement…` / `menyiapkan iklan jaringan…` | 1 |
| `advertising` | `kStateOk` | `READY` / `SIAP` | `waiting for a Mac to connect` / `menunggu Mac terhubung` | 1 |
| `connected` | `kStateInfo` | `CONNECTED` / `TERHUBUNG` | `{peer}` | 2 |
| `streaming` | `kStateOk` | `RECEIVING` / `MENERIMA` | `{width}×{height} · {codec} · {fps}` | 2 |
| `error` | `kStateError` | `FAILED` / `GAGAL` | `{reason}` | 1 |
| *(derived)* degraded | `kStateWarn` | `LOW FPS` / `FPS RENDAH` | `fps {fps} — check CPU/GPU` / `fps {fps} — cek CPU/GPU` | 1 |

**Copy rules.**
- Short and plain; no protocol jargon a non-technical user would not read.
- **Labels are uppercase words of ≤ 12 characters.** The chip is the one place where length is visually
  load-bearing (§3.4); a new locale must respect this or the chip grows.
- A detail line may contain placeholders (`{peer}`, `{fps}`, `{width}`, `{height}`, `{codec}`, `{reason}`).
  **The renderer substitutes; the locale file never contains a number.**
- The words *receiving* / *video* never appear before Sprint 2 (Principle 2) — `state.streaming.*` exists in
  the locale files but is **unreachable** until Sprint 2 wires the state machine.
- A translation must never change a label's **meaning class** (a warning must not become an error word).
  This is a translation-review rule recorded in §15.4.

---

## 7. i18n contract (D6 — user decision: English default, multi-language incl. Indonesian)

### 7.1 Files and ownership

| Item | Contract |
|---|---|
| Location | `assets/i18n/<locale>.json` (BCP-47 primary subtags: `en`, `id`) |
| Shipped in Sprint 1 | `en.json` (**default**), `id.json` — both authored with this spec |
| Authored by | **@ui-designer** (the copy deck is a design artifact; the files are content, not code) |
| Loader owned by | @code-executor: `i18n::get(key, args…)`, `i18n::setLocale(locale)`, `i18n::locales()` |
| Config key (C13) | `locale` — `"en"` default; **survives restart** (gate 1.7) |
| Bridge (brief §5.8) | `hello` carries `locales[]` + active `locale`; command `set_locale` switches at runtime |
| Exposed on screen | Sprint 1: **config-only, no on-screen picker** (restart to change). Sprint 3: the Settings panel gains a Language control (§10.1) |

### 7.2 Resolution + fallback — three tiers, never a blank, never a raw key

```
1. active locale (e.g. id)  →  hit?  render
2. en (default)             →  hit?  render + log once: "i18n: missing key '<k>' in 'id'"
3. compiled-in literal      →  render + log once at warn level
```

- **Tier 3 exists on purpose.** `en.json` itself can be damaged or miss a newly added key; the last resort is
  an English literal compiled into the binary — **never** the raw key id, **never** an empty label. Showing
  `state.advertising.label` to a user is a worse failure than showing `READY`.
- Missing-key logging is **once per (key, locale) per process** — a 1 Hz overlay must not spam the log.
- Unknown locale in config → fall back to `en`, log once, and **do not** overwrite `config.json` (an
  unsupported locale is not corruption to be silently "fixed").
- `set_locale` for an unshipped locale returns the §5.8 `reply` shape with `ok:false`,
  `error.code:"invalid_value"`; the UI reverts its control to the still-active locale (§10.1).

### 7.3 Placeholder, number and encoding rules

- Placeholders are `{name}`, substituted in a way that tolerates **reordering** by a translator.
- Numbers are formatted by the **renderer**, never stored in the locale file. Thousands/decimal marks follow
  the active locale (Indonesian uses `.` for thousands — `1.942`; English `1,942`). `fps` keeps one decimal
  in both locales.
- Files are **UTF-8 without BOM**; keys are ASCII `dotted.lowercase`; values contain no markup.
- **Never build a user-visible sentence by concatenating translated fragments.** A full sentence is one key
  with placeholders — this is why `state.streaming.detail` is a single key, not `label + " · " + detail`.

### 7.4 What is *not* translated

Receiver `name` from config, `deviceId`, addresses, ports, codec ids (`h264`), interface names, file paths,
and log content. These are data or machine identifiers — translating them would make a screenshot
unverifiable against the protocol.

---

## 8. Failure surface (gate 1.8) — in-window, not log-only

Bind failure (port in use / no usable interface) renders as a **blocking card**, centred, above the overlay:

```
┌──────────────────────────────────────────────────────────────┐
│  ✖ FAILED TO START                                           │
│  RTSP port 7000 is already in use by another program.        │
│  Close that program, or change "rtsp.port" in config.json.   │
│                                                              │
│  Log: %LOCALAPPDATA%\Arakatian\AirPlayReceiver\logs\         │
└──────────────────────────────────────────────────────────────┘
```

Keys: `error.bind.title`, `error.bind.port_in_use`, `error.bind.no_interface`, `error.bind.hint`,
`error.log_path`.

Rules: (a) name the **specific** cause and the **specific** fix; (b) always show the log path; (c) the process
still exits non-zero (gate 1.8) but only **after** the card has been presented ≥ 3 s or the user dismissed it —
a window that vanishes before it can be read is a useless error surface. The `error` state reuses this card in
later sprints (session failures), same three rules. Locale does not change the gate: 1.8 asserts the **exit
code**, not the wording.

---

## 9. Pairing surface (Sprint 2)

`UiWindow` **is** the pairing surface — there is no separate pairing window (brief §2 C16). D5 chose
**transient**, so §9.1 is the built path; §9.4 is specified and parked.

### 9.1 ✅ ACTIVE — transient pairing (no PIN) — **D5**

No code, no new surface: reuse the state chip.

| Moment | Chip | Keys |
|---|---|---|
| pair request arrives | `kStateInfo` · `pairing.requested.label` | `pairing.requested.detail` (`{peer} is requesting to connect`) |
| pair succeeds | `kStateOk` · transitions to `state.streaming.*` | — |
| pair refused / timed out | `kStateError` · `pairing.failed.label` | `pairing.failed.detail` |

Design cost: **zero new UI, three keys total** — and nothing to localise for a code that does not exist. This
is why transient is also the cheaper *design* path, not merely the cheaper protocol path.

### 9.4 🅿️ PARKED — PIN pairing (HomeKit-style pair-setup) — **specified, NOT built (D5)**

Kept so the surface is ready if a later sprint wants persistent trusted devices. Parked, not deleted.

```
┌──────────────────────────────────┐
│     Enter this code on your Mac  │   ← pairing.pin.title
│                                  │
│           4 8 2 1                │   ← kFontPin, 4 DIP tracking between digits
│                                  │
│   AirPlay will show a code       │   ← pairing.pin.detail
│   request on the Mac's screen.   │
│                                  │
│   Expires in 60 s                │   ← pairing.pin.expiry, live countdown
│   [ Cancel ]                     │   ← pairing.pin.cancel — only control
└──────────────────────────────────┘
```

Rules: always 4 digits, always grouped `4 8 2 1` (grouping is what makes a code transcribable at 3 m); digits
≥ 72 DIP at ≥ 12:1 contrast; the countdown is a real deadline from the handshake; on expiry the card
self-dismisses to `error` with `pairing.pin.expired`. No "remember this device" checkbox — pairing-store policy
(C4) is not a UI toggle. **Never render a PIN before pair-setup actually starts** (Principle 2, generalised: no
credential for a session that does not exist).

---

## 10. Sprint 3 surfaces (specified now, built later)

### 10.1 Settings panel

Driven entirely by `C13 ConfigStore` + the §5.8 `cmd` set. Every control maps to one config key and one
command; there are no UI-only preferences.

| Field | Control | Command | Config key |
|---|---|---|---|
| **Language / Bahasa** | dropdown (from `hello.locales[]`) | `set_locale` | `locale` |
| Device name | text | `set_config` | `receiver.name` |
| Audio device | dropdown | `list_audio_devices` → `set_audio_device` | `audio.deviceId` |
| Latency buffer | slider 10–200 ms | `set_config` | `audio.targetLatencyMs` |
| Hardware decode | toggle | `set_config` | `video.hwDecode` |
| Scaling | segmented Fit/Fill/Stretch/PixelPerfect | `set_scaling` | `video.scaling` |
| Log level | dropdown | `set_config` + `reload_config` | `log.level` |
| UI scale | stepper 100–200 % | `set_config` | `ui.scale` |

Interaction rules: edits apply **on change** (core hot-reloads); the panel shows the value the **core** reports
back in the next `status`/`reply` — never an optimistic local value. A rejected value (`ok:false`) reverts the
control and shows the reply's `error.message` verbatim (e.g. `audio.targetLatencyMs must be 10..200`).

**Language is the one exception to "revert on reject":** choosing a language re-renders the whole panel, so on
`ok:false` the dropdown reverts **and the error text renders in the still-active locale** — never in the locale
that was rejected (otherwise the user gets an error message in a language they cannot read).

The dropdown lists locale **display names** from each file's `meta.name` (e.g. `English`, `Bahasa Indonesia`),
written **in that language, never translated into the other** — so a user who cannot read the current language
can still find theirs.

### 10.2 Diagnostics pane

Renders the C14 log ring (last N lines) + full-density metrics (§5.2). Read-only, monospace, with a **Copy**
button. Log lines are **not** translated (§7.4); only the pane's labels are.

### 10.3 Tray + hotkeys (C17)

| Item | Action (localised) |
|---|---|
| Tray icon (double-click) | Show/hide window |
| Tray menu | `tray.show` · `tray.fullscreen` · `tray.mute` · `tray.stats` · `tray.quit` |
| `RegisterHotKey` | `Ctrl+Alt+F` fullscreen · `Ctrl+Alt+M` mute · `Ctrl+Alt+S` stats · `Ctrl+Alt+Q` quit |
| Notification | Only for a state change needing the user: session failed, or first connect |

Rules: closing the window (X) minimises to tray **only in Sprint 3+**; in Sprint 1–2, X quits (gate 1.6 requires
a clean exit and a non-existent tray cannot be the close target). Tray tooltip text = `tray.tooltip` + receiver
name. Tray state is shown via a badge/overlay, not by reshaping the icon.

---

## 11. One view model → two renderers

```
StatsCollector(C18) ─1 Hz─┐
SessionState(C…)  ────────┼─► StatusSnapshot ──┬─► T9 resolves i18n ─► T7 overlay (Sprint 1)
Events(C14)       ────────┘   (immutable)       └─► CoreBridge(§5.8, carries locale) ─► WPF client (S3)
```

The `StatusSnapshot` is the only source for both surfaces. Consequence: a field absent from §5.8 is on screen
nowhere, and adding a UI metric means adding it to the schema first (additive, no `v` bump).

**Localisation happens in the UI layer, never in the core.** The core emits codes/enums (`state:"advertising"`);
the UI turns them into words in the active locale. A localised string inside the core would break the WPF client,
the log, and any protocol debugging.

---

## 12. Accessibility & legibility

- **No colour-only meaning.** Every state = colour **+ word** (§6). Colour is redundant reinforcement.
- **Contrast:** primary text ≥ 7:1 on `kBgVideo` (AAA), secondary ≥ 4.5:1. The card's 78% backdrop holds the
  ratio over *any* future video content, not just the Sprint 1 flat colour.
- **Translucency-safe antialiasing:** **grayscale AA only, never ClearType** — subpixel AA fringes when blended
  over a translucent card (see §5.1).
- **DPI:** per-monitor v2; the overlay re-rasterises on `WM_DPICHANGED` (T9 signals T7). Never bitmap-scale.
- **Glanceability:** state word ≥ 20 DIP at 2 m; PIN digits (if ever built) ≥ 72 DIP at 3 m.
- **Keyboard-only:** Sprint 1 needs `ESC`, `Alt+F4`, `F1` and nothing else. Sprint 3 tab order follows §10.1
  field order, focus ring `kAccent` (≥ 3:1).
- **Locale legibility:** verify the chip at 100 % and 150 % DPI in **both** shipped locales (gate 1.7 evidence).
  A label that fits in `en` but wraps in `id` is a defect to fix **in the locale file**, not in the layout.
- **No fabricated progress.** No spinner with an invented percentage; a spinner is allowed only where a real
  operation is pending (pairing countdown, config reload).

---

## 13. Wireframes (Sprint 1)

**A. Idle → advertising.** This is what @reviewer screenshots for gate 1.6 — and A + A′ together are the gate
1.7 evidence.

```
┌─ AirPlay Receiver — Arakatian PC ────────────────────────────── _ □ ✕ ┐
│  ┌──────────────────────────────────────┐                             │
│  │ ● READY · waiting for a Mac          │  ← pill, kStateOk           │
│  │ Arakatian PC · 192.168.1.20:7000     │                             │
│  │ fps 60.0   present 17942             │  ← Consolas, tabular        │
│  └──────────────────────────────────────┘                             │
│                (kBgVideo #101216 — flat, no fake frame)                │
└───────────────────────────────────────────────────────────────────────┘
   F1 = full overlay   ·   ESC / Alt+F4 = quit
```

**A′. Same window with `locale: "id"`** — identical layout, localised chip:

```
│  ┌──────────────────────────────────────┐
│  │ ● SIAP · menunggu Mac terhubung      │
│  │ Arakatian PC · 192.168.1.20:7000     │
│  │ fps 60.0   present 17942             │
│  └──────────────────────────────────────┘
```

Gate 1.7 evidence = **A + A′**, **plus** a restart proving `locale` persisted, **plus** the log path visible.

**B. Full overlay (`F1`) — QA evidence surface**

```
│  ┌────────────────────────────────────────────────────┐
│  │ ● READY · waiting for a Mac                        │
│  │ Arakatian PC · 192.168.1.20:7000                   │
│  │ fps 60.0   present 17942                           │
│  │ ───────────────────────────────────────────────    │
│  │ CPU 14.2%    RAM 312 MB      bind OK (7000)        │
│  │ iface 192.168.1.20  (Ethernet)                     │
│  │ locale en  ·  log %LOCALAPPDATA%\...\logs\         │
│  └────────────────────────────────────────────────────┘
```

**C. Degraded (honest reporting)**

```
│  ┌──────────────────────────────────────┐
│  │ ● LOW FPS · fps 31.2                 │  ← kStateWarn
│  │ Arakatian PC · 192.168.1.20:7000     │
│  │ check CPU/GPU — full overlay (F1)    │
│  └──────────────────────────────────────┘
```

**D. Bind failure (gate 1.8)** — see §8 (keys `error.bind.*`).

**E.** Sprint 2 transient pairing states — §9.1 (chip only). **F.** PIN card — §9.4 (parked). **G.** Sprint 3
settings / diagnostics / tray — §10.

---

## 14. Do / Don't

**Do**
- Show state as **word + colour**; exactly one saturated colour on screen.
- Present `kBgVideo` every frame — a live present loop is the Sprint 1 proof (gate 1.6).
- Take every user-visible string from `i18n::get()`; keep copy in `assets/i18n/*.json` (§7).
- Measure text width and let the overlay card grow (§3.4).
- Route `ESC` and `Alt+F4` through one handler (§4.1).
- Render only fields the §5.8 schema carries (§5.3).
- Resolve locale in **T9**; hand T7 plain strings (§4.2).

**Don't**
- Don't put a user-visible literal in a `.cpp`, and don't show a raw key id when a lookup misses (§7.2).
- Don't translate data: receiver name, addresses, ports, codec ids, paths, log lines (§7.4).
- Don't build a sentence by concatenating translated fragments (§7.3).
- Don't draw anything that reads as video, a thumbnail, or a "connecting to video…" promise before Sprint 2.
- Don't show `drop` / `av_delta` before the pipeline that produces them exists (§5.4).
- Don't build the PIN card, tray, hotkeys, or settings UI in Sprint 1–2 (§1, D5, PRD §4.2).
- Don't hard-code the background to pure `#000` — the idle slate distinguishes "no signal yet" from
  "playing black".
- Don't hold a mutex across `Present`, and don't touch the filesystem from T7 (§4.2).
- Don't let a UI fault take the core down: the Sprint 1 window lives in the core exe, so T9/T7 stay
  allocation-light and exception-free across the frame boundary (brief §3 IPC rationale).

---

## 15. Open items / decisions log

| # | Item | Status |
|---|---|---|
| 15.1 | **Pairing profile** (was: brief §9.2 vs PRD §11.4 contradiction) | ✅ **RESOLVED — D5 = transient**, mask `0x40000280,0x10400`; PIN parked (§9.4). Gate 1.11 is now an equality test (TXT vs code constant). |
| 15.2 | **PRD §8.1 cross-reference** (`§11.1` → should be `§11.4`) | ✅ RESOLVED by @prd-maker in the PRD revision. |
| 15.3 | **UI language** | ✅ **RESOLVED — D6: English default + `id` shipped**, strings in `assets/i18n/*.json`; §7 is the contract. |
| 15.4 | **Translation-review ownership** | 🟡 **OPEN (process; not a Sprint 1 gate).** No one owns reviewing `id.json` wording. Recommendation: @pm owns product voice; a native reviewer signs off before the first public release, enforcing §6's "meaning class" rule. |
| 15.5 | Fullscreen / always-on-top / multi-monitor / cursor auto-hide detail | 🟡 Deferred to the Sprint 3 polish pass (PRD §4.2). No Sprint 1–2 surface depends on it. |
| 15.6 | Locale-aware number formatting depth | 🟡 Sprint 1 formats `fps` (one decimal) and counters only. ICU-style word numbers are **out of scope**; §7.3's separator rule is the limit of what Sprint 1 needs. |

---

## 16. Handoff

| Consumer | What to take from this document |
|---|---|
| **@code-executor (Task 8, Sprint 1)** | §3 tokens + §3.4 i18n layout rule, §4 window / input / threading, §5 overlay (mechanism, layout, contract, truthfulness), §6 copy deck, **§7 i18n contract**, §8 failure card, §13 wireframes A/A′/B/C, §14. Also consumes `assets/i18n/en.json` + `id.json` — **wiring the loader (`i18n::get`/`setLocale`) and the `locale` config key is his; the copy is mine.** |
| **@reviewer (gates 1.6, 1.7, 1.11)** | **1.6** → §4.1 (`ESC`/`Alt+F4` exit 0), §5.4 (monotonic `present`, honest `fps`), §13 A. **1.7** → §13 **A + A′** in both locales + locale survives restart + log path shown. **1.11** → not a UI gate; §15.1 records the mask this UI assumes. |
| **@docs-writer** | §13 A is the screenshot to embed. §7.4 lists what is *not* translated — useful for a README "Languages" section: `en` default, `id` shipped, add a locale by dropping a JSON file in `assets/i18n/`. |
| **Sprint 2** | §9.1 (transient — the built path), §5.3 rows tagged Sprint 2. |
| **Sprint 3** | §10 (settings incl. Language, diagnostics, tray/hotkeys), §11 (single view model). |

**Not proven / not claimed here:** no video frame, no decoder, no audio surface, no PIN card, no tray, no
settings UI, and **no runtime locale switching** (Sprint 1 locale is config + restart). Sprint 1's UI
deliverable is exactly: **a window that presents ≥ 60 Hz, tells the truth about state in two languages, and
exits cleanly.**

# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Nothing here is released and nothing here receives AirPlay yet — no AirPlay
protocol code has been written. See the Status section of the README.

### Added

- Repository scaffolding: CMake + vcpkg manifest (`openssl`, `spdlog`, `catch2`;
  `ffmpeg` behind the `video` feature, `mdnsresponder` behind `mdns`),
  CMake presets, `src/` + `tests/` skeleton (~100 lines: version info, empty
  Win32 entry point, one unit test).
- Architecture Brief, Sprint 1 PRD, DESIGN.md and the pipeline handoff log
  under `docs/`.
- Apache-2.0 license, README, English and Bahasa Indonesia UI strings in
  `assets/i18n/`.
- GitHub Actions CI workflow (first run failed — CMake could not find a Visual
  Studio instance on the runner; fix in progress).

### Known issues

- `CMakePresets.json` uses `"version": 6`, which CMake 4.x refuses to read when
  `$schema` is present — every `cmake --preset ...` command fails until it is
  fixed.
- CI has not been green yet; no build, no test run and no artifact exist.

[Unreleased]: https://github.com/gh05t1733/airplay-receiver

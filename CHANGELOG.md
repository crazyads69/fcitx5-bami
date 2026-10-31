# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Vietnamese input method engine with Telex, VNI, VIQR, and MS layout variants
- Go engine (cgo c-archive) with vendored bamboo-core and local patches
- C++17 Fcitx 5 addon with two-phase composition (edit / preedit)
- Per-application strategy selection with safe surrounding-text editing
- English protection using Enchant dictionary and local n-gram model
- Contextual prediction with optional candidate window
- Macros, field-local VI/EN toggle, and diagnostics tools
- Pixel-art RPG bánh mì icon set (16×16 to 512×512)
- GitHub Actions CI for Go tests and addon builds across Ubuntu/Debian
- Debian, Fedora, and Arch packaging recipes
- End-to-end test harnesses for X11 and Wayland

### Security
- Privacy-first design: learning off by default, no typed text logging
- 0600/0700 file permissions for user data
- Atomic writes for persistent state

## [0.1.0] - 2026-10-31

### Added
- Initial release: Go engine, cgo bridge, Fcitx5 addon
- MIT license

[Unreleased]: https://github.com/crazyads69/fcitx5-bami/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/crazyads69/fcitx5-bami/releases/tag/v0.1.0

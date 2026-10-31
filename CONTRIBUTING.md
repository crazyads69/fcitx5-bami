# Contributing to fcitx5-bami

Thank you for your interest in improving fcitx5-bami! This project is a
Vietnamese input method for Fcitx 5, built with a Go engine (cgo c-archive) and
a C++17 addon. All contributions are welcome under the MIT license.

## Quick start

1. Fork the repository on GitHub.
2. Clone your fork locally.
3. Create a feature branch: `git checkout -b my-feature`.
4. Make your changes, add tests, and verify locally.
5. Push and open a Pull Request.

## Development environment

### Required tools

- CMake >= 3.22
- C++17 compiler (GCC 11+, Clang 14+)
- Go >= 1.18 (no third-party Go modules)
- Fcitx 5.0.14 or newer development packages
- Enchant 2 and an English dictionary (e.g., `hunspell-en-us`)
- Python 3 (for tests and training tools)

### Install build dependencies

**Debian / Ubuntu:**
```sh
sudo apt install build-essential cmake extra-cmake-modules pkg-config golang-go \
  libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev libenchant-2-dev \
  hunspell-en-us libxkbcommon-dev libxcb-ewmh-dev \
  fcitx5-modules-dev fcitx5-modules fcitx5-data python3
```

### Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Go checks

```sh
go vet ./...
go test -race -count=1 ./...
```

## What to contribute

- **Bug fixes:** Please include a minimal reproduction and a regression test.
- **New features:** Open an issue first to discuss design and scope.
- **Documentation:** Fixes, clarifications, and translations are welcome.
- **Tests:** Coverage for untested code paths, especially application fixtures.
- **Packaging:** Improvements for Debian, Fedora, Arch, or Nix.

## Code style

- **Go:** Standard `gofmt`, `go vet`. No third-party dependencies.
- **C++:** Follow the existing style (4-space indent, `snake_case` for locals,
  `PascalCase` for types). Include `SPDX-License-Identifier: MIT` in new `.cpp`
  and `.h` files.
- **CMake:** Use `snake_case` variables, target-based commands.
- **Privacy-critical code** (key path, surrounding-text handling): every change
  must be justified with a test or a reproducible failure.

## Testing requirements

- New engine logic must have a Go unit test.
- New C++ paths must be covered by an existing or new CTest program.
- All CI jobs (Go, addon in containers) must pass before merge.
- End-to-end tests are in `test/e2e-x11/` and `test/e2e-wayland/`; report
  results with container image and actual measured throughput.

## Commit messages

Use clear, imperative commit messages:

```
Fix tone placement for directly typed marked vowels in MS layout

Adds regression test covering the affected input sequence.
Rationale: the existing flattener applied the tone to the first marked
vowel instead of the last, producing wrong output for "hoà" in MS mode.
```

## Security and privacy

- Never log typed text unless an explicit debug flag is set.
- Learning is OFF by default.
- Do not learn in password, URL, or email fields.
- User data files must be mode `0600` in a `0700` directory, written atomically.
- If your change touches the key path, document its timing and allocation
  behavior.

## License

By contributing, you agree that your contributions will be licensed under the
MIT License. See `LICENSE` for the full text.

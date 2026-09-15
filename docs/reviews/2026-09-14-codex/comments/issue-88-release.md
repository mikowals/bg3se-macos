# Issue #88 — v0.44.0 release comment (2026-09-15, Codex gpt-5.6-sol copy pass)

Shipped in v0.44.0: https://github.com/tdimino/bg3se-macos/releases/tag/v0.44.0

Two changes landed. `cmake -B build` now compiles the exact include chain that fails on your machine (`<tuple>` + `<MetalKit/MetalKit.h>`) before generating any target. On failure, it reports `xcode-select -p`, `xcrun --show-sdk-path`, `CMAKE_OSX_SYSROOT`, the compiler, and where `<tuple>` was found, followed by the three fixes in likelihood order. `PYTHONPATH=tools python3 -m bg3se_harness doctor` runs the same checks as the `toolchain_*` rows. `-DBG3SE_SKIP_TOOLCHAIN_PROBE=ON` bypasses the probe and exposes the raw compiler error.

Post `xcode-select -p`, `xcrun --sdk macosx --show-sdk-path`, `clang++ --version`, and the `CMAKE_OSX_SYSROOT` and `CMAKE_CXX_COMPILER` lines from `build/CMakeCache.txt`. The new configure error also prints all of them. That output will pin the cause on a CommandLineTools-only Sonoma install, which I cannot reproduce here. Leaving this open for your output.

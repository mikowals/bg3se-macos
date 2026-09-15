Correction to the July reply: SDK auto-detection landed in `720d067` on 2026-03-31, before your report, so it is not the fix. And `doctor` does not inspect the toolchain at all, despite `docs/harness.md` saying it verifies the SDK. Both are on me.

Queued for v0.44.0: a configure-time ObjC++ compile probe (`<tuple>` + MetalKit) with sysroot diagnostics in CMake, and `doctor` checks for `xcode-select -p`, `xcrun --show-sdk-path`, the compiler and the libc++ headers.

To pin the cause on your machine, post `xcode-select -p`, `xcrun --sdk macosx --show-sdk-path`, `clang++ --version`, and the `CMAKE_OSX_SYSROOT` and `CMAKE_CXX_COMPILER` lines from `build/CMakeCache.txt`. Keeping open.

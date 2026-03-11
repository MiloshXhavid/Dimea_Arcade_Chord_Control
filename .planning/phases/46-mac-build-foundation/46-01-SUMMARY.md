---
plan: 46-01
phase: 46-mac-build-foundation
status: complete
completed: 2026-03-11
---

# Summary: Fix CMakeLists.txt for Xcode Configure

## What Was Built

Fixed `CMakeLists.txt` so `cmake -G Xcode` succeeds on macOS with exit 0.

**Changes made:**
1. Wrapped the bare `configure_file()` call (installer .iss.in → .iss) in `if(WIN32)` — it previously ran unconditionally and caused a "file not found" error on Mac where the Inno Setup template doesn't exist
2. Inserted `if(APPLE)` block with `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` and `CMAKE_OSX_DEPLOYMENT_TARGET "11.0"` as CACHE STRING variables, placed after the WIN32 block and before `include(FetchContent)`

## Verification

- `cmake -G Xcode -B build-mac -S .` exits 0 ✓
- `build-mac/ChordJoystick.xcodeproj/project.pbxproj` exists ✓
- No `configure_file` error in cmake output ✓

## Notes

`CMAKE_OSX_ARCHITECTURES` shows empty in `build-mac/CMakeCache.txt` — this is Xcode 16 generator behavior where `ARCHS = "$(NATIVE_ARCH_ACTUAL)"` is set in the project. Plan 46-03 overrides this at xcodebuild invocation time via `ONLY_ACTIVE_ARCH=NO` (the standard Xcode 16.x universal binary workaround).

## Self-Check: PASSED

## Commits

- `8f6490e`: feat(46-01): fix CMakeLists.txt for Xcode configure on macOS

## Key Files

- `CMakeLists.txt` — guarded configure_file + added if(APPLE) universal binary block

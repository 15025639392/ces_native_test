# Session Log

Append new entries at the top. This file is for continuity across project-based AI sessions.

## 2026-05-13 02:00 Asia/Shanghai - Add Fast PoC Execution Rule

- Changed:
  - Added guidance that early PoC tasks should take large practical steps toward runnable end-to-end capability.
  - Updated `docs/PROJECT_INDEX.md`, `docs/HANDOFF_PROTOCOL.md`, and `docs/AI_HANDOFF.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Keep enough verification to catch obvious breakage, but record non-blocking risks instead of slowing early PoC progress.

## 2026-05-13 01:55 Asia/Shanghai - Link Real Cesium Native

- Changed:
  - Made Android CMake require a real Cesium Native package instead of allowing JNI-only fallback.
  - Defaulted `cesiumNativeRoot` to `/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a`.
  - Added Cesium Native install and vcpkg roots to CMake package/library/include lookup.
  - Replaced handwritten WGS84 ECEF math with `CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian`.
  - Added native per-frame tick stats through the Cesium bridge.
  - Updated docs from `cesiumNativeDir` to `cesiumNativeRoot`.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - APK `libcesium_bridge.so` contains `CesiumGeospatial` symbols and `cesium-native`, with no `jni-bridge` string.
  - APK installed and launched on adb device `7e045e39` after force-stop.
  - Screenshot `/tmp/cesium_native_real_link_restart_probe.png` showed satellite tiles and `cesium linked` in stats.
  - Logcat scan found no app crash, fatal exception, or `UnsatisfiedLinkError`.
- Not verified:
  - The visible imagery is still the Kotlin Canvas Gaode tile renderer, not Cesium-rendered 3D tiles.
- Notes / risks:
  - The app now depends on the local Cesium Native Android arm64 build root. A missing or stale root fails the native build immediately.

## 2026-05-13 01:35 Asia/Shanghai - Add Native Renderer Boundary

- Changed:
  - Added `NativeMapRenderer.kt` with `NativeMapRenderer`, `NativeCameraState`, and `NativeRenderStats`.
  - Moved Gaode tile loading, bitmap cache, Canvas drawing, and renderer cleanup into `GaodeCanvasTileRenderer.kt`.
  - Slimmed `GaodeSatellitePlatformView.kt` so `GaodeSatelliteView` now coordinates the frame loop, renderer, Cesium JNI bridge, stats, and lifecycle.
  - Updated README and handoff docs to describe the renderer boundary.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - APK still contains `lib/arm64-v8a/libcesium_bridge.so`.
  - APK installed and launched on adb device `7e045e39`.
  - Screenshot `/tmp/cesium_renderer_abstraction_probe.png` showed satellite tiles and `cesium jni-bridge` in stats.
  - Logcat scan found no app crash, fatal exception, or `UnsatisfiedLinkError`.
- Not verified:
  - Automated tests were not run because this PoC currently has no `test/` directory by project rule.
  - Did not link a real Cesium Native install yet; no local `cesium-native` package path was provided.
- Notes / risks:
  - Rendering still uses the Kotlin Canvas tile renderer; the new boundary is the insertion point for a Cesium-backed renderer or owner.

## 2026-05-13 01:30 Asia/Shanghai - Remove Early PoC Test Code

- Changed:
  - Removed the Flutter widget smoke check and deleted the empty `test` directory.
  - Removed the obsolete Flutter testing dev dependency and refreshed package resolution.
  - Updated handoff docs so early PoC verification uses `flutter analyze`, APK build, and emulator/device validation instead of automated test code.
- Verified:
  - `flutter pub get` passed.
  - `flutter analyze` passed.
- Not verified:
  - APK build and Android runtime were not relaunched for this documentation/dependency cleanup.
- Notes / risks:
  - Keep avoiding automated test files until the project stabilizes enough that they reduce, rather than add, AI handoff burden.

## 2026-05-13 01:20 Asia/Shanghai - Add Android Cesium Native JNI Bridge

- Changed:
  - Added Android CMake build under `android/app/src/main/cpp`.
  - Added `libcesium_bridge.so` JNI layer with native camera/cache lifecycle calls and ECEF probe stats.
  - Added `NativeCesiumBridge.kt` to own the native handle from the Android `PlatformView`.
  - Mirrored Flutter camera updates and clear-memory commands into the native bridge.
  - Added Cesium bridge status to Flutter stats as `cesium linked` or `cesium jni-bridge`.
  - Added optional Gradle property `-PcesiumNativeDir=/absolute/path` to link an installed Cesium Native CMake package.
  - Updated README and handoff docs.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - APK installed and launched on adb device `7e045e39`.
  - Screenshot `/tmp/cesium_native_bridge_probe.png` showed satellite tiles and `cesium jni-bridge` in stats.
  - Logcat scan found no app crash, fatal exception, or `UnsatisfiedLinkError`.
- Not verified:
  - Did not link a real Cesium Native install yet; no local `cesium-native` package path was provided.
- Notes / risks:
  - Rendering still uses the Kotlin Canvas tile probe.
  - The new bridge is the attachment point for Cesium Native ownership, not the final Cesium renderer.

## 2026-05-13 00:55 Asia/Shanghai - Add Lean Modular Code Rule

- Changed:
  - Added project guidance to keep code small, modular, and easy for future AI sessions to understand.
  - Updated `docs/PROJECT_INDEX.md`, `docs/HANDOFF_PROTOCOL.md`, and `docs/AI_HANDOFF.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Future changes should avoid speculative abstractions and only split modules around real boundaries.

## 2026-05-13 00:50 Asia/Shanghai - Make Low-Token Handoff The Default

- Changed:
  - Updated `README.md`, `docs/PROJECT_INDEX.md`, `docs/HANDOFF_PROTOCOL.md`, and `docs/AI_HANDOFF.md` so low-token handoff is the project default, not something the user must request each time.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Future project-based AI sessions should start with `PROJECT_INDEX.md` plus recent `SESSION_LOG.md` entries automatically.

## 2026-05-13 00:45 Asia/Shanghai - Add Low-Token Handoff Entry

- Changed:
  - Added `docs/PROJECT_INDEX.md` as the short first-read entry point.
  - Updated `docs/HANDOFF_PROTOCOL.md` to read only the index, recent log entries, and task-related files by default.
  - Updated `README.md` and `docs/AI_HANDOFF.md` to point to the low-token path.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Future sessions should avoid loading full docs/code unless the task requires it.

## 2026-05-13 00:40 Asia/Shanghai - Add Cross-Session Handoff Docs

- Changed:
  - Added `docs/HANDOFF_PROTOCOL.md` with start/end-of-session rules.
  - Added this `docs/SESSION_LOG.md` for future change records.
  - Updated `docs/AI_HANDOFF.md` to point future sessions to both files.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - The project directory may not be a git repository, so future sessions should inspect files directly and use this log as the continuity layer.

## 2026-05-13 00:35 Asia/Shanghai - Initial Android Tile Probe And Handoff

- Changed:
  - Created Flutter Android PoC at `/Users/ldy/Desktop/work/cesium_native_android_poc`.
  - Added Android `PlatformView` rendering Gaode satellite XYZ tiles with Kotlin `Canvas`.
  - Added Flutter controls for camera sync, zoom, orbit, cache clear, and stats display.
  - Restricted Android build to `arm64-v8a`.
  - Added `docs/AI_HANDOFF.md` and updated `README.md`.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - Installed on adb device `7e045e39`; satellite tiles rendered successfully with about `60fps`, `63/63 tiles`, `0 errors`.
- Not verified:
  - No Cesium Native integration yet.
  - iOS and HarmonyOS not scaffolded.
- Notes / risks:
  - Direct Gaode tile endpoint is for development validation only and needs licensing/production review.

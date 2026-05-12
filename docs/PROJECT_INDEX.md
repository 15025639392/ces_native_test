# Project Index

Default entry point for AI sessions. Read this first, then only the latest entries in `docs/SESSION_LOG.md`.

## What This Is

Android-first Flutter PoC for validating the native rendering shell before adding Cesium Native.

Current implementation:

- Flutter app embeds Android `PlatformView`.
- Kotlin `NativeMapRenderer` boundary isolates renderer/cache ownership.
- `GaodeCanvasTileRenderer` loads and draws Gaode satellite XYZ tiles.
- Android app builds a C++ `cesium_bridge` JNI library.
- The bridge directly links a real Android arm64 Cesium Native build through `-PcesiumNativeRoot=/absolute/path`.
- Flutter sends camera/cache commands to native via `MethodChannel`.
- Native sends fps/draw/tile/cache stats back to Flutter.
- Android build targets `arm64-v8a`.

## Read Next

- Normal continuation: read top 1-3 entries of `docs/SESSION_LOG.md`, then inspect task-related files. This is the default; the user should not need to repeat it.
- Architecture questions: read `docs/AI_HANDOFF.md`.
- Multi-session rules: read `docs/HANDOFF_PROTOCOL.md`.

## Key Files

- `lib/main.dart`: Flutter UI, `AndroidView`, channel calls, stats display.
- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/MainActivity.kt`: PlatformView registration.
- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/GaodeSatelliteViewFactory.kt`: PlatformView factory.
- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/GaodeSatellitePlatformView.kt`: PlatformView, frame loop, stats, renderer/bridge coordination.
- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/NativeMapRenderer.kt`: renderer interface plus camera/render stats contracts.
- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/GaodeCanvasTileRenderer.kt`: Gaode tile loading, memory cache, Canvas draw, renderer dispose.
- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/NativeCesiumBridge.kt`: JNI owner for the native bridge.
- `android/app/src/main/cpp/CMakeLists.txt`: native build and optional Cesium Native package link.
- `android/app/src/main/cpp/cesium_bridge.cpp`: JNI bridge and camera/cache lifecycle probe.
- `android/app/build.gradle.kts`: Android config and `arm64-v8a` ABI filter.
- `android/app/src/main/AndroidManifest.xml`: internet permission and app entry.

## Commands

```sh
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

## Current Big Caveat

The renderer boundary and C++/JNI bridge exist, and the bridge now directly links Cesium Native. The visible imagery still renders through the Kotlin Canvas probe.

## Code Style Rule

Keep the project small and modular. Prefer a few focused files with clear ownership over broad rewrites or premature abstractions. If a new module is added, it should reduce complexity or isolate a real boundary such as Flutter UI, platform channel, native view lifecycle, tile loading, cache, renderer, or Cesium Native bridge.

Do not add automated test code in the early PoC phase. Use static analysis, Android builds, and emulator/device validation notes instead.

## Execution Rule

Early PoC work should move in large practical steps toward runnable capability. Prefer implementing the main path end to end over spending time on exhaustive edge cases, elaborate abstractions, or defensive polish. Keep enough verification to avoid obvious breakage, then record remaining risks in `docs/SESSION_LOG.md`.

# Project Index

Default entry point for AI sessions. Read this first, then only the latest entries in `docs/SESSION_LOG.md`.

## What This Is

Android-first Flutter PoC for validating a Cesium Native rendering stack behind a Flutter shell.

Current implementation:

- Flutter adapter package embeds Android `PlatformView`.
- Kotlin `TextureView` hosts an EGL render thread for the native globe surface.
- Android app builds a C++ `cesium_bridge` JNI library.
- The bridge directly links a real Android arm64 Cesium Native build through `-PcesiumNativeRoot=/absolute/path`.
- The bridge now owns tile selection, GPU resource preparation, and frame rendering on the main path.
- Flutter sends camera/cache commands to native via `MethodChannel`.
- Native sends fps/draw/tile/cache stats back to Flutter.
- Android build targets `arm64-v8a`.

## Read Next

- Normal continuation: read top 1-3 entries of `docs/SESSION_LOG.md`, then inspect task-related files. This is the default; the user should not need to repeat it.
- Architecture questions: read `docs/AI_HANDOFF.md`.
- Ideal target architecture: read `docs/ARCHITECTURE_IDEAL.md`.
- Migration path to target architecture: read `docs/ARCHITECTURE_MIGRATION.md`.
- SDK target blueprint: read `docs/SDK_BLUEPRINT.md`.
- SDK API draft: read `docs/SDK_API_DRAFT.md`.
- SDK API V1 contract: read `docs/SDK_API_V1.md`.
- Android SDK direct integration: read `docs/SDK_ANDROID_QUICKSTART.md`.
- Multi-session rules: read `docs/HANDOFF_PROTOCOL.md`.

## Key Files

- `examples/flutter-demo/lib/main.dart`: Flutter demo UI, `AndroidView`, channel calls, stats display.
- `sdk/flutter-plugin/lib/cesium_map_sdk.dart`: Flutter SDK package export surface.
- `sdk/flutter-plugin/lib/src/cesium_map_widget.dart`: `CesiumMapWidget`, per-view channel binding, Android-only PlatformView adapter.
- `sdk/flutter-plugin/lib/src/cesium_map_controller.dart`: Flutter controller abstraction and method-channel implementation.
- `sdk/flutter-plugin/android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/CesiumMapSdkPlugin.kt`: Flutter Android plugin entry, automatic PlatformView registration.
- `examples/flutter-demo/android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/MainActivity.kt`: demo Android host activity.
- `examples/android-native-demo/app/src/main/kotlin/com/example/cesiumpoc/native_demo/MainActivity.kt`: native Android demo launcher page.
- `examples/android-native-demo/app/src/main/kotlin/com/example/cesiumpoc/native_demo/NativeSdkMapActivity.kt`: direct `CesiumMapView` demo page.
- `examples/android-native-demo/app/src/main/kotlin/com/example/cesiumpoc/native_demo/NativeSdkFragmentActivity.kt`: direct `CesiumMapFragment` demo page.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapView.kt`: Android SDK public map host view.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapFragment.kt`: Android SDK public fragment host entry.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapController.kt`: Android SDK public controller contract.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapFragmentController.kt`: fragment-to-controller adapter.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumCameraState.kt`: Android SDK public camera contract.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapListener.kt`: Android SDK public event listener contract.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapError.kt`: Android SDK public error contract.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumRenderStats.kt`: Android SDK public render stats contract.
- `sdk/flutter-plugin/android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/internal/flutter/CesiumMapViewFactory.kt`: Flutter-only PlatformView factory.
- `sdk/flutter-plugin/android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/internal/flutter/CesiumMapPlatformView.kt`: Flutter-only PlatformView adapter.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/internal/render/CesiumMapRenderSurface.kt`: `TextureView`, EGL render thread, stats, and bridge coordination.
- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/internal/render/NativeCesiumBridge.kt`: JNI owner for the native bridge source file.
- `sdk/native-core/src/main/cpp/CMakeLists.txt`: native build and optional Cesium Native package link.
- `sdk/native-core/src/main/cpp/cesium_bridge.cpp`: JNI bridge, Cesium tileset lifecycle, selection, resource prep, and OpenGL draw path.
- `examples/flutter-demo/android/app/build.gradle.kts`: demo Android shell config and `arm64-v8a` ABI filter.
- `examples/flutter-demo/android/app/src/main/AndroidManifest.xml`: demo app entry and internet permission.

## Commands

```sh
cd sdk/android-sdk
./gradlew assemble

cd sdk/flutter-plugin
flutter analyze

cd /Users/ldy/Desktop/work/cesium_native_android_poc/examples/flutter-demo
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

## Current Big Caveat

The docs and code were briefly out of sync during the transition from the early Canvas probe to the native EGL/Cesium path. Trust the current code: the runtime renderer path is the native `cesium_bridge`.

## Code Style Rule

Keep the project small and modular. Prefer a few focused files with clear ownership over broad rewrites or premature abstractions. If a new module is added, it should reduce complexity or isolate a real boundary such as Flutter UI, platform channel, native view lifecycle, tile loading, cache, renderer, or Cesium Native bridge.

Do not add automated test code in the early PoC phase. Use static analysis, Android builds, and emulator/device validation notes instead.

## Execution Rule

Early PoC work should move in large practical steps toward runnable capability. Prefer implementing the main path end to end over spending time on exhaustive edge cases, elaborate abstractions, or defensive polish. Keep enough verification to avoid obvious breakage, then record remaining risks in `docs/SESSION_LOG.md`.

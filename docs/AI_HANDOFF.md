# AI Handoff

This is the full project handoff. It is not the default first read. AI sessions should start with `docs/PROJECT_INDEX.md` and recent `docs/SESSION_LOG.md` entries, then open this file only when more context is needed.

Default continuation files:

- `docs/PROJECT_INDEX.md`
- `docs/SESSION_LOG.md`

Useful integration reference:

- `docs/SDK_ANDROID_QUICKSTART.md`

Rules for multi-session work:

- `docs/HANDOFF_PROTOCOL.md`

## Project Goal

Build a minimal Android-first Flutter proof of concept around a real Cesium Native renderer on Android.

The current milestone is not a feature-complete globe product. It validates the native rendering foundation around a real Cesium Native renderer:

- Flutter as the adapter and demo shell layer.
- Android native `PlatformView` and `TextureView` as the rendering surface host.
- Native frame loop, EGL ownership, and memory release.
- A stable Android JNI adapter that isolates Flutter from Cesium Native details.
- A C++ bridge layer that owns Cesium Native tileset lifecycle, selection, resource prep, and draw work.
- Camera/state sync from Flutter to Android native code.
- Basic runtime metrics visible in the Flutter UI.

Target platforms from the broader plan are Android, iOS, and HarmonyOS, with Flutter as the cross-platform business/data layer. This repo currently implements only the Android `arm64-v8a` PoC.

## Current Status

Implemented and verified:

- Flutter demo project lives under `/Users/ldy/Desktop/work/cesium_native_android_poc/examples/flutter-demo`.
- Native Android demo project lives under `/Users/ldy/Desktop/work/cesium_native_android_poc/examples/android-native-demo`.
- Flutter SDK adapter package lives under `/Users/ldy/Desktop/work/cesium_native_android_poc/sdk/flutter-plugin`.
- Android SDK module now has its own Gradle wrapper and can be built directly from `/Users/ldy/Desktop/work/cesium_native_android_poc/sdk/android-sdk`.
- Android-only `PlatformView` registered as `gaode_satellite_view` and `cesium_map_view`.
- Flutter SDK package exposes `CesiumMapWidget` and `CesiumMapController`.
- Flutter demo consumes the SDK package instead of owning the channel code inline.
- Flutter Android plugin now auto-registers the native PlatformView; the demo host no longer manually wires `registerViewFactory`.
- Android SDK now exposes `CesiumMapView` as the public host view.
- Flutter `PlatformView` is now a thin internal adapter over `CesiumMapView`.
- Camera controls are sent from Flutter to native through `MethodChannel`.
- Native view sends stats back to Flutter through the same per-view channel.
- Native tile cache can be cleared from Flutter.
- `CesiumMapRenderSurface` coordinates the frame loop, EGL thread, Cesium bridge, and stats collection.
- Android builds a native `cesium_bridge` shared library.
- `cesium_bridge` directly links a locally installed Android arm64 Cesium Native CMake package with `-PcesiumNativeRoot=/absolute/path`.
- `cesium_bridge` now performs the main path render work instead of acting only as a lifecycle probe.
- Debug APK builds for `android-arm64`.

Last known verification:

```sh
cd sdk/android-sdk
./gradlew assemble

cd sdk/flutter-plugin
flutter analyze

cd /Users/ldy/Desktop/work/cesium_native_android_poc/examples/flutter-demo
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

All passed. The APK was installed on adb device `7e045e39` and launched successfully in an earlier validation run with the native Cesium renderer path active, but on-device launch was not rerun after the latest module split.

## Important Files

- `examples/flutter-demo/lib/main.dart`
  - Flutter demo app shell.
  - Owns UI controls for longitude, latitude, zoom, orbit toggle, and cache clear.
  - Consumes `CesiumMapWidget` from `sdk/flutter-plugin`.
  - Displays native stats and error state as a pure SDK consumer.

- `sdk/flutter-plugin/lib/src/cesium_map_widget.dart`
  - Flutter `AndroidView` adapter surface.
  - Owns per-view `MethodChannel` event wiring for `mapReady`, `cameraChanged`, `stats`, and `error`.

- `sdk/flutter-plugin/lib/src/cesium_map_controller.dart`
  - Flutter controller contract.
  - Owns `setCamera`, `getCameraState`, `clearMemory`, and `getStats`.

- `examples/flutter-demo/android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/MainActivity.kt`
  - Demo host activity only.
  - PlatformView registration is now plugin-driven instead of activity-driven.

- `examples/android-native-demo/app/src/main/kotlin/com/example/cesiumpoc/native_demo/MainActivity.kt`
  - Native Android demo launcher page.
  - Lets the user choose the direct `View` demo or the `Fragment` demo.

- `examples/android-native-demo/app/src/main/kotlin/com/example/cesiumpoc/native_demo/NativeSdkMapActivity.kt`
  - Minimal native Android SDK example page.
  - Consumes `CesiumMapView` directly without Flutter UI involvement.

- `examples/android-native-demo/app/src/main/kotlin/com/example/cesiumpoc/native_demo/NativeSdkFragmentActivity.kt`
  - Minimal native Android fragment-host example page.
  - Consumes `CesiumMapFragment` directly without Flutter UI involvement.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapView.kt`
  - Android SDK public map host view.
  - Wraps the internal render surface and is the new public Android entry.
  - Now exposes listener wiring plus basic host lifecycle methods.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapFragment.kt`
  - Android SDK public fragment host entry.
  - Reuses `CesiumMapView` and forwards fragment lifecycle into it.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapController.kt`
  - Public Android controller contract shared across host forms.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapFragmentController.kt`
  - Adapter that exposes `CesiumMapFragment` through the same controller contract.

- `sdk/flutter-plugin/android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/CesiumMapSdkPlugin.kt`
  - Flutter Android plugin entry.
  - Automatically registers `cesium_map_view` and legacy `gaode_satellite_view`.

- `sdk/flutter-plugin/android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/internal/flutter/CesiumMapViewFactory.kt`
  - Creates `CesiumMapPlatformView` instances for Flutter only.

- `sdk/flutter-plugin/android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/internal/flutter/CesiumMapPlatformView.kt`
  - Thin Flutter adapter that forwards channel calls into `CesiumMapView`.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumCameraState.kt`
  - Public Kotlin camera contract type for Android SDK and Flutter bridge.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapListener.kt`
  - Public Android map listener interface for ready, camera, stats, and error callbacks.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumMapError.kt`
  - Public Android error payload for listener callbacks.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/CesiumRenderStats.kt`
  - Public render stats contract for Android SDK and Flutter bridge.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/internal/render/CesiumMapRenderSurface.kt`
  - Internal `TextureView` render host.
  - Owns EGL render thread, camera sync, stats emission, and native bridge coordination.

- `sdk/android-sdk/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/internal/render/NativeCesiumBridge.kt`
  - Loads `libcesium_bridge.so`.
  - Owns the native handle lifecycle.
  - Mirrors camera/cache commands into C++.
  - Reports render and memory stats back to Kotlin.

- `sdk/native-core/src/main/cpp/CMakeLists.txt`
  - Builds `cesium_bridge`.
  - Accepts `CESIUM_NATIVE_ROOT` from Gradle property `cesiumNativeRoot`.
  - Calls `find_package(cesium-native CONFIG REQUIRED)` unconditionally.

- `sdk/native-core/src/main/cpp/cesium_bridge.cpp`
  - Provides JNI entry points.
  - Owns the Cesium tileset, camera projection, tile selection, resource preparation, and OpenGL draw path.
  - Tracks camera updates, memory clears, selected or loaded tile counts, draw timing, and GPU memory estimates.

- `examples/flutter-demo/android/app/build.gradle.kts`
  - Demo Android shell Gradle config.
  - Restricts native ABI to `arm64-v8a`.

- `examples/flutter-demo/android/app/src/main/AndroidManifest.xml`
  - Adds `android.permission.INTERNET`.

## Channel Contract

The primary per-view channel name is:

```text
cesium_map_view_<viewId>
```

Legacy compatibility registration still exists for:

```text
gaode_satellite_view_<viewId>
```

Flutter to native:

```text
updateCamera
```

Arguments:

```json
{
  "longitude": 116.397389,
  "latitude": 39.908722,
  "zoom": 15.0,
  "autoOrbit": false
}
```

```text
clearMemory
```

Arguments: none.

```text
getCameraState
```

Arguments: none.

```text
getStats
```

Arguments: none.

Native to Flutter:

```text
stats
```

Payload:

```json
{
  "fps": 60.0,
  "drawMs": 2.8,
  "visibleTiles": 63,
  "cachedTiles": 63,
  "loadedTiles": 63,
  "failedTiles": 0,
  "cacheMb": 15.8,
  "cesiumBackend": "cesium-native",
  "cesiumLinked": true,
  "cesiumCameraUpdates": 1,
  "cesiumMemoryClears": 0,
  "cesiumEcefX": -2170000.0,
  "cesiumEcefY": 4380000.0,
  "cesiumEcefZ": 4070000.0
}
```

Keep this contract stable if possible. Future Cesium Native integration can add methods, but avoid changing existing names unless updating both sides and this document.

## Runbook

From the repo root:

```sh
cd examples/flutter-demo
flutter pub get
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

Optional Flutter SDK package check:

```sh
cd sdk/flutter-plugin
flutter pub get
flutter analyze
```

Optional Cesium Native package link:

```sh
cd examples/flutter-demo
flutter build apk --debug --target-platform android-arm64 \
  --android-project-arg=cesiumNativeRoot=/absolute/path/to/cesium-native/build-android-arm64-v8a
```

Install and launch manually:

```sh
adb devices
adb -s <device-id> install -r build/app/outputs/flutter-apk/app-debug.apk
adb -s <device-id> shell am start -n com.example.cesiumpoc.cesium_native_android_poc/.MainActivity
```

Useful QA commands:

```sh
adb -s <device-id> exec-out screencap -p > /tmp/gaode_tile_probe.png
adb -s <device-id> exec-out uiautomator dump /dev/tty > /tmp/gaode_tile_probe_ui.xml
adb -s <device-id> logcat -d -t 300 | rg -i "cesium|gaode|flutter|fatal|exception|crash"
```

Launch the native Android SDK example page directly:

```sh
adb -s <device-id> shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkMapActivity

adb -s <device-id> shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkFragmentActivity
```

## Manual Validation Checklist

1. Launch on Android.
2. Confirm the native view presents the Cesium-backed globe output.
3. Confirm the top stats overlay updates once per second.
4. Move east/west/north/south and confirm the lon/lat text changes and the native globe camera moves.
5. Change zoom and confirm the native camera responds without surface loss or crash.
6. Tap the memory icon and confirm native memory stats react without crash.
7. Enable Orbit and confirm the native view continuously invalidates while fps remains visible.
8. Check logcat for crashes or obvious tile load exceptions.

## Known Limitations

- Cesium Native is not vendored into the repo. The C++/JNI bridge directly links a local Android arm64 build, defaulting to `/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a`.
- Rendering now uses `TextureView` plus an EGL OpenGL ES path driven by `cesium_bridge`.
- Gaode tiles are loaded from a direct URL:

```text
https://webst0%d.is.autonavi.com/appmaptile?style=6&x=%d&y=%d&z=%d
```

- The direct Gaode endpoint is for development validation only. Confirm licensing, authentication, quota, coordinate policy, and production endpoint requirements before using it in a product.
- Cache is memory-only and fixed at 64 MB.
- Tile request cancellation is coarse. `clearMemory()` clears pending keys and cache, but in-flight HTTP calls can still finish.
- The project supports Android only. Non-Android Flutter platforms show an unsupported message.
- iOS and HarmonyOS are not scaffolded here yet.

## Next Development Steps

Recommended order:

1. Keep the Flutter channel contract stable while hardening the current native renderer path.
   - Do not expose Cesium Native C++ APIs directly to Dart.
   - Keep the stable JNI adapter owned by Android native code.

2. Harden Android lifecycle behavior around the current `TextureView` and EGL thread.
   - Pause/resume behavior.
   - Surface recreation.
   - Thread stop and restart correctness.

3. Expand Cesium Native content capabilities on the existing C++ renderer path.
   - Use Cesium Native for tileset lifecycle, camera update, tile selection, metadata, and resource loading.
   - Add imagery, terrain, and richer scene inputs only after the current render loop is stable.

4. Add real performance instrumentation.
   - Android `dumpsys gfxinfo`.
   - Memory snapshots with `dumpsys meminfo`.
   - Optional Perfetto trace for jank and networking.

5. Decide whether to remove or formally keep the legacy Canvas renderer abstraction.
   - If kept, document it as reference code only.
   - If removed, delete dead paths once the EGL/Cesium path is fully verified.

6. Only after Android is stable, mirror the architecture for iOS.
   - Flutter `UiKitView`.
   - Swift/ObjC wrapper.
   - C++ bridge.
   - Metal or Filament renderer.

7. Treat HarmonyOS as a separate risk spike.
   - Validate Flutter HarmonyOS support.
   - Validate PlatformView equivalent.
   - Validate C++ NDK/NAPI bridge.
   - Validate graphics backend.

## Coding Notes For Future AI

- Before editing, read `docs/SESSION_LOG.md` and inspect the current working tree. Do not assume this document is fully current if code changed after the last log entry.
- After meaningful changes, update `docs/SESSION_LOG.md` with what changed, commands run, verification result, and any unresolved risk.
- Keep the codebase intentionally small. Add modules only for real boundaries or to make future AI/human handoff easier.
- Avoid speculative architecture. A module should earn its place by reducing complexity today.
- Early PoC tasks should advance the runnable main path in large steps. Do not be overly conservative when the goal is to prove feasibility.
- Prefer small, verifiable steps. This project exists to reduce risk before broader multi-platform or product-level globe work.
- Do not refactor unrelated Flutter template files unless the task requires it.
- Keep Android package paths aligned with `com.example.cesiumpoc.cesium_native_android_poc`.
- Keep generated build outputs out of documentation and source changes.
- If changing the platform channel payload, update this document and `README.md`.
- Do not add automated test code in the early PoC phase. Use analysis, APK build, and device/emulator validation instead.
- Always run at least:

```sh
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

- If an Android device is available, install and screenshot the app after changes.

# AI Handoff

This is the full project handoff. It is not the default first read. AI sessions should start with `docs/PROJECT_INDEX.md` and recent `docs/SESSION_LOG.md` entries, then open this file only when more context is needed.

Default continuation files:

- `docs/PROJECT_INDEX.md`
- `docs/SESSION_LOG.md`

Rules for multi-session work:

- `docs/HANDOFF_PROTOCOL.md`

## Project Goal

Build a minimal Android-first Flutter proof of concept for the future Cesium Native integration.

The current milestone is not full Cesium Native rendering. It validates the foundation that Cesium Native will later plug into:

- Flutter as the app shell and data/control distribution layer.
- Android native `PlatformView` as the rendering surface.
- Native tile loading, cache ownership, frame loop, and memory release.
- A `NativeMapRenderer` boundary that isolates render/cache ownership from the Android view lifecycle.
- A C++/JNI bridge layer that Cesium Native can attach to without changing the Flutter channel contract.
- Camera/state sync from Flutter to Android native code.
- Basic runtime metrics visible in the Flutter UI.

Target platforms from the broader plan are Android, iOS, and HarmonyOS, with Flutter as the cross-platform business/data layer. This repo currently implements only the Android `arm64-v8a` PoC.

## Current Status

Implemented and verified:

- Flutter app created under `/Users/ldy/Desktop/work/cesium_native_android_poc`.
- Android-only `PlatformView` registered as `gaode_satellite_view`.
- Flutter embeds the native view with `AndroidView`.
- Kotlin `GaodeCanvasTileRenderer` loads Gaode satellite XYZ tiles behind `NativeMapRenderer`.
- Camera controls are sent from Flutter to native through `MethodChannel`.
- Native view sends stats back to Flutter through the same per-view channel.
- Native tile cache can be cleared from Flutter.
- `GaodeSatelliteView` coordinates frame loop, renderer, Cesium bridge, and Flutter stats.
- Android builds a native `cesium_bridge` shared library.
- `cesium_bridge` directly links a locally installed Android arm64 Cesium Native CMake package with `-PcesiumNativeRoot=/absolute/path`.
- Debug APK builds for `android-arm64`.

Last known verification:

```sh
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

Both passed. The APK was installed on adb device `7e045e39` and displayed Gaode satellite imagery successfully. Observed first-screen stats were about `60fps`, `63/63 tiles`, `0 errors`, and roughly `2.8ms` draw time.

## Important Files

- `lib/main.dart`
  - Flutter app shell.
  - Creates `AndroidView(viewType: 'gaode_satellite_view')`.
  - Owns UI controls for longitude, latitude, zoom, orbit toggle, and cache clear.
  - Sends `updateCamera` and `clearMemory` over `MethodChannel`.
  - Receives native `stats` events and displays fps/draw/tile/cache metrics.

- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/MainActivity.kt`
  - Registers the Android platform view factory with Flutter.

- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/GaodeSatelliteViewFactory.kt`
  - Creates `GaodeSatellitePlatformView` instances.

- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/GaodeSatellitePlatformView.kt`
  - Contains the native platform view wrapper and the custom `GaodeSatelliteView`.
  - Coordinates camera updates, frame loop, renderer calls, Cesium bridge calls, stats, and dispose.

- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/NativeMapRenderer.kt`
  - Defines `NativeMapRenderer`.
  - Defines `NativeCameraState` and `NativeRenderStats`.

- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/GaodeCanvasTileRenderer.kt`
  - Loads tiles on a fixed 4-thread executor.
  - Stores decoded `Bitmap` tiles in a 64 MB `LruCache`.
  - Draws visible XYZ tiles on Android `Canvas`.
  - Reports render/cache stats to the view.
  - Releases cache and executor in `release()`.

- `android/app/src/main/kotlin/com/example/cesiumpoc/cesium_native_android_poc/NativeCesiumBridge.kt`
  - Loads `libcesium_bridge.so`.
  - Owns the native handle lifecycle.
  - Mirrors camera/cache commands into C++.
  - Reports whether Cesium Native is linked.

- `android/app/src/main/cpp/CMakeLists.txt`
  - Builds `cesium_bridge`.
  - Accepts `CESIUM_NATIVE_ROOT` from Gradle property `cesiumNativeRoot`.
  - Calls `find_package(cesium-native CONFIG REQUIRED)` unconditionally.

- `android/app/src/main/cpp/cesium_bridge.cpp`
  - Provides JNI entry points.
  - Tracks camera updates and memory clears.
  - Computes an ECEF camera probe from WGS84 coordinates.

- `android/app/build.gradle.kts`
  - Android app Gradle config.
  - Restricts native ABI to `arm64-v8a`.

- `android/app/src/main/AndroidManifest.xml`
  - Adds `android.permission.INTERNET`.

## Channel Contract

The per-view channel name is:

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
flutter pub get
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

Optional Cesium Native package link:

```sh
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

## Manual Validation Checklist

1. Launch on Android.
2. Confirm satellite imagery appears in the native view.
3. Confirm the top stats overlay updates once per second.
4. Move east/west/north/south and confirm the lon/lat text changes and the native map moves.
5. Change zoom and confirm tile labels switch to the new `z/x/y`.
6. Tap the memory icon and confirm cache drops/reloads.
7. Enable Orbit and confirm the native view continuously invalidates while fps remains visible.
8. Check logcat for crashes or obvious tile load exceptions.

## Known Limitations

- Cesium Native is not vendored into the repo. The C++/JNI bridge directly links a local Android arm64 build, defaulting to `/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a`.
- Rendering uses Android `Canvas` behind `NativeMapRenderer`, not OpenGL ES, Vulkan, Metal, Filament, or Cesium Native output.
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

1. Add a Cesium-backed renderer/owner beside `GaodeCanvasTileRenderer`.
   - Keep the Flutter channel contract stable.
   - Keep the existing `NativeMapRenderer` interface small unless Cesium Native needs a concrete new lifecycle event.

2. Expand the existing C++ bridge from lifecycle probe to Cesium Native ownership.
   - Do not expose Cesium Native C++ APIs directly to Dart.
   - Keep the stable JNI adapter owned by Android native code.

3. Integrate Cesium Native for tile selection/loading.
   - Use Cesium Native for tileset lifecycle, camera update, tile selection, metadata, and resource loading.
   - Keep GPU upload/rendering as a separate native renderer concern.

4. Add real performance instrumentation.
   - Android `dumpsys gfxinfo`.
   - Memory snapshots with `dumpsys meminfo`.
   - Optional Perfetto trace for jank and networking.

5. Add lifecycle hardening.
   - Pause/resume behavior.
   - Surface recreation.
   - Network failures.
   - Backpressure for tile requests.
   - Explicit native resource ownership.

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
- Prefer small, verifiable steps. This project exists to reduce risk before the heavier Cesium Native integration.
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

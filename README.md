# Cesium Native Android PoC

This is a small Android-first Flutter probe for validating the rendering shell before adding Cesium Native.

AI continuation defaults to the low-token handoff flow:

- [docs/PROJECT_INDEX.md](/Users/ldy/Desktop/work/cesium_native_android_poc/docs/PROJECT_INDEX.md)

Only open [docs/AI_HANDOFF.md](/Users/ldy/Desktop/work/cesium_native_android_poc/docs/AI_HANDOFF.md) when the short index and recent session log are not enough.

Current scope:

- Flutter app shell and controls.
- Android `PlatformView` embedded through `AndroidView`.
- Native Kotlin `View` that loads and draws Gaode satellite XYZ tiles.
- `NativeMapRenderer` boundary around renderer/cache ownership.
- Android C++/JNI bridge layer reserved for Cesium Native.
- `arm64-v8a`-only Android build.
- Camera sync from Flutter to native view.
- Native tile memory release through the memory button.
- Basic runtime stats: fps, draw time, visible/cached tiles, loaded tiles, failed tiles, cache MB.

## Run

```sh
flutter pub get
flutter run -d <android-device-id>
```

Build a debug APK:

```sh
flutter build apk --debug --target-platform android-arm64
```

## Cesium Native Link

The app now builds `cesium_bridge` against a real Android arm64 Cesium Native build. There is no fallback JNI-only mode.

By default Gradle uses:

```text
/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a
```

To use another Cesium Native Android build root, pass:

```sh
flutter build apk --debug --target-platform android-arm64 \
  --android-project-arg=cesiumNativeRoot=/absolute/path/to/cesium-native/build-android-arm64-v8a
```

That root must contain `install/share/cesium-native/cmake/cesium-nativeConfig.cmake` and `vcpkg_installed/arm64-android`. When it is loaded, the runtime stats bar shows `cesium linked`.

## Validation Checklist

1. Start on Android and confirm the native view displays Gaode satellite imagery.
2. Change zoom with the slider and confirm native tile `z/x/y` labels update.
3. Tap direction buttons and confirm the Flutter lon/lat text and native tile view move together.
4. Tap the memory button and confirm cache drops and tiles reload.
5. Watch the top stats overlay for fps, draw time, tile loads, failures, and cache size.

## Notes

- Cesium Native is linked from the local build root above, not vendored into this repo. Use `-PcesiumNativeRoot=...` to point at a different Android arm64 build.
- The Gaode tile endpoint is used directly for development validation. Confirm licensing, quota, key, and production endpoint requirements before shipping.

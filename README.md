# Cesium Native Android PoC

This is a small Android-first Flutter probe for validating a Cesium Native rendering stack on Android.

AI continuation defaults to the low-token handoff flow:

- [docs/PROJECT_INDEX.md](/Users/ldy/Desktop/work/cesium_native_android_poc/docs/PROJECT_INDEX.md)
- [docs/SDK_ANDROID_QUICKSTART.md](/Users/ldy/Desktop/work/cesium_native_android_poc/docs/SDK_ANDROID_QUICKSTART.md)

Only open [docs/AI_HANDOFF.md](/Users/ldy/Desktop/work/cesium_native_android_poc/docs/AI_HANDOFF.md) when the short index and recent session log are not enough.

Current scope:

- Flutter SDK adapter package and demo app shell.
- Independent native Android demo app.
- Android `PlatformView` embedded through `AndroidView`.
- Native Kotlin `TextureView` host with an EGL render thread.
- Android C++/JNI bridge layer that directly owns the Cesium Native renderer path.
- Formal source split started under `sdk/android-sdk` and `sdk/native-core`.
- Flutter adapter package now lives under `sdk/flutter-plugin`.
- `arm64-v8a`-only Android build.
- Camera sync from Flutter to native view.
- Native tile memory release through the memory button.
- Basic runtime stats: fps, draw time, visible/cached tiles, loaded tiles, failed tiles, cache MB.

## Run

From the Flutter demo root:

```sh
cd examples/flutter-demo
flutter pub get
flutter run -d <android-device-id>
```

Build a debug APK:

```sh
cd examples/flutter-demo
flutter build apk --debug --target-platform android-arm64
```

Launch the minimal native Android SDK example page:

```sh
adb shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkMapActivity

adb shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkFragmentActivity
```

## Cesium Native Link

The app now builds `cesium_bridge` against a real Android arm64 Cesium Native build. There is no fallback JNI-only mode.

By default Gradle uses:

```text
/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a
```

To use another Cesium Native Android build root, pass:

```sh
cd examples/flutter-demo
flutter build apk --debug --target-platform android-arm64 \
  --android-project-arg=cesiumNativeRoot=/absolute/path/to/cesium-native/build-android-arm64-v8a
```

That root must contain `install/share/cesium-native/cmake/cesium-nativeConfig.cmake` and `vcpkg_installed/arm64-android`. When it is loaded, the runtime stats bar shows `cesium linked`.

## Validation Checklist

1. Start on Android and confirm the native view presents the Cesium-backed globe output.
2. Change zoom and confirm the Flutter controls continue to drive the native camera.
3. Tap direction buttons and confirm the Flutter lon/lat text and native camera move together.
4. Tap the memory button and confirm native memory stats react without crashing.
5. Watch the top stats overlay for fps, draw time, tile loads, and cache or GPU memory changes.

## Notes

- Cesium Native is linked from the local build root above, not vendored into this repo. Use `-PcesiumNativeRoot=...` to point at a different Android arm64 build.
- The current demo rendering path is `Flutter demo -> Android PlatformView/TextureView/EGL -> C++ cesium_bridge`.
- The active Android SDK host code now lives under `sdk/android-sdk`, and the active C++ core now lives under `sdk/native-core`.
- The active Flutter adapter code now lives under `sdk/flutter-plugin`, and `examples/flutter-demo` is now a package consumer instead of holding the SDK implementation inline.
- Android PlatformView registration is now handled by the Flutter plugin automatically instead of demo `MainActivity` manual wiring.
- Native Android SDK validation pages now live in `examples/android-native-demo`, so Android host verification is no longer mixed into the Flutter demo shell.
- The Gaode tile endpoint is used directly for development validation. Confirm licensing, quota, key, and production endpoint requirements before shipping.

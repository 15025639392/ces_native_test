# Cesium Native Android Renderer Backend

This repository hosts an Android renderer backend for `cesium-native`.

The core boundary is documented in
[docs/RENDERER_BACKEND.md](/Users/ldy/Desktop/work/cesium_native_android_poc/docs/RENDERER_BACKEND.md).

## Layout

- `sdk/native-core`: C++ JNI bridge, Cesium Native tileset ownership, GPU resource preparation, and OpenGL drawing.
- `sdk/android-sdk`: Android `CesiumMapView` / `CesiumMapFragment` host layer.
- `sdk/flutter-plugin`: Flutter adapter over the Android SDK.
- `examples/android-native-demo`: native Android validation app.
- `examples/flutter-demo`: Flutter consumer demo.

## Build

The native bridge links a local Android arm64 build of `cesium-native`.

Default build root:

```text
/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a
```

Build the native Android demo:

```sh
cd examples/android-native-demo
./gradlew :app:assembleDebug
```

Build the Flutter demo:

```sh
cd examples/flutter-demo
flutter build apk --debug --target-platform android-arm64
```

Override the Cesium Native build root when needed:

```sh
./gradlew :app:assembleDebug -PcesiumNativeRoot=/absolute/path/to/cesium-native/build-android-arm64-v8a
```

## Runtime Rule

Do not add local tile selection, imagery tile selection, parent fallback, or
raster attachment management here. Those belong to `cesium-native`.

The host backend provides `ViewState`, implements renderer resource hooks, and
draws the tiles returned by `cesium-native`.

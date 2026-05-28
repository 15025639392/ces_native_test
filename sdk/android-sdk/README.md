# Android SDK Host

This module is the Android host layer for the Cesium Native renderer backend.

It owns:

- `CesiumMapView`
- `CesiumMapFragment`
- lifecycle forwarding to the internal render surface
- camera, gesture, performance, imagery URL-template, and stats API

It does not own tile selection, imagery tile selection, raster fallback, or
raster attachment lifecycle. Those are delegated to `cesium-native` through
`sdk/native-core`.

Build:

```sh
./gradlew assemble
```

Use `-PcesiumNativeRoot=/absolute/path/to/build-android-arm64-v8a` to point at
a different Cesium Native Android build.

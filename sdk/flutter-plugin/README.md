# Flutter Adapter

This package adapts the Android SDK into Flutter.

It owns:

- `CesiumMapWidget`
- `CesiumMapController`
- `AndroidView` and `MethodChannel` bridging
- Platform view registration for `cesium_map_view`

It does not own Cesium Native lifecycle, tile selection, imagery selection, or
GPU rendering. Those stay in `sdk/android-sdk` and `sdk/native-core`.

package com.example.cesiumpoc.cesium_native_android_poc

/**
 * WGS84 camera contract shared by Android hosts and the Flutter bridge.
 *
 * `altitudeMeters` is height above the WGS84 ellipsoid. Tile selection and
 * raster overlay management remain inside cesium-native; the host only passes
 * camera state into the renderer backend.
 */
data class CesiumCameraState(
    val longitude: Double,
    val latitude: Double,
    val altitudeMeters: Double,
    val autoOrbit: Boolean,
    val bearing: Double = 0.0,
    val pitch: Double = 0.0,
)

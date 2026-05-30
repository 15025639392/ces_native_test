package com.example.cesiumpoc.cesium_native_android_poc

/**
 * WGS84 camera contract shared by Android hosts and the Flutter bridge.
 *
 * `longitude` and `latitude` are the WGS84 view target. `altitudeMeters` is
 * the camera range from that target in meters. Tile selection and raster
 * overlay management remain inside cesium-native; the host only passes camera
 * state into the renderer backend.
 */
data class CesiumCameraState(
    val longitude: Double,
    val latitude: Double,
    val altitudeMeters: Double,
    val autoOrbit: Boolean,
    val bearing: Double = 0.0,
    val pitch: Double = 0.0,
)

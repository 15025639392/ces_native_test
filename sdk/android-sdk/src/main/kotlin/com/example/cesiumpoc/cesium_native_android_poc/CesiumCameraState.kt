package com.example.cesiumpoc.cesium_native_android_poc

/**
 * V1 camera contract shared by Android hosts and the Flutter bridge.
 *
 * The current native renderer still mainly consumes lon/lat/zoom plus a
 * simple orbit flag. `bearing` and `pitch` are included now so the SDK can
 * evolve toward a fuller camera model without another contract break.
 */
data class CesiumCameraState(
    val longitude: Double,
    val latitude: Double,
    val zoom: Double,
    val autoOrbit: Boolean,
    val bearing: Double = 0.0,
    val pitch: Double = 0.0,
)

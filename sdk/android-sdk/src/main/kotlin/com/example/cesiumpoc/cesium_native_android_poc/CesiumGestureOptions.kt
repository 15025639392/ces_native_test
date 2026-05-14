package com.example.cesiumpoc.cesium_native_android_poc

/**
 * V1 gesture configuration surface.
 *
 * Pan, pinch zoom, double-tap zoom, optional inertia, two-finger rotate,
 * and two-finger tilt now map to the Android host gesture layer.
 */
data class CesiumGestureOptions(
    val panEnabled: Boolean = true,
    val zoomEnabled: Boolean = true,
    val rotateEnabled: Boolean = false,
    val tiltEnabled: Boolean = false,
    val inertiaEnabled: Boolean = false,
)

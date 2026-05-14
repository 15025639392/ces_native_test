package com.example.cesiumpoc.cesium_native_android_poc

/**
 * Minimal Android callback surface for V1 hosts.
 */
interface CesiumMapListener {
    fun onMapReady() = Unit

    fun onCameraMoveStarted(state: CesiumCameraState) = Unit

    fun onCameraChanged(state: CesiumCameraState) = Unit

    fun onCameraIdle(state: CesiumCameraState) = Unit

    fun onRenderStats(stats: CesiumRenderStats) = Unit

    fun onError(error: CesiumMapError) = Unit
}

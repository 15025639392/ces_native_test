package com.example.cesiumpoc.cesium_native_android_poc

/**
 * Minimal shared control surface for Android hosts in V1.
 *
 * `CesiumMapView` implements this directly, and `CesiumMapFragment` exposes
 * the same contract through `fragment.controller`.
 */
interface CesiumMapController {
    var cameraState: CesiumCameraState
    var interactionEnabled: Boolean
    var gestureOptions: CesiumGestureOptions
    var performanceOptions: CesiumPerformanceOptions

    fun setCamera(camera: CesiumCameraState) {
        cameraState = camera
    }

    fun clearMemory()

    fun getStats(): CesiumRenderStats

    fun setListener(listener: CesiumMapListener?)
}

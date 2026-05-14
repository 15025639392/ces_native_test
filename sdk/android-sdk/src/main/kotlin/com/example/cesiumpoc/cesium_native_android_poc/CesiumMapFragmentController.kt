package com.example.cesiumpoc.cesium_native_android_poc

/**
 * Adapter that exposes `CesiumMapFragment` through the same control contract
 * used by direct `CesiumMapView` hosts.
 */
class CesiumMapFragmentController(
    private val fragment: CesiumMapFragment,
) : CesiumMapController {
    override var cameraState: CesiumCameraState
        get() = fragment.cameraState ?: CesiumCameraState(
            longitude = 116.397389,
            latitude = 39.908722,
            zoom = 15.0,
            autoOrbit = false,
        )
        set(value) {
            fragment.setCamera(value)
        }

    override var interactionEnabled: Boolean
        get() = fragment.interactionEnabled
        set(value) {
            fragment.setInteractionEnabled(value)
        }

    override var gestureOptions: CesiumGestureOptions
        get() = fragment.gestureOptions
        set(value) {
            fragment.setGestureOptions(value)
        }

    override var performanceOptions: CesiumPerformanceOptions
        get() = fragment.performanceOptions
        set(value) {
            fragment.setPerformanceOptions(value)
        }

    override fun clearMemory() {
        fragment.clearMemory()
    }

    override fun getStats(): CesiumRenderStats {
        return fragment.getStats()
    }

    override fun setListener(listener: CesiumMapListener?) {
        fragment.setListener(listener)
    }
}

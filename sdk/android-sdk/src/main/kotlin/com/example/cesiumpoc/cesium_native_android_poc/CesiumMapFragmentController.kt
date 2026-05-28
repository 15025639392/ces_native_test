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
            longitude = 104.0,
            latitude = 35.0,
            altitudeMeters = 3_535_534.0,
            autoOrbit = false,
            pitch = 35.0,
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

    override var imagerySource: ImagerySource?
        get() = fragment.imagerySource
        set(value) {
            fragment.setImagerySource(value)
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

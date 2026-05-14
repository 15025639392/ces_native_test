package com.example.cesiumpoc.cesium_native_android_poc

import android.content.Context
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.widget.FrameLayout
import com.example.cesiumpoc.cesium_native_android_poc.internal.render.CesiumMapRenderSurface

/**
 * Primary Android SDK host view for the current V1 surface.
 *
 * This wraps the internal render surface, owns host-facing listener dispatch,
 * and exposes the smallest control and lifecycle surface that is already
 * grounded in the current runtime implementation.
 */
class CesiumMapView(
    context: Context,
) : FrameLayout(context), CesiumMapController {
    private val renderSurface = CesiumMapRenderSurface(context)
    private var listener: CesiumMapListener? = null

    internal var onMapReady: (() -> Unit)?
        get() = null
        set(value) {
            onMapReadyCallback = value
        }

    internal var onCameraChanged: ((CesiumCameraState) -> Unit)?
        get() = null
        set(value) {
            onCameraChangedCallback = value
        }

    internal var onCameraMoveStarted: ((CesiumCameraState) -> Unit)?
        get() = null
        set(value) {
            onCameraMoveStartedCallback = value
        }

    internal var onCameraIdle: ((CesiumCameraState) -> Unit)?
        get() = null
        set(value) {
            onCameraIdleCallback = value
        }

    internal var onStats: ((CesiumRenderStats) -> Unit)?
        get() = null
        set(value) {
            onStatsCallback = value
        }

    internal var onError: ((code: String, message: String, details: String?) -> Unit)?
        get() = null
        set(value) {
            onErrorCallback = value
        }

    private var onMapReadyCallback: (() -> Unit)? = null
    private var onCameraMoveStartedCallback: ((CesiumCameraState) -> Unit)? = null
    private var onCameraChangedCallback: ((CesiumCameraState) -> Unit)? = null
    private var onCameraIdleCallback: ((CesiumCameraState) -> Unit)? = null
    private var onStatsCallback: ((CesiumRenderStats) -> Unit)? = null
    private var onErrorCallback: ((code: String, message: String, details: String?) -> Unit)? = null

    override var cameraState: CesiumCameraState
        get() = renderSurface.cameraState
        set(value) {
            renderSurface.setCamera(value)
        }

    override var interactionEnabled: Boolean
        get() = renderSurface.interactionEnabled
        set(value) {
            renderSurface.interactionEnabled = value
        }

    override var gestureOptions: CesiumGestureOptions
        get() = renderSurface.gestureOptions
        set(value) {
            renderSurface.gestureOptions = value
        }

    override var performanceOptions: CesiumPerformanceOptions
        get() = renderSurface.performanceOptions
        set(value) {
            renderSurface.performanceOptions = value
        }

    init {
        addView(renderSurface, LayoutParams(MATCH_PARENT, MATCH_PARENT))
        renderSurface.onMapReady = {
            listener?.onMapReady()
            onMapReadyCallback?.invoke()
        }
        renderSurface.onCameraMoveStarted = { camera ->
            listener?.onCameraMoveStarted(camera)
            onCameraMoveStartedCallback?.invoke(camera)
        }
        renderSurface.onCameraChanged = { camera ->
            listener?.onCameraChanged(camera)
            onCameraChangedCallback?.invoke(camera)
        }
        renderSurface.onCameraIdle = { camera ->
            listener?.onCameraIdle(camera)
            onCameraIdleCallback?.invoke(camera)
        }
        renderSurface.onStats = { stats ->
            listener?.onRenderStats(stats)
            onStatsCallback?.invoke(stats)
        }
        renderSurface.onError = { code, message, details ->
            val error = CesiumMapError(code = code, message = message, details = details)
            listener?.onError(error)
            onErrorCallback?.invoke(code, message, details)
        }
    }

    override fun setListener(listener: CesiumMapListener?) {
        this.listener = listener
    }

    override fun setCamera(camera: CesiumCameraState) {
        renderSurface.setCamera(camera)
    }

    fun onCreate() = Unit

    fun onStart() = Unit

    fun onResume() {
        renderSurface.setHostActive(true)
    }

    fun onPause() {
        renderSurface.setHostActive(false)
    }

    fun onStop() = Unit

    fun onDestroy() {
        release()
    }

    fun onLowMemory() {
        clearMemory()
    }

    override fun clearMemory() {
        renderSurface.clearMemory()
    }

    fun release() {
        renderSurface.release()
    }

    override fun getStats(): CesiumRenderStats = renderSurface.snapshotStats()
}

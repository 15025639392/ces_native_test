package com.example.cesiumpoc.cesium_native_android_poc

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment

/**
 * Fragment host entry for Android SDK consumers.
 *
 * This keeps the fragment shape very small and delegates the real map host
 * work to `CesiumMapView`.
 */
class CesiumMapFragment : Fragment() {
    private var mapView: CesiumMapView? = null
    private var listener: CesiumMapListener? = null
    private var pendingCamera: CesiumCameraState? = null
    private var pendingInteractionEnabled: Boolean = true
    private var pendingGestureOptions: CesiumGestureOptions = CesiumGestureOptions()
    private var pendingPerformanceOptions: CesiumPerformanceOptions = CesiumPerformanceOptions()
    val controller: CesiumMapController = CesiumMapFragmentController(this)

    val cameraState: CesiumCameraState?
        get() = mapView?.cameraState ?: pendingCamera
    val interactionEnabled: Boolean
        get() = mapView?.interactionEnabled ?: pendingInteractionEnabled
    val gestureOptions: CesiumGestureOptions
        get() = mapView?.gestureOptions ?: pendingGestureOptions
    val performanceOptions: CesiumPerformanceOptions
        get() = mapView?.performanceOptions ?: pendingPerformanceOptions

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        pendingCamera = arguments?.toCameraState()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val view = CesiumMapView(requireContext())
        mapView = view
        view.onCreate()
        view.setListener(listener)
        view.interactionEnabled = pendingInteractionEnabled
        view.gestureOptions = pendingGestureOptions
        view.performanceOptions = pendingPerformanceOptions
        pendingCamera?.let(view::setCamera)
        return view
    }

    override fun onStart() {
        super.onStart()
        mapView?.onStart()
    }

    override fun onResume() {
        super.onResume()
        mapView?.onResume()
    }

    override fun onPause() {
        mapView?.onPause()
        super.onPause()
    }

    override fun onStop() {
        mapView?.onStop()
        super.onStop()
    }

    override fun onLowMemory() {
        super.onLowMemory()
        mapView?.onLowMemory()
    }

    override fun onDestroyView() {
        mapView?.onDestroy()
        mapView = null
        super.onDestroyView()
    }

    fun setListener(listener: CesiumMapListener?) {
        this.listener = listener
        mapView?.setListener(listener)
    }

    fun setCamera(camera: CesiumCameraState) {
        pendingCamera = camera
        mapView?.setCamera(camera)
    }

    fun setInteractionEnabled(enabled: Boolean) {
        pendingInteractionEnabled = enabled
        mapView?.interactionEnabled = enabled
    }

    fun setGestureOptions(options: CesiumGestureOptions) {
        pendingGestureOptions = options
        mapView?.gestureOptions = options
    }

    fun setPerformanceOptions(options: CesiumPerformanceOptions) {
        pendingPerformanceOptions = options
        mapView?.performanceOptions = options
    }

    fun clearMemory() {
        mapView?.clearMemory()
    }

    fun getStats(): CesiumRenderStats {
        return mapView?.getStats() ?: CesiumRenderStats()
    }

    companion object {
        private const val ARG_LONGITUDE = "longitude"
        private const val ARG_LATITUDE = "latitude"
        private const val ARG_ZOOM = "zoom"
        private const val ARG_AUTO_ORBIT = "autoOrbit"
        private const val ARG_BEARING = "bearing"
        private const val ARG_PITCH = "pitch"

        fun newInstance(initialCamera: CesiumCameraState? = null): CesiumMapFragment {
            return CesiumMapFragment().apply {
                arguments = Bundle().apply {
                    initialCamera?.let {
                        putDouble(ARG_LONGITUDE, it.longitude)
                        putDouble(ARG_LATITUDE, it.latitude)
                        putDouble(ARG_ZOOM, it.zoom)
                        putBoolean(ARG_AUTO_ORBIT, it.autoOrbit)
                        putDouble(ARG_BEARING, it.bearing)
                        putDouble(ARG_PITCH, it.pitch)
                    }
                }
            }
        }

        private fun Bundle.toCameraState(): CesiumCameraState? {
            if (!containsKey(ARG_LONGITUDE)) return null
            return CesiumCameraState(
                longitude = getDouble(ARG_LONGITUDE),
                latitude = getDouble(ARG_LATITUDE),
                zoom = getDouble(ARG_ZOOM),
                autoOrbit = getBoolean(ARG_AUTO_ORBIT, false),
                bearing = getDouble(ARG_BEARING, 0.0),
                pitch = getDouble(ARG_PITCH, 0.0),
            )
        }
    }
}

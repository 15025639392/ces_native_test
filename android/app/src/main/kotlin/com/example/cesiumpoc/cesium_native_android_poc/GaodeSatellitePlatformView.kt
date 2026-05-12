package com.example.cesiumpoc.cesium_native_android_poc

import android.content.Context
import android.graphics.SurfaceTexture
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLSurface
import android.os.Handler
import android.os.Looper
import android.view.TextureView
import android.view.View
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.platform.PlatformView
import java.util.concurrent.atomic.AtomicBoolean

class GaodeSatellitePlatformView(
    context: Context,
    messenger: BinaryMessenger,
    viewId: Int,
    params: Map<*, *>,
) : PlatformView, MethodChannel.MethodCallHandler {
    private val globeView = CesiumGpuGlobeView(context)
    private val channel = MethodChannel(messenger, "gaode_satellite_view_$viewId")

    init {
        channel.setMethodCallHandler(this)
        globeView.updateCamera(
            longitude = params.doubleValue("longitude", 116.397389),
            latitude = params.doubleValue("latitude", 39.908722),
            zoom = params.doubleValue("zoom", 15.0),
            autoOrbit = params.boolValue("autoOrbit", false),
        )
        globeView.onStats = { channel.invokeMethod("stats", it) }
    }

    override fun getView(): View = globeView

    override fun dispose() {
        channel.setMethodCallHandler(null)
        globeView.release()
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "updateCamera" -> {
                val args = call.arguments as? Map<*, *> ?: emptyMap<Any, Any>()
                globeView.updateCamera(
                    longitude = args.doubleValue("longitude", globeView.longitude),
                    latitude = args.doubleValue("latitude", globeView.latitude),
                    zoom = args.doubleValue("zoom", globeView.zoom),
                    autoOrbit = args.boolValue("autoOrbit", globeView.autoOrbit),
                )
                result.success(null)
            }
            "clearMemory" -> {
                globeView.clearMemory()
                result.success(null)
            }
            else -> result.notImplemented()
        }
    }
}

private class CesiumGpuGlobeView(context: Context) :
    TextureView(context),
    TextureView.SurfaceTextureListener {
    @Volatile
    var longitude: Double = 116.397389
        private set
    @Volatile
    var latitude: Double = 39.908722
        private set
    @Volatile
    var zoom: Double = 15.0
        private set
    @Volatile
    var autoOrbit: Boolean = false
        private set
    var onStats: ((Map<String, Any>) -> Unit)? = null

    private val mainHandler = Handler(Looper.getMainLooper())
    private val cesiumBridge = NativeCesiumBridge()
    private val released = AtomicBoolean(false)
    private var renderThread: EglRenderThread? = null
    private var widthPx = 1
    private var heightPx = 1
    private var frameCount = 0
    private var fps = 0.0
    private var lastFrameNanos = 0L
    private var lastStatsNanos = 0L
    private var lastOrbitNanos = 0L

    init {
        isOpaque = true
        surfaceTextureListener = this
    }

    fun updateCamera(longitude: Double, latitude: Double, zoom: Double, autoOrbit: Boolean) {
        this.longitude = longitude.coerceIn(-180.0, 180.0)
        this.latitude = latitude.coerceIn(-85.0, 85.0)
        this.zoom = zoom.coerceIn(3.0, 19.0)
        this.autoOrbit = autoOrbit
        val camera = NativeCameraState(this.longitude, this.latitude, this.zoom, this.autoOrbit)
        cesiumBridge.updateCamera(camera.longitude, camera.latitude, camera.zoom, camera.autoOrbit)
    }

    fun clearMemory() {
        cesiumBridge.clearMemory()
        emitStats()
    }

    fun release() {
        if (released.compareAndSet(false, true)) {
            renderThread?.requestStop()
            renderThread = null
            cesiumBridge.release()
            onStats = null
        }
    }

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        widthPx = width.coerceAtLeast(1)
        heightPx = height.coerceAtLeast(1)
        renderThread = EglRenderThread(
            surface,
            widthPx,
            heightPx,
            onReady = {
                cesiumBridge.onSurfaceCreated()
                cesiumBridge.onSurfaceChanged(widthPx, heightPx)
                cesiumBridge.updateCamera(longitude, latitude, zoom, autoOrbit)
            },
            drawFrame = { drawFrame() },
        ).also { it.start() }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        widthPx = width.coerceAtLeast(1)
        heightPx = height.coerceAtLeast(1)
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        renderThread?.requestStop()
        renderThread = null
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) = Unit

    private fun drawFrame() {
        if (released.get()) return

        val now = System.nanoTime()
        val dt = if (lastFrameNanos == 0L) 0.0 else (now - lastFrameNanos) / 1_000_000_000.0
        lastFrameNanos = now
        frameCount++

        if (autoOrbit) {
            if (lastOrbitNanos != 0L) {
                val orbitDt = (now - lastOrbitNanos) / 1_000_000_000.0
                longitude = wrapLongitude(longitude + orbitDt * 0.015)
                cesiumBridge.updateCamera(longitude, latitude, zoom, autoOrbit)
            }
            lastOrbitNanos = now
        } else {
            lastOrbitNanos = 0L
        }

        cesiumBridge.renderFrame(widthPx, heightPx, dt)

        if (lastStatsNanos == 0L) {
            lastStatsNanos = now
        }
        val statsElapsed = now - lastStatsNanos
        if (statsElapsed >= 1_000_000_000L) {
            fps = frameCount * 1_000_000_000.0 / statsElapsed
            frameCount = 0
            lastStatsNanos = now
            emitStats()
        }
    }

    private fun emitStats() {
        val stats = cesiumBridge.snapshot()
        val payload = mapOf(
            "fps" to fps,
            "drawMs" to stats.drawMs,
            "visibleTiles" to stats.selectedTiles,
            "cachedTiles" to stats.selectedTiles,
            "loadedTiles" to stats.loadedTiles,
            "failedTiles" to 0,
            "cacheMb" to stats.gpuMemoryMb,
            "cesiumBackend" to stats.backend,
            "cesiumLinked" to stats.linked,
            "cesiumCameraUpdates" to stats.cameraUpdates,
            "cesiumMemoryClears" to stats.memoryClears,
            "cesiumFrameUpdates" to stats.frameUpdates,
            "cesiumCameraDirty" to stats.cameraDirty,
            "cesiumFrameDt" to stats.lastDeltaSeconds,
            "cesiumEcefX" to stats.ecefX,
            "cesiumEcefY" to stats.ecefY,
            "cesiumEcefZ" to stats.ecefZ,
        )
        mainHandler.post { onStats?.invoke(payload) }
    }

    private fun wrapLongitude(value: Double): Double {
        var lon = value
        while (lon > 180.0) lon -= 360.0
        while (lon < -180.0) lon += 360.0
        return lon
    }
}

private class EglRenderThread(
    private val surfaceTexture: SurfaceTexture,
    private val initialWidth: Int,
    private val initialHeight: Int,
    private val onReady: () -> Unit,
    private val drawFrame: () -> Unit,
) : Thread("CesiumGpuRenderer") {
    private val running = AtomicBoolean(true)

    fun requestStop() {
        running.set(false)
        join(500)
    }

    override fun run() {
        val display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        EGL14.eglInitialize(display, null, 0, null, 0)
        val config = chooseConfig(display)
        val context = EGL14.eglCreateContext(
            display,
            config,
            EGL14.EGL_NO_CONTEXT,
            intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 3, EGL14.EGL_NONE),
            0,
        )
        val surface = EGL14.eglCreateWindowSurface(
            display,
            config,
            surfaceTexture,
            intArrayOf(EGL14.EGL_NONE),
            0,
        )
        EGL14.eglMakeCurrent(display, surface, surface, context)

        try {
            onReady()
            while (running.get()) {
                drawFrame()
                EGL14.eglSwapBuffers(display, surface)
                sleep(16)
            }
        } finally {
            EGL14.eglMakeCurrent(
                display,
                EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_CONTEXT,
            )
            EGL14.eglDestroySurface(display, surface)
            EGL14.eglDestroyContext(display, context)
            EGL14.eglTerminate(display)
        }
    }

    private fun chooseConfig(display: EGLDisplay): EGLConfig {
        val configs = arrayOfNulls<EGLConfig>(1)
        val count = IntArray(1)
        EGL14.eglChooseConfig(
            display,
            intArrayOf(
                EGL14.EGL_RENDERABLE_TYPE, 0x40,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_WINDOW_BIT,
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_DEPTH_SIZE, 0,
                EGL14.EGL_NONE,
            ),
            0,
            configs,
            0,
            configs.size,
            count,
            0,
        )
        return requireNotNull(configs[0]) {
            "Unable to choose EGL config for $initialWidth x $initialHeight"
        }
    }
}

private fun Map<*, *>.doubleValue(key: String, fallback: Double): Double {
    return (this[key] as? Number)?.toDouble() ?: fallback
}

private fun Map<*, *>.boolValue(key: String, fallback: Boolean): Boolean {
    return this[key] as? Boolean ?: fallback
}

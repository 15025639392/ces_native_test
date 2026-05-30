package com.example.cesiumpoc.cesium_native_android_poc.internal.render

import android.content.Context
import android.graphics.SurfaceTexture
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLDisplay
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.TextureView
import android.view.ViewConfiguration
import com.example.cesiumpoc.cesium_native_android_poc.CesiumCameraState
import com.example.cesiumpoc.cesium_native_android_poc.CesiumGestureOptions
import com.example.cesiumpoc.cesium_native_android_poc.ImagerySource
import com.example.cesiumpoc.cesium_native_android_poc.CesiumPerformanceOptions
import com.example.cesiumpoc.cesium_native_android_poc.CesiumRenderStats
import com.example.cesiumpoc.cesium_native_android_poc.NativeCesiumBridge
import com.example.cesiumpoc.cesium_native_android_poc.QuantizedMeshTerrainSource
import com.example.cesiumpoc.cesium_native_android_poc.TerrainSource
import com.example.cesiumpoc.cesium_native_android_poc.UrlTemplateImagerySource
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.max
import kotlin.math.pow

internal class CesiumMapRenderSurface(
    context: Context,
) : TextureView(context), TextureView.SurfaceTextureListener {
    private enum class CameraChangeSource {
        PROGRAMMATIC,
        GESTURE,
        AUTO_ORBIT,
        INERTIA,
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val cesiumBridge = NativeCesiumBridge()
    private val released = AtomicBoolean(false)
    private val readyEmitted = AtomicBoolean(false)
    private val activeFrameIntervalNanos = 16_666_667L
    private val backgroundPollIntervalNanos = 250_000_000L
    private val minFlingVelocity = ViewConfiguration.get(context).scaledMinimumFlingVelocity.toDouble()
    private val cameraChangedThrottleMs = 33L
    private val cameraIdleDelayMs = 280L
    private val panSensitivity = 0.68
    private val doubleTapAltitudeScale = 0.58
    private val rotateSensitivity = 0.55
    private val tiltSensitivity = 0.06
    private val minGesturePanDistancePx = 0.75
    private val minRotateDeltaDegrees = 0.18
    private val minTiltDeltaDegrees = 0.12
    private val scaleGestureDetector =
        ScaleGestureDetector(
            context,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScaleBegin(detector: ScaleGestureDetector): Boolean {
                    if (!interactionEnabled || !gestureOptions.zoomEnabled) return false
                    stopInertia()
                    return true
                }

                override fun onScale(detector: ScaleGestureDetector): Boolean {
                    if (!interactionEnabled || !gestureOptions.zoomEnabled) return false
                    applyZoomScale(
                        scaleFactor = detector.scaleFactor.toDouble(),
                        focusX = detector.focusX.toDouble(),
                        focusY = detector.focusY.toDouble(),
                    )
                    return true
                }
            },
        )
    private val gestureDetector =
        GestureDetector(
            context,
            object : GestureDetector.SimpleOnGestureListener() {
                override fun onDown(e: MotionEvent): Boolean {
                    stopInertia()
                    return interactionEnabled
                }

                override fun onScroll(
                    e1: MotionEvent?,
                    e2: MotionEvent,
                    distanceX: Float,
                    distanceY: Float,
                ): Boolean {
                    if (
                        !interactionEnabled ||
                        !gestureOptions.panEnabled ||
                        scaleGestureDetector.isInProgress ||
                        e2.pointerCount > 1
                    ) {
                        return false
                    }
                    applyPan(
                        currentX = e2.x.toDouble(),
                        currentY = e2.y.toDouble(),
                        distanceX = distanceX.toDouble(),
                        distanceY = distanceY.toDouble(),
                        source = CameraChangeSource.GESTURE,
                    )
                    return true
                }

                override fun onDoubleTap(e: MotionEvent): Boolean {
                    if (!interactionEnabled || !gestureOptions.zoomEnabled) return false
                    stopInertia()
                    applyZoomDelta(1.0, e.x.toDouble(), e.y.toDouble())
                    return true
                }

                override fun onFling(
                    e1: MotionEvent?,
                    e2: MotionEvent,
                    velocityX: Float,
                    velocityY: Float,
                ): Boolean {
                    if (!interactionEnabled || !gestureOptions.panEnabled || !gestureOptions.inertiaEnabled) {
                        return false
                    }
                    val speed = max(abs(velocityX), abs(velocityY)).toDouble()
                    if (speed < minFlingVelocity) return false
                    startInertia(velocityX.toDouble(), velocityY.toDouble())
                    return true
                }
            },
        )

    @Volatile
    private var longitude: Double = 104.0
    @Volatile
    private var latitude: Double = 35.0
    @Volatile
    private var altitudeMeters: Double = 3_535_534.0
    @Volatile
    private var autoOrbit: Boolean = false
    @Volatile
    private var bearing: Double = 0.0
    @Volatile
    private var pitch: Double = 35.0

    var interactionEnabled: Boolean = true
        set(value) {
            field = value
            isEnabled = value
            isClickable = value
            isLongClickable = value
        }
    var gestureOptions: CesiumGestureOptions = CesiumGestureOptions()
    var performanceOptions: CesiumPerformanceOptions = CesiumPerformanceOptions()
        set(value) {
            field = value
            applyEffectiveMaximumScreenSpaceError()
        }
    var imagerySource: ImagerySource? = null
        set(value) {
            field = value
            applyImagerySource(force = true)
        }
    var terrainSource: TerrainSource? = null
        set(value) {
            field = value
            applyTerrainSource(force = true)
        }

    var onStats: ((CesiumRenderStats) -> Unit)? = null
    var onMapReady: (() -> Unit)? = null
    var onCameraMoveStarted: ((CesiumCameraState) -> Unit)? = null
    var onCameraChanged: ((CesiumCameraState) -> Unit)? = null
    var onCameraIdle: ((CesiumCameraState) -> Unit)? = null
    var onError: ((code: String, message: String, details: String?) -> Unit)? = null

    private var renderThread: EglRenderThread? = null
    @Volatile
    private var hostActive = true
    @Volatile
    private var attachedToHostWindow = false
    @Volatile
    private var hostWindowVisible = false
    private var widthPx = 1
    private var heightPx = 1
    private var frameCount = 0
    private var fps = 0.0
    private var appliedMaximumScreenSpaceError = Double.NaN
    private var lastFrameNanos = 0L
    private var lastStatsNanos = 0L
    private var lastOrbitNanos = 0L
    private var isCameraMoving = false
    private var pendingCameraChanged: CesiumCameraState? = null
    private var lastCameraChangedDispatchUptimeMs = 0L
    private var inertiaVelocityX = 0.0
    private var inertiaVelocityY = 0.0
    private var inertiaLastFrameNanos = 0L
    private var inertiaRunning = false
    private var multiTouchTracking = false
    private var firstMultiTouchPointerId = MotionEvent.INVALID_POINTER_ID
    private var secondMultiTouchPointerId = MotionEvent.INVALID_POINTER_ID
    private var lastMultiTouchAngleDegrees = 0.0
    private var lastMultiTouchFocusY = 0.0
    private var lastMultiTouchFirstY = 0.0
    private var lastMultiTouchSecondY = 0.0
    private val cameraChangedRunnable =
        Runnable {
            pendingCameraChanged?.let(::dispatchCameraChangedNow)
        }
    private val cameraIdleRunnable =
        Runnable {
            isCameraMoving = false
            cesiumBridge.setCameraMoving(false)
            applyEffectiveMaximumScreenSpaceError()
            onCameraIdle?.invoke(cameraState)
        }
    private val inertiaRunnable =
        object : Runnable {
            override fun run() {
                if (!inertiaRunning || released.get() || !interactionEnabled || !gestureOptions.inertiaEnabled) {
                    inertiaRunning = false
                    return
                }
                val now = System.nanoTime()
                val dt = if (inertiaLastFrameNanos == 0L) 0.016 else (now - inertiaLastFrameNanos) / 1_000_000_000.0
                inertiaLastFrameNanos = now
                applyPan(
                    currentX = widthPx * 0.5,
                    currentY = heightPx * 0.5,
                    distanceX = -inertiaVelocityX * dt,
                    distanceY = -inertiaVelocityY * dt,
                    source = CameraChangeSource.INERTIA,
                )
                val decay = 0.84.pow(dt / 0.016)
                inertiaVelocityX *= decay
                inertiaVelocityY *= decay
                if (max(abs(inertiaVelocityX), abs(inertiaVelocityY)) < 18.0) {
                    inertiaRunning = false
                    scheduleCameraIdle()
                    return
                }
                mainHandler.postDelayed(this, 16L)
            }
        }

    init {
        isOpaque = true
        isClickable = true
        surfaceTextureListener = this
    }

    val cameraState: CesiumCameraState
        get() = CesiumCameraState(longitude, latitude, altitudeMeters, autoOrbit, bearing, pitch)

    fun setCamera(camera: CesiumCameraState) {
        applyCamera(camera, CameraChangeSource.PROGRAMMATIC)
    }

    fun setHostActive(active: Boolean) {
        hostActive = active
        if (!active) {
            resetFrameCounters()
        }
    }

    fun clearMemory() {
        cesiumBridge.clearMemory()
        emitStats()
    }

    fun release() {
        if (released.compareAndSet(false, true)) {
            stopInertia()
            mainHandler.removeCallbacks(cameraChangedRunnable)
            mainHandler.removeCallbacks(cameraIdleRunnable)
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
            surfaceTexture = surface,
            initialWidth = widthPx,
            initialHeight = heightPx,
            onReady = {
                cesiumBridge.onSurfaceCreated()
                cesiumBridge.onSurfaceChanged(widthPx, heightPx)
                applyEffectiveMaximumScreenSpaceError(force = true)
                applyImagerySource(force = true)
                applyTerrainSource(force = true)
                val camera = cameraState
                syncLocalCamera(
                    cesiumBridge.updateCamera(
                        camera.longitude,
                        camera.latitude,
                        camera.altitudeMeters,
                        camera.autoOrbit,
                        camera.bearing,
                        camera.pitch,
                    ),
                )
                if (readyEmitted.compareAndSet(false, true)) {
                    mainHandler.post { onMapReady?.invoke() }
                }
            },
            drawFrame = { drawFrame() },
            frameIntervalNanos = { currentFrameIntervalNanos() },
            onError = { throwable ->
                emitError(
                    code = "egl_render_thread_failed",
                    message = throwable.message ?: "EGL render thread failed.",
                )
            },
        ).also { it.start() }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        widthPx = width.coerceAtLeast(1)
        heightPx = height.coerceAtLeast(1)
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        renderThread?.requestStop()
        renderThread = null
        readyEmitted.set(false)
        resetFrameCounters()
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) = Unit

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        attachedToHostWindow = true
        hostWindowVisible = windowVisibility == VISIBLE
    }

    override fun onDetachedFromWindow() {
        attachedToHostWindow = false
        hostWindowVisible = false
        resetFrameCounters()
        super.onDetachedFromWindow()
    }

    override fun onWindowVisibilityChanged(visibility: Int) {
        super.onWindowVisibilityChanged(visibility)
        hostWindowVisible = visibility == VISIBLE
        if (!hostWindowVisible) {
            resetFrameCounters()
        }
    }

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (!interactionEnabled) return false
        return super.dispatchTouchEvent(event)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!interactionEnabled) return false
        val scaleHandled = scaleGestureDetector.onTouchEvent(event)
        val twoFingerHandled = handleTwoFingerGesture(event)
        val gestureHandled = gestureDetector.onTouchEvent(event)
        if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
            stopInertia()
            resetMultiTouchTracking()
            scheduleCameraIdle()
        }
        if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_POINTER_UP) {
            if (event.pointerCount <= 2) {
                resetMultiTouchTracking()
            }
            performClick()
            if (!inertiaRunning) {
                scheduleCameraIdle()
            }
        }
        return scaleHandled || twoFingerHandled || gestureHandled || super.onTouchEvent(event)
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    fun snapshotStats(): CesiumRenderStats {
        val stats = cesiumBridge.snapshot()
        return CesiumRenderStats(
            fps = fps,
            drawMs = stats.drawMs,
            visibleTiles = stats.selectedTiles,
            cachedTiles = stats.selectedTiles,
            loadedTiles = stats.loadedTiles,
            failedTiles = 0,
            cacheMb = stats.gpuMemoryMb,
            cesiumBackend = stats.backend,
            cesiumLinked = stats.linked,
            cesiumCameraUpdates = stats.cameraUpdates,
            cesiumMemoryClears = stats.memoryClears,
            cesiumFrameUpdates = stats.frameUpdates,
            cesiumCameraDirty = stats.cameraDirty,
            cesiumFrameDt = stats.lastDeltaSeconds,
            cesiumEcefX = stats.ecefX,
            cesiumEcefY = stats.ecefY,
            cesiumEcefZ = stats.ecefZ,
        )
    }

    private fun drawFrame(): Boolean {
        if (released.get()) return false
        if (!shouldRenderFrame()) {
            resetFrameCounters()
            return false
        }

        val now = System.nanoTime()
        val dt = if (lastFrameNanos == 0L) 0.0 else (now - lastFrameNanos) / 1_000_000_000.0
        lastFrameNanos = now
        frameCount++

        if (autoOrbit) {
            if (lastOrbitNanos != 0L) {
                val orbitDt = (now - lastOrbitNanos) / 1_000_000_000.0
                adoptNativeCamera(
                    cesiumBridge.orbitCamera(orbitDt, 0.015),
                    CameraChangeSource.AUTO_ORBIT,
                )
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
        return true
    }

    private fun emitStats() {
        mainHandler.post { onStats?.invoke(snapshotStats()) }
    }

    private fun applyImagerySource(force: Boolean = false) {
        if (!force && imagerySource == null) return
        val urlTemplate = (imagerySource as? UrlTemplateImagerySource)?.urlTemplate
        cesiumBridge.setImageryUrlTemplate(urlTemplate)
    }

    private fun applyTerrainSource(force: Boolean = false) {
        if (!force && terrainSource == null) return
        val layerJsonUrl = (terrainSource as? QuantizedMeshTerrainSource)?.layerJsonUrl
        cesiumBridge.setTerrainLayerJsonUrl(layerJsonUrl)
    }

    private fun emitError(code: String, message: String, details: String? = null) {
        mainHandler.post { onError?.invoke(code, message, details) }
    }

    private fun applyPan(
        currentX: Double,
        currentY: Double,
        distanceX: Double,
        distanceY: Double,
        source: CameraChangeSource,
    ) {
        if (!gestureOptions.panEnabled) return
        val pixelDistance = kotlin.math.hypot(distanceX, distanceY)
        if (pixelDistance < minGesturePanDistancePx) return
        val nativeCamera = cesiumBridge.panCamera(
            currentX = currentX,
            currentY = currentY,
            distanceX = distanceX,
            distanceY = distanceY,
            sensitivity = panSensitivity,
        )
        adoptNativeCamera(nativeCamera, source)
    }

    private fun applyZoomScale(scaleFactor: Double, focusX: Double, focusY: Double) {
        if (!gestureOptions.zoomEnabled || scaleFactor <= 0.0) return
        applyAltitudeScale(1.0 / scaleFactor, focusX, focusY)
    }

    private fun handleTwoFingerGesture(event: MotionEvent): Boolean {
        if (event.pointerCount < 2) return false
        if (!gestureOptions.rotateEnabled && !gestureOptions.tiltEnabled) return false

        if (
            !multiTouchTracking ||
            event.actionMasked == MotionEvent.ACTION_POINTER_DOWN ||
            event.actionMasked == MotionEvent.ACTION_DOWN
        ) {
            beginMultiTouchTracking(event)
            return false
        }

        val firstPointerIndex = event.findPointerIndex(firstMultiTouchPointerId)
        val secondPointerIndex = event.findPointerIndex(secondMultiTouchPointerId)
        if (firstPointerIndex < 0 || secondPointerIndex < 0) {
            beginMultiTouchTracking(event)
            return false
        }

        val firstX = event.getX(firstPointerIndex).toDouble()
        val firstY = event.getY(firstPointerIndex).toDouble()
        val secondX = event.getX(secondPointerIndex).toDouble()
        val secondY = event.getY(secondPointerIndex).toDouble()
        val angleDegrees = Math.toDegrees(atan2(secondY - firstY, secondX - firstX))
        val focusX = (firstX + secondX) * 0.5
        val focusY = (firstY + secondY) * 0.5

        if (event.actionMasked != MotionEvent.ACTION_MOVE) return false

        var changed = false

        if (gestureOptions.rotateEnabled) {
            val bearingDelta = shortestAngleDelta(lastMultiTouchAngleDegrees, angleDegrees) * rotateSensitivity
            if (abs(bearingDelta) > minRotateDeltaDegrees) {
                adoptNativeCamera(cesiumBridge.rotateCamera(bearingDelta, focusX, focusY), CameraChangeSource.GESTURE)
                changed = true
            }
        }

        if (gestureOptions.tiltEnabled) {
            val firstYDelta = firstY - lastMultiTouchFirstY
            val secondYDelta = secondY - lastMultiTouchSecondY
            val focusYDelta = if (firstYDelta * secondYDelta > 0.0) {
                (firstYDelta + secondYDelta) * 0.5
            } else {
                focusY - lastMultiTouchFocusY
            }
            val pitchDelta = -focusYDelta * tiltSensitivity
            if (abs(pitchDelta) > minTiltDeltaDegrees) {
                adoptNativeCamera(cesiumBridge.tiltCamera(pitchDelta, focusX, focusY), CameraChangeSource.GESTURE)
                changed = true
            }
        }

        lastMultiTouchAngleDegrees = angleDegrees
        lastMultiTouchFocusY = focusY
        lastMultiTouchFirstY = firstY
        lastMultiTouchSecondY = secondY

        return changed
    }

    private fun beginMultiTouchTracking(event: MotionEvent) {
        if (event.pointerCount < 2) {
            resetMultiTouchTracking()
            return
        }
        firstMultiTouchPointerId = event.getPointerId(0)
        secondMultiTouchPointerId = event.getPointerId(1)
        val firstX = event.getX(0).toDouble()
        val firstY = event.getY(0).toDouble()
        val secondX = event.getX(1).toDouble()
        val secondY = event.getY(1).toDouble()
        multiTouchTracking = true
        lastMultiTouchAngleDegrees = Math.toDegrees(atan2(secondY - firstY, secondX - firstX))
        lastMultiTouchFocusY = (firstY + secondY) * 0.5
        lastMultiTouchFirstY = firstY
        lastMultiTouchSecondY = secondY
        stopInertia()
    }

    private fun applyZoomDelta(delta: Double, focusX: Double? = null, focusY: Double? = null) {
        if (!gestureOptions.zoomEnabled || delta == 0.0) return
        applyAltitudeScale(doubleTapAltitudeScale.pow(delta), focusX, focusY)
    }

    private fun applyAltitudeScale(scale: Double, focusX: Double? = null, focusY: Double? = null) {
        if (!gestureOptions.zoomEnabled || scale <= 0.0) return
        val nativeCamera = if (focusX != null && focusY != null) {
            cesiumBridge.scaleCamera(scale, focusX, focusY)
        } else {
            cesiumBridge.scaleCameraFromCenter(scale)
        }
        adoptNativeCamera(nativeCamera, CameraChangeSource.GESTURE)
    }

    private fun applyCamera(camera: CesiumCameraState, source: CameraChangeSource) {
        if (source == CameraChangeSource.PROGRAMMATIC || source == CameraChangeSource.GESTURE) {
            stopInertia()
        }
        val nativeCamera = cesiumBridge.updateCamera(
            camera.longitude,
            camera.latitude,
            camera.altitudeMeters,
            camera.autoOrbit,
            camera.bearing,
            camera.pitch,
        )
        adoptNativeCamera(nativeCamera, source)
    }

    private fun adoptNativeCamera(camera: CesiumCameraState, source: CameraChangeSource) {
        syncLocalCamera(camera)
        onCameraUpdated(cameraState, source)
    }

    private fun syncLocalCamera(camera: CesiumCameraState) {
        longitude = camera.longitude
        latitude = camera.latitude
        altitudeMeters = camera.altitudeMeters
        autoOrbit = camera.autoOrbit
        bearing = camera.bearing
        pitch = camera.pitch
    }

    private fun onCameraUpdated(camera: CesiumCameraState, source: CameraChangeSource) {
        if (!isCameraMoving) {
            isCameraMoving = true
            cesiumBridge.setCameraMoving(true)
            mainHandler.post { onCameraMoveStarted?.invoke(camera) }
        }
        applyEffectiveMaximumScreenSpaceError()
        pendingCameraChanged = camera
        scheduleThrottledCameraChanged(camera)
        if (source == CameraChangeSource.AUTO_ORBIT && camera.autoOrbit) {
            mainHandler.removeCallbacks(cameraIdleRunnable)
        } else {
            scheduleCameraIdle()
        }
    }

    private fun scheduleThrottledCameraChanged(camera: CesiumCameraState) {
        val now = SystemClock.uptimeMillis()
        val elapsed = now - lastCameraChangedDispatchUptimeMs
        if (elapsed >= cameraChangedThrottleMs) {
            dispatchCameraChangedNow(camera)
            return
        }
        pendingCameraChanged = camera
        mainHandler.removeCallbacks(cameraChangedRunnable)
        mainHandler.postDelayed(cameraChangedRunnable, cameraChangedThrottleMs - elapsed)
    }

    private fun dispatchCameraChangedNow(camera: CesiumCameraState) {
        pendingCameraChanged = camera
        lastCameraChangedDispatchUptimeMs = SystemClock.uptimeMillis()
        mainHandler.post { onCameraChanged?.invoke(camera) }
    }

    private fun scheduleCameraIdle() {
        mainHandler.removeCallbacks(cameraIdleRunnable)
        mainHandler.postDelayed(cameraIdleRunnable, cameraIdleDelayMs)
    }

    private fun applyEffectiveMaximumScreenSpaceError(force: Boolean = false) {
        val target = effectiveMaximumScreenSpaceError()
        if (!force && abs(appliedMaximumScreenSpaceError - target) < 0.0001) return
        appliedMaximumScreenSpaceError = target
        cesiumBridge.setMaximumScreenSpaceError(target)
    }

    private fun effectiveMaximumScreenSpaceError(): Double {
        return performanceOptions.maximumScreenSpaceError
    }

    private fun startInertia(velocityX: Double, velocityY: Double) {
        stopInertia()
        inertiaVelocityX = velocityX
        inertiaVelocityY = velocityY
        inertiaLastFrameNanos = 0L
        inertiaRunning = true
        mainHandler.post(inertiaRunnable)
    }

    private fun stopInertia() {
        inertiaRunning = false
        inertiaVelocityX = 0.0
        inertiaVelocityY = 0.0
        inertiaLastFrameNanos = 0L
        mainHandler.removeCallbacks(inertiaRunnable)
    }

    private fun resetMultiTouchTracking() {
        multiTouchTracking = false
        firstMultiTouchPointerId = MotionEvent.INVALID_POINTER_ID
        secondMultiTouchPointerId = MotionEvent.INVALID_POINTER_ID
        lastMultiTouchAngleDegrees = 0.0
        lastMultiTouchFocusY = 0.0
        lastMultiTouchFirstY = 0.0
        lastMultiTouchSecondY = 0.0
    }

    private fun shouldRenderFrame(): Boolean {
        return hostActive &&
            attachedToHostWindow &&
            hostWindowVisible &&
            isShown &&
            widthPx > 0 &&
            heightPx > 0
    }

    private fun currentFrameIntervalNanos(): Long {
        if (!shouldRenderFrame()) return backgroundPollIntervalNanos
        if (autoOrbit || inertiaRunning || isCameraMoving) return activeFrameIntervalNanos
        return cesiumBridge.recommendedFrameIntervalNanos().coerceAtLeast(activeFrameIntervalNanos / 4)
    }

    private fun resetFrameCounters() {
        lastFrameNanos = 0L
        lastOrbitNanos = 0L
        lastStatsNanos = 0L
        frameCount = 0
        fps = 0.0
    }

    private fun shortestAngleDelta(fromDegrees: Double, toDegrees: Double): Double {
        var delta = toDegrees - fromDegrees
        while (delta > 180.0) delta -= 360.0
        while (delta < -180.0) delta += 360.0
        return delta
    }
}

private class EglRenderThread(
    private val surfaceTexture: SurfaceTexture,
    private val initialWidth: Int,
    private val initialHeight: Int,
    private val onReady: () -> Unit,
    private val drawFrame: () -> Boolean,
    private val frameIntervalNanos: () -> Long,
    private val onError: (Throwable) -> Unit,
) : Thread("CesiumGpuRenderer") {
    private val running = AtomicBoolean(true)
    private val minimumFrameIntervalNanos = 4_000_000L

    fun requestStop() {
        running.set(false)
        join(500)
    }

    override fun run() {
        try {
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
                    val targetFrameNanos = frameIntervalNanos().coerceAtLeast(minimumFrameIntervalNanos)
                    val frameStartNanos = System.nanoTime()
                    val rendered = drawFrame()
                    if (rendered) {
                        EGL14.eglSwapBuffers(display, surface)
                    }
                    val frameElapsedNanos = System.nanoTime() - frameStartNanos
                    val remainingNanos = targetFrameNanos - frameElapsedNanos
                    if (remainingNanos > 0L) {
                        sleep(remainingNanos / 1_000_000L, (remainingNanos % 1_000_000L).toInt())
                    } else {
                        Thread.yield()
                    }
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
        } catch (t: Throwable) {
            onError(t)
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
                EGL14.EGL_DEPTH_SIZE, 24,
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

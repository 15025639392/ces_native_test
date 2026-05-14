package com.example.cesiumpoc.cesium_native_android_poc

internal class NativeCesiumBridge {
    private var nativeHandle: Long = nativeCreate()

    val isCesiumNativeLinked: Boolean
        get() = nativeIsCesiumNativeLinked()

    val backendName: String
        get() = nativeBackendName()

    fun updateCamera(
        longitude: Double,
        latitude: Double,
        zoom: Double,
        autoOrbit: Boolean,
        bearing: Double,
        pitch: Double,
    ) {
        if (nativeHandle == 0L) return
        nativeUpdateCamera(nativeHandle, longitude, latitude, zoom, autoOrbit, bearing, pitch)
    }

    fun setMaximumScreenSpaceError(maximumScreenSpaceError: Double) {
        if (nativeHandle == 0L) return
        nativeSetMaximumScreenSpaceError(nativeHandle, maximumScreenSpaceError)
    }

    fun onSurfaceCreated() {
        if (nativeHandle == 0L) return
        nativeOnSurfaceCreated(nativeHandle)
    }

    fun onSurfaceChanged(width: Int, height: Int) {
        if (nativeHandle == 0L) return
        nativeOnSurfaceChanged(nativeHandle, width, height)
    }

    fun renderFrame(width: Int, height: Int, deltaSeconds: Double) {
        if (nativeHandle == 0L) return
        nativeRenderFrame(nativeHandle, width, height, deltaSeconds)
    }

    fun recommendedFrameIntervalNanos(): Long {
        if (nativeHandle == 0L) return 16_666_667L
        return nativeRecommendedFrameIntervalNanos(nativeHandle)
    }

    fun clearMemory() {
        if (nativeHandle == 0L) return
        nativeClearMemory(nativeHandle)
    }

    fun snapshot(): CesiumBridgeStats {
        if (nativeHandle == 0L) return CesiumBridgeStats()
        val ecef = nativeCameraEcef(nativeHandle)
        return CesiumBridgeStats(
            backend = backendName,
            linked = isCesiumNativeLinked,
            cameraUpdates = nativeCameraUpdates(nativeHandle),
            memoryClears = nativeMemoryClears(nativeHandle),
            frameUpdates = nativeFrameUpdates(nativeHandle),
            cameraDirty = nativeCameraDirty(nativeHandle),
            lastDeltaSeconds = nativeLastDeltaSeconds(nativeHandle),
            selectedTiles = nativeSelectedTiles(nativeHandle),
            loadedTiles = nativeLoadedTiles(nativeHandle),
            drawMs = nativeDrawMs(nativeHandle),
            gpuMemoryMb = nativeGpuMemoryMb(nativeHandle),
            ecefX = ecef.getOrElse(0) { 0.0 },
            ecefY = ecef.getOrElse(1) { 0.0 },
            ecefZ = ecef.getOrElse(2) { 0.0 },
        )
    }

    fun release() {
        val handle = nativeHandle
        if (handle == 0L) return
        nativeHandle = 0L
        nativeDestroy(handle)
    }

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeUpdateCamera(
        handle: Long,
        longitude: Double,
        latitude: Double,
        zoom: Double,
        autoOrbit: Boolean,
        bearing: Double,
        pitch: Double,
    )
    private external fun nativeOnSurfaceCreated(handle: Long)
    private external fun nativeOnSurfaceChanged(handle: Long, width: Int, height: Int)
    private external fun nativeRenderFrame(handle: Long, width: Int, height: Int, deltaSeconds: Double)
    private external fun nativeRecommendedFrameIntervalNanos(handle: Long): Long
    private external fun nativeSetMaximumScreenSpaceError(handle: Long, maximumScreenSpaceError: Double)
    private external fun nativeClearMemory(handle: Long)
    private external fun nativeIsCesiumNativeLinked(): Boolean
    private external fun nativeBackendName(): String
    private external fun nativeCameraEcef(handle: Long): DoubleArray
    private external fun nativeCameraUpdates(handle: Long): Long
    private external fun nativeMemoryClears(handle: Long): Long
    private external fun nativeFrameUpdates(handle: Long): Long
    private external fun nativeCameraDirty(handle: Long): Boolean
    private external fun nativeLastDeltaSeconds(handle: Long): Double
    private external fun nativeSelectedTiles(handle: Long): Int
    private external fun nativeLoadedTiles(handle: Long): Int
    private external fun nativeDrawMs(handle: Long): Double
    private external fun nativeGpuMemoryMb(handle: Long): Double

    companion object {
        init {
            System.loadLibrary("cesium_bridge")
        }
    }
}

internal data class CesiumBridgeStats(
    val backend: String = "released",
    val linked: Boolean = false,
    val cameraUpdates: Long = 0,
    val memoryClears: Long = 0,
    val frameUpdates: Long = 0,
    val cameraDirty: Boolean = false,
    val lastDeltaSeconds: Double = 0.0,
    val selectedTiles: Int = 0,
    val loadedTiles: Int = 0,
    val drawMs: Double = 0.0,
    val gpuMemoryMb: Double = 0.0,
    val ecefX: Double = 0.0,
    val ecefY: Double = 0.0,
    val ecefZ: Double = 0.0,
)

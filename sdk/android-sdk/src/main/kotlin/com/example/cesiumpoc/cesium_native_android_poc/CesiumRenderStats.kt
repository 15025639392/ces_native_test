package com.example.cesiumpoc.cesium_native_android_poc

/**
 * Public render and runtime stats payload for Android and Flutter hosts.
 */
data class CesiumRenderStats(
    val fps: Double = 0.0,
    val drawMs: Double = 0.0,
    val visibleTiles: Int = 0,
    val cachedTiles: Int = 0,
    val loadedTiles: Int = 0,
    val failedTiles: Int = 0,
    val cacheMb: Double = 0.0,
    val cesiumBackend: String = "released",
    val cesiumLinked: Boolean = false,
    val cesiumCameraUpdates: Long = 0,
    val cesiumMemoryClears: Long = 0,
    val cesiumFrameUpdates: Long = 0,
    val cesiumCameraDirty: Boolean = false,
    val cesiumFrameDt: Double = 0.0,
    val cesiumEcefX: Double = 0.0,
    val cesiumEcefY: Double = 0.0,
    val cesiumEcefZ: Double = 0.0,
)

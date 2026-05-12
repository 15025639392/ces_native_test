package com.example.cesiumpoc.cesium_native_android_poc

import android.graphics.Canvas

internal data class NativeCameraState(
    val longitude: Double,
    val latitude: Double,
    val zoom: Double,
    val autoOrbit: Boolean,
)

internal data class NativeRenderStats(
    val drawMs: Double = 0.0,
    val visibleTiles: Int = 0,
    val cachedTiles: Int = 0,
    val loadedTiles: Int = 0,
    val failedTiles: Int = 0,
    val cacheMb: Double = 0.0,
)

internal interface NativeMapRenderer {
    val stats: NativeRenderStats

    fun updateCamera(camera: NativeCameraState)
    fun draw(canvas: Canvas, width: Int, height: Int)
    fun clearMemory()
    fun release()
}

package com.example.cesiumpoc.cesium_native_android_poc

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Handler
import android.os.Looper
import android.util.LruCache
import java.net.HttpURLConnection
import java.net.URL
import java.util.Collections
import java.util.Locale
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.PI
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.ln
import kotlin.math.pow
import kotlin.math.tan

internal class GaodeCanvasTileRenderer(
    private val invalidateView: () -> Unit,
) : NativeMapRenderer {
    override var stats = NativeRenderStats()
        private set

    private val mainHandler = Handler(Looper.getMainLooper())
    private val executor: ExecutorService = Executors.newFixedThreadPool(4)
    private val pendingLoads = Collections.synchronizedSet(mutableSetOf<String>())
    private val disposed = AtomicBoolean(false)
    private val tileSize = 256
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 24f
        setShadowLayer(3f, 1f, 1f, Color.BLACK)
    }
    private val missingTilePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(28, 35, 32)
    }
    private val gridPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(80, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 1f
    }
    private val tileCache = object : LruCache<String, Bitmap>(64 * 1024 * 1024) {
        override fun sizeOf(key: String, value: Bitmap): Int = value.byteCount

        override fun entryRemoved(
            evicted: Boolean,
            key: String,
            oldValue: Bitmap,
            newValue: Bitmap?,
        ) {
            if (newValue == null && !oldValue.isRecycled) {
                oldValue.recycle()
            }
        }
    }

    private var camera = NativeCameraState(
        longitude = 116.397389,
        latitude = 39.908722,
        zoom = 15.0,
        autoOrbit = false,
    )
    private var loadedTiles = 0
    private var failedTiles = 0

    override fun updateCamera(camera: NativeCameraState) {
        this.camera = camera
    }

    override fun draw(canvas: Canvas, width: Int, height: Int) {
        val start = System.nanoTime()
        val z = camera.zoom.toInt()
        val scale = 2.0.pow(z)
        val centerWorldX = lonToTileX(camera.longitude, z) * tileSize
        val centerWorldY = latToTileY(camera.latitude, z) * tileSize
        val leftWorld = centerWorldX - width / 2.0
        val topWorld = centerWorldY - height / 2.0
        val startX = floor(leftWorld / tileSize).toInt()
        val endX = ceil((leftWorld + width) / tileSize).toInt()
        val startY = floor(topWorld / tileSize).toInt()
        val endY = ceil((topWorld + height) / tileSize).toInt()
        val maxTile = scale.toInt()
        var visible = 0

        for (x in startX..endX) {
            for (y in startY..endY) {
                if (y !in 0 until maxTile) continue
                val wrappedX = ((x % maxTile) + maxTile) % maxTile
                val key = "$z/$wrappedX/$y"
                val drawX = (x * tileSize - leftWorld).toFloat()
                val drawY = (y * tileSize - topWorld).toFloat()
                val bitmap = tileCache.get(key)
                if (bitmap != null && !bitmap.isRecycled) {
                    canvas.drawBitmap(bitmap, drawX, drawY, null)
                } else {
                    canvas.drawRect(
                        drawX,
                        drawY,
                        drawX + tileSize,
                        drawY + tileSize,
                        missingTilePaint,
                    )
                    requestTile(key, z, wrappedX, y)
                }
                canvas.drawRect(drawX, drawY, drawX + tileSize, drawY + tileSize, gridPaint)
                canvas.drawText(key, drawX + 8f, drawY + 28f, labelPaint)
                visible++
            }
        }

        stats = NativeRenderStats(
            drawMs = (System.nanoTime() - start) / 1_000_000.0,
            visibleTiles = visible,
            cachedTiles = tileCache.snapshot().size,
            loadedTiles = loadedTiles,
            failedTiles = failedTiles,
            cacheMb = tileCache.size() / 1024.0 / 1024.0,
        )
    }

    override fun clearMemory() {
        pendingLoads.clear()
        tileCache.evictAll()
        stats = stats.copy(cachedTiles = 0, cacheMb = 0.0)
    }

    override fun release() {
        if (disposed.compareAndSet(false, true)) {
            pendingLoads.clear()
            tileCache.evictAll()
            executor.shutdownNow()
        }
    }

    private fun requestTile(key: String, z: Int, x: Int, y: Int) {
        if (pendingLoads.contains(key) || disposed.get()) return
        pendingLoads.add(key)
        executor.execute {
            try {
                val server = ((x + y) % 4) + 1
                val url = URL(
                    String.format(
                        Locale.US,
                        "https://webst0%d.is.autonavi.com/appmaptile?style=6&x=%d&y=%d&z=%d",
                        server,
                        x,
                        y,
                        z,
                    ),
                )
                val connection = (url.openConnection() as HttpURLConnection).apply {
                    connectTimeout = 6000
                    readTimeout = 6000
                    requestMethod = "GET"
                    setRequestProperty("User-Agent", "GaodeTileProbe/0.1 Android")
                }
                connection.inputStream.use { stream ->
                    val bitmap = BitmapFactory.decodeStream(stream)
                    if (bitmap != null && !disposed.get()) {
                        tileCache.put(key, bitmap)
                        loadedTiles++
                        mainHandler.post { invalidateView() }
                    } else {
                        failedTiles++
                    }
                }
                connection.disconnect()
            } catch (_: Exception) {
                failedTiles++
            } finally {
                pendingLoads.remove(key)
            }
        }
    }

    private fun lonToTileX(lon: Double, z: Int): Double {
        return (lon + 180.0) / 360.0 * 2.0.pow(z)
    }

    private fun latToTileY(lat: Double, z: Int): Double {
        val latRad = Math.toRadians(lat)
        return (1.0 - ln(tan(latRad) + 1.0 / kotlin.math.cos(latRad)) / PI) / 2.0 * 2.0.pow(z)
    }
}

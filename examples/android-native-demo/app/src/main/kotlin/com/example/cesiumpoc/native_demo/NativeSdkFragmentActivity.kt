package com.example.cesiumpoc.native_demo

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.FragmentActivity
import com.example.cesiumpoc.cesium_native_android_poc.CesiumCameraState
import com.example.cesiumpoc.cesium_native_android_poc.CesiumMapError
import com.example.cesiumpoc.cesium_native_android_poc.CesiumMapFragment
import com.example.cesiumpoc.cesium_native_android_poc.CesiumMapListener
import com.example.cesiumpoc.cesium_native_android_poc.CesiumPerformanceOptions
import com.example.cesiumpoc.cesium_native_android_poc.CesiumRenderStats
import com.example.cesiumpoc.cesium_native_android_poc.QuantizedMeshTerrainSource
import com.example.cesiumpoc.cesium_native_android_poc.UrlTemplateImagerySource

class NativeSdkFragmentActivity : FragmentActivity() {
    private val imagerySource =
        UrlTemplateImagerySource(
            id = "osm",
            urlTemplate = "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
        )
    private val terrainSource =
        QuantizedMeshTerrainSource(
            id = "swisstopo_terrain",
            layerJsonUrl = "https://3d.geo.admin.ch/ch.swisstopo.terrain.3d/v1/20250101/layer.json",
        )

    private val fragment =
        CesiumMapFragment.newInstance(
            CesiumCameraState(
                longitude = 7.75,
                latitude = 46.02,
                altitudeMeters = 80_000.0,
                autoOrbit = false,
                bearing = 25.0,
                pitch = 55.0,
            ),
        )

    private lateinit var statusView: TextView
    private lateinit var cameraView: TextView
    private lateinit var statsView: TextView
    private lateinit var performanceView: TextView

    private val mainHandler = Handler(Looper.getMainLooper())
    private var maximumScreenSpaceError = 4.0
    private val fragmentContainerId = View.generateViewId()
    private val statsTicker =
        object : Runnable {
            override fun run() {
                renderStats(fragment.controller.getStats())
                mainHandler.postDelayed(this, 1000)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildContentView())
        fragment.controller.imagerySource = imagerySource
        fragment.controller.terrainSource = terrainSource

        fragment.setListener(
            object : CesiumMapListener {
                override fun onMapReady() {
                    runOnUiThread { statusView.text = "原生 SDK Fragment 页面已就绪" }
                }

                override fun onCameraChanged(state: CesiumCameraState) {
                    runOnUiThread {
                        renderCamera(state)
                    }
                }

                override fun onRenderStats(stats: CesiumRenderStats) {
                    runOnUiThread { renderStats(stats) }
                }

                override fun onError(error: CesiumMapError) {
                    runOnUiThread { statusView.text = "错误: ${error.message}" }
                }
            },
        )

        if (savedInstanceState == null) {
            supportFragmentManager.beginTransaction()
                .replace(fragmentContainerId, fragment)
                .commitNow()
        }
    }

    override fun onResume() {
        super.onResume()
        mainHandler.post(statsTicker)
    }

    override fun onPause() {
        mainHandler.removeCallbacks(statsTicker)
        super.onPause()
    }

    private fun buildContentView(): FrameLayout {
        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }

        root.addView(
            FrameLayout(this).apply { id = fragmentContainerId },
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            ),
        )

        val topPanel =
            LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setBackgroundColor(0xCC101513.toInt())
                setPadding(dp(12), dp(10), dp(12), dp(10))
            }

        statusView = metricText("原生 SDK Fragment 页面初始化中")
        cameraView = metricText("camera --")
        statsView = metricText("stats --")
        performanceView = metricText("detail --")
        topPanel.addView(statusView)
        topPanel.addView(cameraView)
        topPanel.addView(statsView)
        topPanel.addView(performanceView)

        root.addView(
            topPanel,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.TOP,
            ).apply { setMargins(dp(12), dp(12), dp(12), 0) },
        )

        renderCamera(fragment.controller.cameraState)
        renderPerformance()
        renderStats(fragment.controller.getStats())
        return root
    }

    private fun renderCamera(camera: CesiumCameraState) {
        cameraView.text =
            "camera  lon ${camera.longitude.format(5)}  lat ${camera.latitude.format(5)}  alt ${(camera.altitudeMeters / 1000.0).format(1)}km  bearing ${camera.bearing.format(1)}  pitch ${camera.pitch.format(1)}"
    }

    private fun renderStats(stats: CesiumRenderStats) {
        statsView.text =
            "stats  fps ${stats.fps.format(1)}  draw ${stats.drawMs.format(1)}ms  tiles ${stats.visibleTiles}/${stats.cachedTiles}  cache ${stats.cacheMb.format(1)}MB"
    }

    private fun renderPerformance() {
        performanceView.text = "detail  mse ${maximumScreenSpaceError.format(1)}  terrain ${terrainSource.id}"
    }

    private fun metricText(text: String): TextView {
        return TextView(this).apply {
            this.text = text
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun Double.format(digits: Int): String = "%.${digits}f".format(this)
}

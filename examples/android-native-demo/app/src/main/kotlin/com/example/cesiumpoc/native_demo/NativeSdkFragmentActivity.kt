package com.example.cesiumpoc.native_demo

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
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

class NativeSdkFragmentActivity : FragmentActivity() {
    private val fragment =
        CesiumMapFragment.newInstance(
            CesiumCameraState(
                longitude = 116.397389,
                latitude = 39.908722,
                zoom = 15.0,
                autoOrbit = false,
            ),
        )

    private lateinit var statusView: TextView
    private lateinit var cameraView: TextView
    private lateinit var statsView: TextView
    private lateinit var performanceView: TextView
    private lateinit var orbitButton: Button
    private lateinit var mse4Button: Button
    private lateinit var mse8Button: Button
    private lateinit var mse12Button: Button

    private val mainHandler = Handler(Looper.getMainLooper())
    private var autoOrbit = false
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

        fragment.setListener(
            object : CesiumMapListener {
                override fun onMapReady() {
                    runOnUiThread { statusView.text = "原生 SDK Fragment 页面已就绪" }
                }

                override fun onCameraChanged(state: CesiumCameraState) {
                    runOnUiThread {
                        autoOrbit = state.autoOrbit
                        renderCamera(state)
                        renderOrbitButton()
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

        val controls =
            LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setBackgroundColor(0xDD17201D.toInt())
                setPadding(dp(16), dp(12), dp(16), dp(16))
            }

        val firstRow =
            LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
            }

        orbitButton =
            actionButton("Orbit") {
                autoOrbit = !autoOrbit
                val current = fragment.controller.cameraState
                fragment.controller.setCamera(current.copy(autoOrbit = autoOrbit))
                renderOrbitButton()
            }

        val clearButton =
            actionButton("清内存") {
                fragment.controller.clearMemory()
                renderStats(fragment.controller.getStats())
            }

        firstRow.addView(orbitButton, linearParams(1f))
        firstRow.addView(spacer())
        firstRow.addView(clearButton, linearParams(1f))

        controls.addView(firstRow)
        controls.addView(rowSpacer())
        controls.addView(buildMseRow())
        controls.addView(rowSpacer())
        controls.addView(buildButtonRow("西", { moveBy(-0.01, 0.0) }, "东", { moveBy(0.01, 0.0) }))
        controls.addView(rowSpacer())
        controls.addView(buildButtonRow("南", { moveBy(0.0, -0.01) }, "北", { moveBy(0.0, 0.01) }))
        controls.addView(rowSpacer())
        controls.addView(buildButtonRow("左转", { rotateBy(-15.0) }, "右转", { rotateBy(15.0) }))
        controls.addView(rowSpacer())
        controls.addView(buildButtonRow("压低", { pitchBy(10.0) }, "抬高", { pitchBy(-10.0) }))
        controls.addView(rowSpacer())
        controls.addView(buildButtonRow("缩小", { zoomBy(-1.0) }, "放大", { zoomBy(1.0) }))

        root.addView(
            topPanel,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.TOP,
            ).apply { setMargins(dp(12), dp(12), dp(12), 0) },
        )

        root.addView(
            controls,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM,
            ),
        )

        renderCamera(fragment.controller.cameraState)
        renderOrbitButton()
        renderPerformance()
        renderStats(fragment.controller.getStats())
        return root
    }

    private fun buildMseRow(): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            mse4Button = actionButton("MSE 4") { setMaximumScreenSpaceError(4.0) }
            mse8Button = actionButton("MSE 8") { setMaximumScreenSpaceError(8.0) }
            mse12Button = actionButton("MSE 12") { setMaximumScreenSpaceError(12.0) }
            addView(mse4Button, linearParams(1f))
            addView(spacer())
            addView(mse8Button, linearParams(1f))
            addView(spacer())
            addView(mse12Button, linearParams(1f))
        }
    }

    private fun buildButtonRow(
        leftLabel: String,
        leftClick: () -> Unit,
        rightLabel: String,
        rightClick: () -> Unit,
    ): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(actionButton(leftLabel, leftClick), linearParams(1f))
            addView(spacer())
            addView(actionButton(rightLabel, rightClick), linearParams(1f))
        }
    }

    private fun moveBy(lonDelta: Double, latDelta: Double) {
        val current = fragment.controller.cameraState
        fragment.controller.setCamera(
            current.copy(
                longitude = (current.longitude + lonDelta).coerceIn(-180.0, 180.0),
                latitude = (current.latitude + latDelta).coerceIn(-85.0, 85.0),
            ),
        )
    }

    private fun zoomBy(delta: Double) {
        val current = fragment.controller.cameraState
        fragment.controller.setCamera(current.copy(zoom = (current.zoom + delta).coerceIn(3.0, 19.0)))
    }

    private fun rotateBy(delta: Double) {
        val current = fragment.controller.cameraState
        fragment.controller.setCamera(current.copy(bearing = current.bearing + delta))
    }

    private fun pitchBy(delta: Double) {
        val current = fragment.controller.cameraState
        fragment.controller.setCamera(current.copy(pitch = (current.pitch + delta).coerceIn(0.0, 85.0)))
    }

    private fun setMaximumScreenSpaceError(value: Double) {
        maximumScreenSpaceError = value
        fragment.controller.performanceOptions = CesiumPerformanceOptions(maximumScreenSpaceError = value)
        renderPerformance()
    }

    private fun renderCamera(camera: CesiumCameraState) {
        cameraView.text =
            "camera  lon ${camera.longitude.format(5)}  lat ${camera.latitude.format(5)}  zoom ${camera.zoom.format(1)}  bearing ${camera.bearing.format(1)}  pitch ${camera.pitch.format(1)}"
    }

    private fun renderStats(stats: CesiumRenderStats) {
        statsView.text =
            "stats  fps ${stats.fps.format(1)}  draw ${stats.drawMs.format(1)}ms  tiles ${stats.visibleTiles}/${stats.cachedTiles}  cache ${stats.cacheMb.format(1)}MB"
    }

    private fun renderPerformance() {
        performanceView.text = "detail  mse ${maximumScreenSpaceError.format(1)}"
        renderMseButtons()
    }

    private fun renderOrbitButton() {
        orbitButton.text = if (autoOrbit) "Orbit 开" else "Orbit 关"
    }

    private fun renderMseButtons() {
        renderMseButton(mse4Button, maximumScreenSpaceError == 4.0)
        renderMseButton(mse8Button, maximumScreenSpaceError == 8.0)
        renderMseButton(mse12Button, maximumScreenSpaceError == 12.0)
    }

    private fun renderMseButton(button: Button, selected: Boolean) {
        button.setBackgroundColor(if (selected) 0xFF0E8F74.toInt() else 0xFF1C6B5A.toInt())
    }

    private fun actionButton(label: String, onClick: () -> Unit): Button {
        return Button(this).apply {
            text = label
            setOnClickListener { onClick() }
            setTextColor(Color.WHITE)
            setBackgroundColor(0xFF1C6B5A.toInt())
            isAllCaps = false
        }
    }

    private fun metricText(text: String): TextView {
        return TextView(this).apply {
            this.text = text
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
        }
    }

    private fun linearParams(weight: Float): LinearLayout.LayoutParams {
        return LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, weight)
    }

    private fun spacer(): TextView {
        return TextView(this).apply {
            layoutParams = LinearLayout.LayoutParams(dp(8), 1)
        }
    }

    private fun rowSpacer(): TextView {
        return TextView(this).apply {
            layoutParams = LinearLayout.LayoutParams(1, dp(8))
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun Double.format(digits: Int): String = "%.${digits}f".format(this)
}

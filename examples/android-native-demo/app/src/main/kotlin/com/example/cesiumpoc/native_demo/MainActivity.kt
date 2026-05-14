package com.example.cesiumpoc.native_demo

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root =
            LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER_HORIZONTAL
                setBackgroundColor(Color.parseColor("#111614"))
                setPadding(dp(24), dp(48), dp(24), dp(24))
            }

        root.addView(
            TextView(this).apply {
                text = "Android Native Demo"
                textSize = 22f
                setTextColor(Color.WHITE)
            },
        )

        root.addView(
            TextView(this).apply {
                text = "直接消费 sdk/android-sdk 的原生验证页"
                textSize = 14f
                setTextColor(Color.parseColor("#B8C3BE"))
                setPadding(0, dp(8), 0, dp(24))
            },
        )

        root.addView(
            launchButton("打开 CesiumMapView 示例") {
                startActivity(Intent(this, NativeSdkMapActivity::class.java))
            },
        )

        root.addView(space())

        root.addView(
            launchButton("打开 CesiumMapFragment 示例") {
                startActivity(Intent(this, NativeSdkFragmentActivity::class.java))
            },
        )

        setContentView(root)
    }

    private fun launchButton(label: String, onClick: () -> Unit): Button {
        return Button(this).apply {
            text = label
            isAllCaps = false
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#1C6B5A"))
            setOnClickListener { onClick() }
            layoutParams =
                LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                )
        }
    }

    private fun space(): TextView {
        return TextView(this).apply {
            layoutParams =
                LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    dp(12),
                )
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()
}

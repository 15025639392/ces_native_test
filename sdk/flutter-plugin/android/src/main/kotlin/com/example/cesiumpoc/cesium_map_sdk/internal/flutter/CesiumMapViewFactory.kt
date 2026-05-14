package com.example.cesiumpoc.cesium_map_sdk.internal.flutter

import android.content.Context
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.StandardMessageCodec
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory

internal class CesiumMapViewFactory(
    private val messenger: BinaryMessenger,
    private val viewType: String,
) : PlatformViewFactory(StandardMessageCodec.INSTANCE) {
    override fun create(context: Context, viewId: Int, args: Any?): PlatformView {
        val params = args as? Map<*, *> ?: emptyMap<Any, Any>()
        return CesiumMapPlatformView(context, messenger, viewId, viewType, params)
    }
}

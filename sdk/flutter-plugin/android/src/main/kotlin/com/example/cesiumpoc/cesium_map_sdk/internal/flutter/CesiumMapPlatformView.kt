package com.example.cesiumpoc.cesium_map_sdk.internal.flutter

import android.content.Context
import android.view.View
import com.example.cesiumpoc.cesium_native_android_poc.CesiumCameraState
import com.example.cesiumpoc.cesium_native_android_poc.CesiumGestureOptions
import com.example.cesiumpoc.cesium_native_android_poc.CesiumMapView
import com.example.cesiumpoc.cesium_native_android_poc.CesiumPerformanceOptions
import com.example.cesiumpoc.cesium_native_android_poc.CesiumRenderStats
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.platform.PlatformView

internal class CesiumMapPlatformView(
    context: Context,
    messenger: BinaryMessenger,
    viewId: Int,
    viewType: String,
    params: Map<*, *>,
) : PlatformView, MethodChannel.MethodCallHandler {
    private val mapView = CesiumMapView(context)
    private val channel = MethodChannel(messenger, "${viewType}_$viewId")

    init {
        channel.setMethodCallHandler(this)
        mapView.setCamera(
            CesiumCameraState(
                longitude = params.doubleValue("longitude", 116.397389),
                latitude = params.doubleValue("latitude", 39.908722),
                zoom = params.doubleValue("zoom", 15.0),
                autoOrbit = params.boolValue("autoOrbit", false),
                bearing = params.doubleValue("bearing", 0.0),
                pitch = params.doubleValue("pitch", 0.0),
            ),
        )
        mapView.interactionEnabled = params.boolValue("interactionEnabled", true)
        mapView.gestureOptions = params.toGestureOptions()
        mapView.performanceOptions = params.toPerformanceOptions()
        mapView.onStats = { channel.invokeMethod("stats", it.toMap()) }
        mapView.onMapReady = { channel.invokeMethod("mapReady", null) }
        mapView.onCameraMoveStarted = { channel.invokeMethod("cameraMoveStarted", it.toMap()) }
        mapView.onCameraChanged = { channel.invokeMethod("cameraChanged", it.toMap()) }
        mapView.onCameraIdle = { channel.invokeMethod("cameraIdle", it.toMap()) }
        mapView.onError = { code, message, details ->
            channel.invokeMethod(
                "error",
                mapOf(
                    "code" to code,
                    "message" to message,
                    "details" to details,
                ),
            )
        }
    }

    override fun getView(): View = mapView

    override fun dispose() {
        channel.setMethodCallHandler(null)
        mapView.release()
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        try {
            when (call.method) {
                "updateCamera" -> {
                    val args = call.arguments as? Map<*, *> ?: emptyMap<Any, Any>()
                    val currentCamera = mapView.cameraState
                    mapView.setCamera(
                        CesiumCameraState(
                            longitude = args.doubleValue("longitude", currentCamera.longitude),
                            latitude = args.doubleValue("latitude", currentCamera.latitude),
                            zoom = args.doubleValue("zoom", currentCamera.zoom),
                            autoOrbit = args.boolValue("autoOrbit", currentCamera.autoOrbit),
                            bearing = args.doubleValue("bearing", currentCamera.bearing),
                            pitch = args.doubleValue("pitch", currentCamera.pitch),
                        ),
                    )
                    result.success(null)
                }
                "getCameraState" -> {
                    result.success(mapView.cameraState.toMap())
                }
                "getInteractionEnabled" -> {
                    result.success(mapView.interactionEnabled)
                }
                "getGestureOptions" -> {
                    result.success(mapView.gestureOptions.toMap())
                }
                "getPerformanceOptions" -> {
                    result.success(mapView.performanceOptions.toMap())
                }
                "clearMemory" -> {
                    mapView.clearMemory()
                    result.success(null)
                }
                "setInteractionEnabled" -> {
                    mapView.interactionEnabled = call.arguments as? Boolean ?: true
                    result.success(null)
                }
                "setGestureOptions" -> {
                    val args = call.arguments as? Map<*, *> ?: emptyMap<Any, Any>()
                    mapView.gestureOptions = args.toGestureOptions()
                    result.success(null)
                }
                "setPerformanceOptions" -> {
                    val args = call.arguments as? Map<*, *> ?: emptyMap<Any, Any>()
                    mapView.performanceOptions = args.toPerformanceOptions()
                    result.success(null)
                }
                "getStats" -> {
                    result.success(mapView.getStats().toMap())
                }
                else -> result.notImplemented()
            }
        } catch (t: Throwable) {
            mapView.onError?.invoke(
                "platform_view_call_failed",
                t.message ?: "Platform view call failed.",
                call.method,
            )
            result.error("platform_view_call_failed", t.message, call.method)
        }
    }
}

private fun Map<*, *>.doubleValue(key: String, fallback: Double): Double {
    return (this[key] as? Number)?.toDouble() ?: fallback
}

private fun Map<*, *>.boolValue(key: String, fallback: Boolean): Boolean {
    return this[key] as? Boolean ?: fallback
}

private fun Map<*, *>.toGestureOptions(): CesiumGestureOptions {
    return CesiumGestureOptions(
        panEnabled = boolValue("panEnabled", true),
        zoomEnabled = boolValue("zoomEnabled", true),
        rotateEnabled = boolValue("rotateEnabled", false),
        tiltEnabled = boolValue("tiltEnabled", false),
        inertiaEnabled = boolValue("inertiaEnabled", false),
    )
}

private fun Map<*, *>.toPerformanceOptions(): CesiumPerformanceOptions {
    return CesiumPerformanceOptions(
        maximumScreenSpaceError = doubleValue("maximumScreenSpaceError", 4.0),
        movementMaximumScreenSpaceError =
            if (containsKey("movementMaximumScreenSpaceError")) {
                (this["movementMaximumScreenSpaceError"] as? Number)?.toDouble()
            } else {
                8.0
            },
    )
}

private fun CesiumCameraState.toMap(): Map<String, Any> {
    return mapOf(
        "longitude" to longitude,
        "latitude" to latitude,
        "zoom" to zoom,
        "autoOrbit" to autoOrbit,
        "bearing" to bearing,
        "pitch" to pitch,
    )
}

private fun CesiumGestureOptions.toMap(): Map<String, Any> {
    return mapOf(
        "panEnabled" to panEnabled,
        "zoomEnabled" to zoomEnabled,
        "rotateEnabled" to rotateEnabled,
        "tiltEnabled" to tiltEnabled,
        "inertiaEnabled" to inertiaEnabled,
    )
}

private fun CesiumPerformanceOptions.toMap(): Map<String, Any> {
    return buildMap {
        put("maximumScreenSpaceError", maximumScreenSpaceError)
        movementMaximumScreenSpaceError?.let {
            put("movementMaximumScreenSpaceError", it)
        }
    }
}

private fun CesiumRenderStats.toMap(): Map<String, Any> {
    return mapOf(
        "fps" to fps,
        "drawMs" to drawMs,
        "visibleTiles" to visibleTiles,
        "cachedTiles" to cachedTiles,
        "loadedTiles" to loadedTiles,
        "failedTiles" to failedTiles,
        "cacheMb" to cacheMb,
        "cesiumBackend" to cesiumBackend,
        "cesiumLinked" to cesiumLinked,
        "cesiumCameraUpdates" to cesiumCameraUpdates,
        "cesiumMemoryClears" to cesiumMemoryClears,
        "cesiumFrameUpdates" to cesiumFrameUpdates,
        "cesiumCameraDirty" to cesiumCameraDirty,
        "cesiumFrameDt" to cesiumFrameDt,
        "cesiumEcefX" to cesiumEcefX,
        "cesiumEcefY" to cesiumEcefY,
        "cesiumEcefZ" to cesiumEcefZ,
    )
}

package com.example.cesiumpoc.cesium_map_sdk

import com.example.cesiumpoc.cesium_map_sdk.internal.flutter.CesiumMapConstants
import com.example.cesiumpoc.cesium_map_sdk.internal.flutter.CesiumMapViewFactory
import io.flutter.embedding.engine.plugins.FlutterPlugin

class CesiumMapSdkPlugin : FlutterPlugin {
    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        binding.platformViewRegistry.registerViewFactory(
            CesiumMapConstants.VIEW_TYPE,
            CesiumMapViewFactory(
                binding.binaryMessenger,
                CesiumMapConstants.VIEW_TYPE,
            ),
        )
        binding.platformViewRegistry.registerViewFactory(
            CesiumMapConstants.LEGACY_VIEW_TYPE,
            CesiumMapViewFactory(
                binding.binaryMessenger,
                CesiumMapConstants.LEGACY_VIEW_TYPE,
            ),
        )
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) = Unit
}

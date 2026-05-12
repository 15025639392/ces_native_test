package com.example.cesiumpoc.cesium_native_android_poc

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        flutterEngine
            .platformViewsController
            .registry
            .registerViewFactory(
                "gaode_satellite_view",
                GaodeSatelliteViewFactory(flutterEngine.dartExecutor.binaryMessenger),
            )
    }
}

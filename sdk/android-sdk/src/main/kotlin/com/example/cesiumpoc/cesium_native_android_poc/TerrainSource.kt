package com.example.cesiumpoc.cesium_native_android_poc

sealed interface TerrainSource {
    val id: String
}

data class QuantizedMeshTerrainSource(
    override val id: String,
    val layerJsonUrl: String,
) : TerrainSource

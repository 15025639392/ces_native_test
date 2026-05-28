package com.example.cesiumpoc.cesium_native_android_poc

sealed interface ImagerySource {
    val id: String
}

data class UrlTemplateImagerySource(
    override val id: String,
    val urlTemplate: String,
) : ImagerySource

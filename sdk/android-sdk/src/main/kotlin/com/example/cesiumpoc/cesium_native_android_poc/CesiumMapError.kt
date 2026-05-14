package com.example.cesiumpoc.cesium_native_android_poc

/**
 * Lightweight error payload surfaced to Android hosts in V1.
 */
data class CesiumMapError(
    val code: String,
    val message: String,
    val details: String? = null,
)

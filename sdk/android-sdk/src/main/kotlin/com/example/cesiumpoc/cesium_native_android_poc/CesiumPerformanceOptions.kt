package com.example.cesiumpoc.cesium_native_android_poc

/**
 * V1 performance tuning surface for Android and Flutter hosts.
 *
 * `maximumScreenSpaceError` is the first formal detail-pressure knob.
 * Larger values reduce tile detail pressure and memory usage, while smaller
 * values favor sharper imagery at a higher runtime cost.
 */
data class CesiumPerformanceOptions(
    val maximumScreenSpaceError: Double = 4.0,
    val movementMaximumScreenSpaceError: Double? = 8.0,
) {
    init {
        require(maximumScreenSpaceError > 0.0) {
            "maximumScreenSpaceError must be > 0.0"
        }
        require(movementMaximumScreenSpaceError == null || movementMaximumScreenSpaceError > 0.0) {
            "movementMaximumScreenSpaceError must be null or > 0.0"
        }
    }
}

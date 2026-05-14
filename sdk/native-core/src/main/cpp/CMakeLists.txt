cmake_minimum_required(VERSION 3.22.1)

project(cesium_native_android_poc LANGUAGES CXX)

add_library(
    cesium_bridge
    SHARED
        cesium_bridge.cpp
        batch_renderer.cpp
        gl_resources.cpp
        imagery.cpp
        prepare_renderer_resources.cpp
)

target_compile_features(cesium_bridge PRIVATE cxx_std_17)

target_compile_options(
    cesium_bridge
    PRIVATE
        -Wall
        -Wextra
        -Werror
)

if(NOT DEFINED CESIUM_NATIVE_ROOT OR "${CESIUM_NATIVE_ROOT}" STREQUAL "")
    message(FATAL_ERROR "CESIUM_NATIVE_ROOT must point to an Android arm64 Cesium Native build directory.")
endif()

list(APPEND CMAKE_PREFIX_PATH
    "${CESIUM_NATIVE_ROOT}/install"
    "${CESIUM_NATIVE_ROOT}/vcpkg_installed/arm64-android"
)
list(APPEND CMAKE_FIND_ROOT_PATH
    "${CESIUM_NATIVE_ROOT}/install"
    "${CESIUM_NATIVE_ROOT}/vcpkg_installed/arm64-android"
)
list(APPEND CMAKE_LIBRARY_PATH
    "${CESIUM_NATIVE_ROOT}/vcpkg_installed/arm64-android/lib"
    "${CESIUM_NATIVE_ROOT}/vcpkg_installed/arm64-android/debug/lib"
)
list(APPEND CMAKE_INCLUDE_PATH
    "${CESIUM_NATIVE_ROOT}/vcpkg_installed/arm64-android/include"
)
set(cesium-native_DIR "${CESIUM_NATIVE_ROOT}/install/share/cesium-native/cmake")

find_library(log_lib log)
find_library(gles_lib GLESv3)
find_package(cesium-native CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

target_compile_definitions(cesium_bridge PRIVATE CESIUM_NATIVE_LINKED=1)

target_link_libraries(
    cesium_bridge
    PRIVATE
        ${log_lib}
        ${gles_lib}
        CURL::libcurl
        CesiumGeospatial
        Cesium3DTilesSelection
)

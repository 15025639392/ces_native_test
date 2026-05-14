plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.example.cesiumpoc.cesium_native_android_poc.sdk"
    compileSdk = 36

    val defaultCesiumNativeRoot =
        "/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a"
    val cesiumNativeRoot =
        providers.gradleProperty("cesiumNativeRoot").orElse(defaultCesiumNativeRoot).get()

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_17.toString()
    }

    defaultConfig {
        minSdk = 26

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                abiFilters += listOf("arm64-v8a")
                cppFlags += listOf("-std=c++17")
                arguments += listOf("-DCESIUM_NATIVE_ROOT=$cesiumNativeRoot")
            }
        }

        consumerProguardFiles("consumer-rules.pro")
    }

    externalNativeBuild {
        cmake {
            path = file("../native-core/src/main/cpp/CMakeLists.txt")
        }
    }

    buildTypes {
        release {
            consumerProguardFiles("consumer-rules.pro")
        }
    }
}

dependencies {
    implementation("androidx.annotation:annotation:1.9.1")
    implementation("androidx.fragment:fragment-ktx:1.8.6")
}

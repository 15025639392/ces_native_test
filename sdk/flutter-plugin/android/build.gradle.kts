group = "com.example.cesiumpoc.cesium_map_sdk"
version = "1.0-SNAPSHOT"

buildscript {
    val kotlinVersion = "2.2.20"
    repositories {
        google()
        mavenCentral()
    }

    dependencies {
        classpath("com.android.tools.build:gradle:8.11.1")
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:$kotlinVersion")
    }
}

dependencies {
    implementation("androidx.annotation:annotation:1.9.1")
    implementation("androidx.fragment:fragment-ktx:1.8.6")
}

allprojects {
    repositories {
        google()
        mavenCentral()
    }
}

plugins {
    id("com.android.library")
    id("kotlin-android")
}

android {
    namespace = "com.example.cesiumpoc.cesium_map_sdk"
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

    sourceSets {
        getByName("main") {
            manifest.srcFile("../../android-sdk/src/main/AndroidManifest.xml")
            java.srcDirs("src/main/kotlin", "../../android-sdk/src/main/kotlin")
        }
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

        consumerProguardFiles("../../android-sdk/consumer-rules.pro")
    }

    externalNativeBuild {
        cmake {
            path = file("../../native-core/src/main/cpp/CMakeLists.txt")
        }
    }

    buildTypes {
        release {
            consumerProguardFiles("../../android-sdk/consumer-rules.pro")
        }
    }
}

# Android SDK 快速接入

这份文档面向直接消费 `sdk/android-sdk` 的 Android 宿主。

当前阶段它的目标不是讲一套未来大而全的 SDK，而是给出基于当前代码事实的最小接入方式。

## 当前公开入口

当前建议宿主直接使用这些公开类型：

- `CesiumMapView`
- `CesiumMapFragment`
- `CesiumMapController`
- `CesiumCameraState`
- `CesiumGestureOptions`
- `CesiumMapListener`
- `CesiumMapError`
- `CesiumRenderStats`

## 方式一：直接使用 `CesiumMapView`

```kotlin
class DemoActivity : Activity() {
    private lateinit var mapView: CesiumMapView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        mapView = CesiumMapView(this).apply {
            onCreate()
            cameraState = CesiumCameraState(
                longitude = 116.397389,
                latitude = 39.908722,
                zoom = 15.0,
                autoOrbit = false,
            )
            interactionEnabled = true
            gestureOptions = CesiumGestureOptions()
            setListener(
                object : CesiumMapListener {
                    override fun onMapReady() = Unit
                    override fun onCameraMoveStarted(state: CesiumCameraState) = Unit
                    override fun onCameraChanged(state: CesiumCameraState) = Unit
                    override fun onCameraIdle(state: CesiumCameraState) = Unit
                    override fun onRenderStats(stats: CesiumRenderStats) = Unit
                    override fun onError(error: CesiumMapError) = Unit
                },
            )
        }

        setContentView(mapView)
    }

    override fun onStart() {
        super.onStart()
        mapView.onStart()
    }

    override fun onResume() {
        super.onResume()
        mapView.onResume()
    }

    override fun onPause() {
        mapView.onPause()
        super.onPause()
    }

    override fun onStop() {
        mapView.onStop()
        super.onStop()
    }

    override fun onLowMemory() {
        super.onLowMemory()
        mapView.onLowMemory()
    }

    override fun onDestroy() {
        mapView.onDestroy()
        super.onDestroy()
    }
}
```

## 方式二：使用 `CesiumMapFragment`

```kotlin
class DemoFragmentActivity : FragmentActivity() {
    private val mapFragment =
        CesiumMapFragment.newInstance(
            CesiumCameraState(
                longitude = 116.397389,
                latitude = 39.908722,
                zoom = 15.0,
                autoOrbit = false,
            ),
        )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if (savedInstanceState == null) {
            supportFragmentManager.beginTransaction()
                .replace(android.R.id.content, mapFragment)
                .commitNow()
        }

        mapFragment.controller.setListener(
            object : CesiumMapListener {
                override fun onMapReady() = Unit
            },
        )
    }
}
```

## 控制面

当前最小控制面是：

```kotlin
val controller: CesiumMapController = mapFragment.controller

controller.cameraState = controller.cameraState.copy(zoom = 16.0)
controller.interactionEnabled = false
controller.gestureOptions = CesiumGestureOptions(zoomEnabled = false)

val stats = controller.getStats()
controller.clearMemory()
```

## 当前语义边界

当前需要明确几点：

1. `cameraState`
   - 这是当前最稳定的相机输入面。
   - 当前正式输入面已经扩成 `longitude / latitude / zoom / autoOrbit / bearing / pitch`。
   - `bearing / pitch` 现在已经进入 Android native 相机朝向计算。

2. `interactionEnabled`
   - 当前已经有真实 native host 行为。
   - 关闭后会停止当前宿主 view 的触摸分发。

3. `gestureOptions`
   - 当前已经是正式 SDK 配置项。
   - `panEnabled`、`zoomEnabled`、`inertiaEnabled` 已经有真实 Android 原生行为。
   - `rotateEnabled`、`tiltEnabled` 现在已经分别接入双指旋转和双指俯仰手势。

4. 生命周期
   - `CesiumMapView` 当前已经暴露基本生命周期入口。
   - 除 `onDestroy` / `onLowMemory` 外，其余多数仍是轻量宿主接口，后续会继续增强。

5. 相机事件
   - `onCameraMoveStarted`：相机开始连续变化时触发一次。
   - `onCameraChanged`：这是节流后的进行中回调，不保证逐帧抛出。
   - `onCameraIdle`：相机变化停止并稳定后触发。

## 当前仓库里的原生验证页

当前已有两个直接消费 Android SDK 的示例页：

- `examples/android-native-demo/.../NativeSdkMapActivity`
- `examples/android-native-demo/.../NativeSdkFragmentActivity`

安装 APK 后可以直接启动：

```sh
adb shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkMapActivity

adb shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkFragmentActivity
```

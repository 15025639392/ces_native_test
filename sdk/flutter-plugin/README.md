# Flutter SDK 适配层

这个目录承载 Flutter 侧正式适配层，职责很明确：

- 暴露 Flutter 可消费的 `CesiumMapWidget`
- 封装 `MethodChannel` 与 `AndroidView`
- 把 Android 原生地图页能力适配为 Flutter 组件与控制器
- 通过 Flutter Android 插件自动注册 `PlatformView`
- 对齐 Flutter V1 控制面，例如 `setCamera`、`getStats`、`setInteractionEnabled`、`setGestureOptions`

当前这层已经具备：

- 设置和读取相机状态
- 设置和读取交互启用状态
- 设置和读取手势配置
- 接收 `onCameraMoveStarted / onCameraChanged / onCameraIdle` 相机事件
- 清理内存
- 获取渲染统计

它不是渲染核心。

真正的地图渲染与 Cesium Native 生命周期仍然位于：

- `sdk/android-sdk`
- `sdk/native-core`

当前阶段它还是一个轻量适配包，主要目标是把 Flutter 逻辑从示例工程中拆出来，让 `examples/flutter-demo` 回归为纯消费方。

当前 Android 自动注册入口：

- `android/src/main/kotlin/com/example/cesiumpoc/cesium_map_sdk/CesiumMapSdkPlugin.kt`

它会在 FlutterEngine 附着时自动注册：

- `cesium_map_view`
- `gaode_satellite_view`（兼容保留）

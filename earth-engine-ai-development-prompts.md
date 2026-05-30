# Cesium Native 地球引擎开发 AI 提示词文档

这份文档用于把“基于 cesium-native 开发 Android / Flutter 地球引擎”转换成 AI 可以直接执行的开发提示词。适用于本仓库：

- `sdk/native-core`：C++ JNI bridge、Cesium Native tileset、GPU 资源准备、OpenGL 绘制。
- `sdk/android-sdk`：Android `CesiumMapView` / `CesiumMapFragment` 宿主层。
- `sdk/flutter-plugin`：Flutter PlatformView 适配层。
- `examples/android-native-demo`：Android 原生验证 Demo。
- `examples/flutter-demo`：Flutter 验证 Demo。

## 总提示词

```text
你是一个资深 C++ / Android / Flutter / Cesium Native 工程师。请在当前仓库中开发一个基于 cesium-native 的移动端 3D 地球引擎渲染后端。

当前仓库不是从零实现瓦片引擎，而是为 cesium-native 提供 Android / Flutter 宿主层、渲染资源准备、OpenGL 绘制和公开 SDK API。

请先阅读：
- README.md
- docs/RENDERER_BACKEND.md
- sdk/native-core/README.md
- sdk/android-sdk/README.md
- earth-engine-ai-terms.md
- earth-engine-bug-description-guide.md

必须遵守架构边界：
- cesium-native 负责 tileset 生命周期、SSE 瓦片选择、加载队列、缓存淘汰、raster overlay provider、raster attachment、fallback、overlay texture coordinates。
- 本仓库负责 Android View / EGL 生命周期、host camera state 到 Cesium3DTilesSelection::ViewState 的转换、IPrepareRendererResources、GPU buffer / texture 上传、OpenGL 绘制、Android SDK 和 Flutter plugin 适配。
- 不允许在宿主层重新实现 tile selection、imagery tile selection、parent fallback、raster attachment lifecycle。

开发时请保持小步改动：
1. 先定位现有类、接口和线程边界。
2. 复用 Cesium Native 的 WGS84、Cartographic、Ellipsoid、LocalHorizontalCoordinateSystem、Math 等能力。
3. Kotlin / Dart / C++ API 命名保持现有风格。
4. 修改后运行相关构建或最小验证命令。
5. 输出变更摘要、验证结果和剩余风险。
```

## 架构边界提示词

```text
请检查并改进当前 cesium-native Android renderer backend 的架构边界。

目标：
- 保证 tile traversal、SSE、tile load queue、cache eviction、raster overlay selection、raster attachment lifecycle 继续由 cesium-native 管理。
- 宿主层只负责 camera ViewState 输入、renderer resource prepare、GPU upload、OpenGL draw 和 Android / Flutter API。
- 如果发现宿主层存在自定义 tile selection、imagery fallback、手写 raster UV、父子瓦片替代逻辑，请指出风险并设计迁移方案。

请输出：
- 当前代码中相关文件和职责说明。
- 违反边界的地方，按严重程度排序。
- 最小修复方案。
- 需要补充的测试或 Demo 验证步骤。
```

## Native Core 开发提示词

```text
请在 sdk/native-core 中实现或修复 cesium-native C++ 渲染后端功能。

开发要求：
- 入口优先查看 cesium_bridge.cpp、prepare_renderer_resources.*、gl_resources.*、curl_asset_accessor.*、imagery_source_config.*。
- 每帧渲染循环必须遵守：
  1. 根据当前 WGS84 camera 构建 Cesium3DTilesSelection::ViewState。
  2. 调用 Tileset::updateViewGroup。
  3. 调用 Tileset::loadTiles。
  4. 在 render thread dispatch Cesium main-thread tasks。
  5. 只绘制 ViewUpdateResult::tilesToRenderThisFrame。
- GPU 资源上传应跨帧调度，避免手势过程中一次性上传导致掉帧。
- OpenGL draw path 使用 OpenGL 深度约定，不要直接替换成 Cesium Native reversed-Z Vulkan-style projection，除非同步修改 shader depth convention。
- 坐标计算使用 CesiumGeospatial::Ellipsoid::WGS84、Cartographic、LocalHorizontalCoordinateSystem 和 CesiumUtility::Math，不要手写米/度近似。

验收：
- 能编译 Android native demo。
- 地球可显示、可拖拽、可缩放。
- 瓦片加载、纹理上传、绘制命令数量可通过日志或 stats 观察。
- 无明显黑屏、瓦片错位、闪烁、native crash。
```

## Android SDK 开发提示词

```text
请在 sdk/android-sdk 中开发 Android 原生地图 SDK 能力。

目标：
- 提供稳定的 CesiumMapView / CesiumMapFragment API。
- 正确转发生命周期到内部 render surface。
- 支持 camera、gesture、performance、imagery URL-template、render stats、error listener。
- 保持 Android 宿主层轻量，不复制 C++ backend 已拥有的 WGS84 camera math、tile selection、imagery selection、render fallback 逻辑。

重点文件：
- CesiumMapView.kt
- CesiumMapFragment.kt
- CesiumMapController.kt
- CesiumMapFragmentController.kt
- CesiumCameraState.kt
- CesiumGestureOptions.kt
- CesiumPerformanceOptions.kt
- CesiumRenderStats.kt
- internal/render/CesiumMapRenderSurface.kt
- internal/render/NativeCesiumBridge.kt

开发要求：
- Public API 要有清晰的数据类和错误模型。
- Activity / Fragment lifecycle、Surface created / destroyed、onPause / onResume 时序必须清楚。
- Native handle 创建、释放、重复释放、防止 destroy 后回调都要谨慎处理。
- 不要在 UI thread 做重型资源加载或阻塞等待 native 渲染。

验收：
- examples/android-native-demo 能 assembleDebug。
- 打开 Demo 后地图能显示并响应手势。
- 页面进入后台再返回不黑屏、不崩溃。
- 退出页面后没有 native use-after-free 或重复释放日志。
```

## Flutter Plugin 开发提示词

```text
请在 sdk/flutter-plugin 中开发 Flutter 地图插件能力。

目标：
- Flutter 层提供 CesiumMapWidget、CesiumMapController、ImagerySource、CesiumCameraState、CesiumGestureOptions、CesiumPerformanceOptions、CesiumRenderStats 等 API。
- Android 侧通过 PlatformView 接入 sdk/android-sdk。
- Dart API 与 Android SDK 语义一致，避免 Flutter 层重新实现底层地球数学或瓦片逻辑。

重点文件：
- sdk/flutter-plugin/lib/cesium_map_sdk.dart
- sdk/flutter-plugin/lib/src/*.dart
- sdk/flutter-plugin/android/src/main/kotlin/.../CesiumMapSdkPlugin.kt
- sdk/flutter-plugin/android/src/main/kotlin/.../CesiumMapPlatformView.kt
- sdk/flutter-plugin/android/src/main/kotlin/.../CesiumMapViewFactory.kt
- sdk/flutter-plugin/android/src/main/kotlin/.../CesiumMapConstants.kt

开发要求：
- MethodChannel 参数要稳定、可扩展，并处理 null / 类型错误。
- PlatformView attach / detach、dispose、route 切换、Activity 重建要正确转发。
- Dart controller 不应在 native view 未 ready 时直接发送不可恢复命令。
- 错误要映射成 CesiumMapError 或清晰的 Flutter exception。

验收：
- examples/flutter-demo 能 flutter build apk --debug --target-platform android-arm64。
- Flutter 页面能显示地图、手势可用、切换页面后释放正常。
- Android logcat 中没有 PlatformView detach 后继续访问 native handle 的错误。
```

## 相机与手势开发提示词

```text
请实现或修复地球相机与手势交互。

架构要求：
- Public camera state 使用 WGS84 geodetic state。
- longitude / latitude 表示视图目标点，单位为度。
- altitudeMeters 表示相机距离目标点的范围，单位为米。
- bearing / pitch 表示以目标点为中心的观察方向。
- 不暴露 WebMercator zoom level 作为核心 camera state。
- pinch、double tap 等缩放交互应按 altitudeMeters 做乘法变化。
- C++ backend 负责 ellipsoid ray picking、focus-stable pan / zoom、rotate、tilt、auto-orbit、camera sanitization。
- Android / Flutter host 只转发屏幕手势事件，不重复实现 WGS84 相机数学。

请重点检查：
- lon / lat 顺序是否一致。
- degree / radian 边界是否清楚。
- pitch / bearing 方向是否与用户预期一致。
- 手势过程中焦点是否稳定。
- 极区、跨 180 度经线、超低高度、超高高度是否有保护。

验收：
- 拖拽地球时焦点跟手。
- pinch zoom 以手势中心附近缩放。
- double tap 缩放不跳变。
- pitch / rotate 不穿地、不翻转。
```

## 影像图层开发提示词

```text
请实现或修复 URL-template raster imagery 支持。

当前公开范围：
- 支持一个 URL-template raster overlay。
- 对应 CesiumRasterOverlays::UrlTemplateRasterOverlay。

架构要求：
- raster provider、raster attachment、fallback、translation、scale、overlay texture coordinate IDs 由 cesium-native 管理。
- OpenGL renderer 使用 Cesium Native 生成的 _CESIUMOVERLAY_* texture coordinates。
- 不允许从经纬度线性插值合成 fallback UV，这会导致 WebMercator 影像在椭球上错缝。
- 默认开发模板优先使用 HTTP tile endpoint，避免为了验证而关闭 TLS 校验。
- 生产 provider 的授权、鉴权、HTTPS 和 license 策略不应写死在 SDK 内。

请输出：
- API 设计或修复点。
- native overlay 配置映射。
- 加载错误、HTTP 错误、超时、空瓦片的处理方式。
- Demo 验证步骤。
```

## 渲染性能开发提示词

```text
请优化 cesium-native Android renderer backend 的交互性能。

目标：
- 手势过程中保持响应优先。
- 控制 render thread main-task dispatch、GPU upload、并发 tile load 对帧时间的压力。
- 允许在 active gesture 期间临时提高 screen-space error、减少并发加载、延后部分资源上传。
- 这些策略只能影响压力控制，不能替代 cesium-native 的 tile management。

请检查：
- 每帧 draw call、visible tile count、pending upload count、pending request count。
- 手势中和静止后的 SSE / load / upload 策略差异。
- 纹理上传是否集中在单帧。
- UI thread 是否被阻塞。
- native task queue 是否在 render thread 合理 dispatch。

验收：
- 快速拖拽和缩放时不卡死。
- 停止交互后能逐步补齐高精度瓦片。
- 不出现长期低清、瓦片空洞、资源泄漏。
```

## 生命周期与稳定性提示词

```text
请排查并修复 Android / Flutter 地图生命周期稳定性问题。

重点场景：
- Activity / Fragment onPause、onResume、onDestroy。
- Surface created、changed、destroyed。
- EGL context 丢失与重建。
- Flutter PlatformView attach、detach、dispose、route push/pop。
- native handle 创建、销毁和回调时序。

要求：
- destroy 后不得继续访问 native pointer。
- Surface 销毁后不得继续提交 OpenGL draw。
- resume 后 render loop、surface、GL resource 状态要一致。
- MethodChannel / listener / callback 要能处理 view 已释放的情况。

请输出：
- 生命周期状态机或关键时序说明。
- 可能的 race condition。
- 最小修复。
- logcat 验证点。
```

## Bug 修复提示词

```text
请根据下面的 Bug 报告分析并修复当前仓库问题。

Bug 报告：
【粘贴 earth-engine-bug-description-guide.md 中的完整 Bug 报告】

分析要求：
- 先判断模块归属：相机 / 坐标 / 拾取 / 瓦片 / 图层 / 渲染 / 生命周期 / Flutter 适配层 / JNI / 线程。
- 再列出最可能的 3 个根因。
- 阅读相关文件后再修改代码。
- 修改必须遵守 docs/RENDERER_BACKEND.md 的 ownership boundary。
- 不要通过重写 cesium-native 已负责的 tile selection 或 raster attachment 来修复表面现象。

交付要求：
- 说明根因。
- 给出代码修改摘要。
- 给出验证命令。
- 如果无法完整验证，说明缺少什么环境或数据。
```

## 常用验收命令

```sh
# Android 原生 Demo
cd examples/android-native-demo
./gradlew :app:assembleDebug

# Flutter Demo
cd examples/flutter-demo
flutter build apk --debug --target-platform android-arm64

# Android SDK
cd sdk/android-sdk
./gradlew assemble
```

如果本地 cesium-native Android 构建目录不同，使用：

```sh
./gradlew :app:assembleDebug -PcesiumNativeRoot=/absolute/path/to/cesium-native/build-android-arm64-v8a
```

## AI 开发禁止事项

```text
不要做这些事：
- 不要在 Android / Flutter 宿主层实现 tile selection。
- 不要在宿主层实现 imagery tile selection。
- 不要在宿主层实现 parent fallback。
- 不要手写 raster attachment lifecycle。
- 不要用经纬度线性插值伪造 raster UV。
- 不要用固定米/度近似做 WGS84 相机移动。
- 不要混用 degree / radian。
- 不要把 WebMercator zoom level 作为核心 camera state。
- 不要在 UI thread 做重型 native / GPU 工作。
- 不要在 Surface destroyed 后继续 OpenGL draw。
- 不要在 native object destroyed 后继续回调 Kotlin / Dart。
- 不要为了测试地图源而在 SDK 中关闭 TLS 校验。
```

## AI 输出格式建议

```text
请按以下格式输出：

1. 我先读到的架构事实
2. 问题定位
3. 修改方案
4. 实际代码改动
5. 验证命令和结果
6. 剩余风险

如果你需要修改代码，请直接修改，不要只给建议。
如果需要新增 API，请先说明 public contract，再实现 Android SDK、Flutter plugin、native bridge 的对应映射。
```

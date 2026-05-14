# SDK 化蓝图

这份文档描述的是：如果当前项目不只是做一个 PoC App，而是要演进成一个可复用的地图 SDK，推荐的目标结构应该是什么。

这里的核心判断是：

- SDK 的一等公民应该是原生 SDK。
- Flutter 应该是 SDK 的适配层，不应该成为地图内核宿主。
- Demo App 只是验证和展示 SDK 的样例，不应该反过来主导 SDK 的长期结构。

## 目标产物

建议最终至少产出三类东西：

1. C++ 地图内核
2. Android 原生 SDK
3. Flutter 适配层 SDK

可以把它们理解成：

```text
native-core
  -> 真正的地图能力内核

android-sdk
  -> 给 Android 宿主接入的正式 SDK

flutter-plugin
  -> 给 Flutter 宿主接入的适配封装
```

## 为什么原生 SDK 应该是一等公民

如果目标是 SDK，而不是单一 App，原生 SDK 应该成为中心，原因有四个：

1. 地图渲染、生命周期、手势系统天然属于平台层。
2. 宿主应用未必是 Flutter。
3. Flutter 作为接入层更灵活，但不适合承担地图内核宿主职责。
4. 原生 SDK 更容易稳定 API、管理版本、处理性能问题和平台差异。

所以正确方向不是“先做 Flutter 架构，再把它抽成 SDK”，而是：

先做原生地图 SDK，再做 Flutter 适配层。

## 推荐模块结构

```text
sdk/
  native-core/
    src/main/cpp/

  android-sdk/
    src/main/kotlin/
    README.md

  flutter-plugin/
    lib/
    android/
    ios/           # 未来
    harmony/       # 未来如需

examples/
  flutter-demo/

docs/
  SDK_BLUEPRINT.md
  SDK_API_DRAFT.md
  ARCHITECTURE_IDEAL.md
  ARCHITECTURE_MIGRATION.md
```

## 分层职责

## 当前落地状态

这一轮拆分之后，仓库里已经开始有真实目录承载：

- `sdk/native-core/src/main/cpp`
- `sdk/android-sdk/src/main/kotlin`
- `examples/flutter-demo`

当前仍然保留的过渡状态是：

- Flutter demo 还在仓库根目录运行。
- `android/app` 仍然作为 demo Android 壳存在。
- demo 壳通过 Gradle `sourceSets` 和 `externalNativeBuild` 引用 `sdk/` 下的正式源码。

### 1. `native-core`

这是地图能力内核。

应负责：

- Cesium Native 集成。
- tileset 生命周期。
- 资源加载与调度。
- 相机状态与场景更新。
- 渲染资源准备与绘制。
- 统计与诊断能力。
- 尽可能平台无关的核心逻辑。

不应负责：

- Android `View` 生命周期。
- Flutter channel 细节。
- 宿主业务 UI。

### 2. `android-sdk`

这是 Android 正式接入层。

应负责：

- `MapView`、`MapFragment`、可选 `MapActivity`。
- 生命周期接入。
- 手势系统。
- 原生页面宿主。
- 渲染宿主视图，例如 `TextureView` 或未来的 `SurfaceView`。
- Android 到 C++ 的稳定桥接。
- 向宿主暴露清晰 API。

不应负责：

- 承担 Flutter 业务 UI。
- 把 Demo 页面逻辑写进正式 SDK API。

### 3. `flutter-plugin`

这是 Flutter 适配层。

应负责：

- Flutter `Widget` 封装。
- `Controller` 封装。
- Flutter 和 Android SDK 的桥接。
- Flutter Overlay 组件和集成模式。
- 事件回传和参数映射。

不应负责：

- 绕过 Android SDK 直接控制 C++ 内核。
- 成为地图引擎主宿主。

### 4. `examples`

这是样例层。

应负责：

- 演示 Android 原生 SDK 接入。
- 演示 Flutter SDK 接入。
- 验证典型业务集成方式。

不应负责：

- 承担正式 SDK 的长期 API 设计责任。

## 推荐对外产品形态

Android 侧建议优先提供：

- `CesiumMapView`
- `CesiumMapFragment`

按需提供：

- `CesiumMapActivity`

Flutter 侧建议优先提供：

- `CesiumMapWidget`
- `CesiumMapController`

这意味着：

- Android 原生宿主可以直接使用 `View` 或 `Fragment`。
- Flutter 宿主使用 `Widget + Controller`。
- 但 Flutter 适配层底下调用的仍然是 Android SDK，而不是直接碰 C++。

## 推荐 API 面

SDK 对外 API 最少应覆盖以下几类能力。

### 1. 地图生命周期接口

例如：

- `onCreate` / `onStart` / `onResume` / `onPause` / `onStop` / `onDestroy`
- `onSurfaceChanged`
- `onLowMemory`

### 2. 相机接口

例如：

- `setCamera`
- `animateCamera`
- `moveCamera`
- `getCameraState`
- `fitBounds`

### 3. 交互接口

例如：

- `setInteractionEnabled`
- `setGestureOptions`
- `setInteractionMode`
- `pick`

### 4. 场景与资源接口

例如：

- `loadScene`
- `setImagerySource`
- `setTerrainSource`
- `addTileset`
- `removeTileset`
- `clearMemory`

### 5. 事件接口

例如：

- `onMapReady`
- `onCameraChanged`
- `onSelectionChanged`
- `onRenderStats`
- `onError`

### 6. 诊断接口

例如：

- `getStats`
- `setLogLevel`
- `enableDebugOverlay`

## 推荐 API 设计原则

1. 先小后大：先定义最小稳定 API，不要一开始把所有设想都暴露出去。
2. 事件和命令分离：命令用于驱动地图，事件用于回传状态。
3. 避免暴露内部实现细节：不要把零散的 Cesium 内部概念直接抛给宿主。
4. Flutter API 尽量映射 Android SDK API，而不是自创一套完全不同的语义。
5. Debug 能力要内建，不要等出问题再补。

## 推荐页面组织策略

如果目标是 SDK，推荐把“原生地图页 + Flutter Overlay”理解为：

- 这是推荐的集成模式之一。
- 不是 SDK 唯一对外形态。

更准确地说：

- Android SDK 应首先提供 `MapView` / `MapFragment`
- 原生地图页只是 Android Demo 或宿主可选组织方式
- Flutter Overlay 是 Flutter 宿主的一种高价值集成模式

也就是说，SDK 产品形态要比 Demo 页面结构更抽象。

## 从当前仓库到 SDK 的建议演进

建议顺序：

1. 先稳定当前 C++ 渲染主路径。
2. 再把 Android 宿主层整理成更像 SDK 的接口。
3. 再把当前 Flutter 页面逻辑收敛成 Flutter 适配层。
4. 最后把 Demo 和 SDK 代码分离。

短期最重要的不是立刻拆目录，而是先定义：

- SDK 模块边界
- SDK 对外 API
- Demo 和 SDK 的职责边界

## 建议暂时不要做的事

- 不要让 Flutter 层继续直接定义地图内核语义。
- 不要把 Demo 页面代码直接包装后当成正式 SDK API。
- 不要在 SDK 结构未稳定前同时做大规模渲染后端切换。

## 一句话结论

如果目标是 SDK，最合理的主线是：

先做 Android 原生地图 SDK，再在其上做 Flutter 适配层，而不是反过来。

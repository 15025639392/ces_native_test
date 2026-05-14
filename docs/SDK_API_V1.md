# SDK API V1 最小正式契约

这份文档不是继续发散 API 设想，而是把第一版真正建议承诺给宿主的能力收紧成最小集合。

目标很明确：

- 先定义一版小而稳的正式契约。
- 只承诺当前架构方向能稳住的能力。
- 先避免把高风险、高波动能力过早写进正式 API。

## 设计原则

V1 只承诺以下类型的能力：

1. 地图创建与销毁
2. 相机设置与读取
3. 基础交互开关
4. 基础统计回传
5. 错误回传
6. 内存清理

V1 暂不正式承诺：

- 拾取系统
- 富图层管理
- 高级场景编排
- 复杂动画系统
- 截图、投影转换等工具能力
- Vulkan 或多后端选择能力

## Android SDK V1

### 公开类型

V1 建议只正式公开这些核心类型：

- `CesiumMapView`
- `CesiumMapFragment`
- `CesiumMapController`
- `CesiumCameraState`
- `CesiumGestureOptions`
- `CesiumRenderStats`
- `CesiumMapListener`
- `CesiumMapError`

### Android V1 接口

```kotlin
interface CesiumMapController {
    var cameraState: CesiumCameraState
    var interactionEnabled: Boolean
    var gestureOptions: CesiumGestureOptions

    fun setCamera(camera: CesiumCameraState)

    fun clearMemory()
    fun getStats(): CesiumRenderStats

    fun setListener(listener: CesiumMapListener?)
}
```

当前仓库已经开始向这个方向收敛，但还不是完整实现：

- `CesiumMapView` 已经实现最小版 `CesiumMapController`
- `CesiumMapFragment` 通过 `controller` 暴露同一套控制语义
- `interactionEnabled` 和 `gestureOptions` 已落地到 Android/Flutter 接口面
- 当前交互配置已经开始具备真实原生行为：平移、缩放、双击缩放、惯性、双指旋转、双指俯仰与回调节流都已在 Android 宿主层落地

```kotlin
interface CesiumMapListener {
    fun onMapReady()
    fun onCameraMoveStarted(state: CesiumCameraState)
    fun onCameraChanged(state: CesiumCameraState)
    fun onCameraIdle(state: CesiumCameraState)
    fun onRenderStats(stats: CesiumRenderStats)
    fun onError(error: CesiumMapError)
}
```

### Android V1 数据类型

```kotlin
data class CesiumCameraState(
    val longitude: Double,
    val latitude: Double,
    val zoom: Double,
    val autoOrbit: Boolean = false,
    val bearing: Double = 0.0,
    val pitch: Double = 0.0,
)
```

当前实现状态需要明确一点：

- `bearing / pitch` 已经进入 native 相机朝向计算。
- `rotateEnabled / tiltEnabled` 已经升级成正式双指原生手势。

```kotlin
data class CesiumGestureOptions(
    val panEnabled: Boolean = true,
    val zoomEnabled: Boolean = true,
    val rotateEnabled: Boolean = false,
    val tiltEnabled: Boolean = false,
    val inertiaEnabled: Boolean = false,
)
```

```kotlin
data class CesiumRenderStats(
    val fps: Double,
    val drawMs: Double,
    val loadedTiles: Int,
    val selectedTiles: Int,
    val gpuMemoryMb: Double,
)
```

### 为什么 V1 先用 `zoom` 而不是 `height`

原因不是 `zoom` 更理想，而是它更贴近当前代码事实。

当前仓库里：

- Flutter 输入的是 `longitude`、`latitude`、`zoom`、`autoOrbit`
- native 侧接收的也是这组字段
- C++ 侧内部再把 `zoom` 映射为相机高度

所以 V1 若想尽快和现有实现对齐，先承诺 `zoom` 语义更稳。  
未来如果要升级到更标准的相机模型，再通过 V2 演进到 `height / heading / pitch / roll` 或等价语义收敛。

## Flutter SDK V1

### 公开类型

V1 建议 Flutter 侧只正式公开：

- `CesiumMapWidget`
- `CesiumMapController`
- `CesiumCameraState`
- `CesiumGestureOptions`
- `CesiumRenderStats`
- `CesiumMapListener`

### Flutter V1 接口

```dart
class CesiumMapWidget extends StatefulWidget {
  const CesiumMapWidget({
    super.key,
    this.initialCamera,
    this.gestureOptions,
    this.onMapReady,
    this.onCameraMoveStarted,
    this.onCameraChanged,
    this.onCameraIdle,
    this.onRenderStats,
    this.overlayBuilder,
  });

  final CesiumCameraState? initialCamera;
  final CesiumGestureOptions? gestureOptions;
  final VoidCallback? onMapReady;
  final ValueChanged<CesiumCameraState>? onCameraMoveStarted;
  final ValueChanged<CesiumCameraState>? onCameraChanged;
  final ValueChanged<CesiumCameraState>? onCameraIdle;
  final ValueChanged<CesiumRenderStats>? onRenderStats;
  final WidgetBuilder? overlayBuilder;
}
```

```dart
abstract class CesiumMapController {
  Future<void> setCamera(CesiumCameraState camera);
  Future<CesiumCameraState> getCameraState();

  Future<bool> getInteractionEnabled();
  Future<CesiumGestureOptions> getGestureOptions();

  Future<void> setInteractionEnabled(bool enabled);
  Future<void> setGestureOptions(CesiumGestureOptions options);

  Future<void> clearMemory();
  Future<CesiumRenderStats> getStats();
}
```

## 当前代码能力映射

下面这张表回答的是：V1 契约里哪些已经在当前仓库中有雏形。

| V1 能力 | 当前状态 | 现有依据 |
|---|---|---|
| `setCamera` | 已有雏形 | Flutter `updateCamera` + native `updateCamera` |
| `getCameraState` | 已有雏形 | Flutter `getCameraState` + native `getCameraState` |
| `setInteractionEnabled` | 已有正式入口 | Android/Flutter SDK 都已有接口与 channel，当前已接入原生触摸启停开关 |
| `setGestureOptions` | 已有正式入口 | Android/Flutter SDK 都已有接口与 channel，Android 当前已具备平移、缩放、双击缩放、惯性、双指旋转与双指俯仰 |
| `clearMemory` | 已有雏形 | Flutter `clearMemory` -> native `clearMemory` |
| `getStats` | 已有雏形 | native `snapshot()` + Flutter stats payload |
| `onMapReady` | 已有雏形 | native `mapReady` 事件已打通到 Flutter |
| `onCameraMoveStarted` | 已有正式入口 | Android SDK 与 Flutter 事件桥接已打通 |
| `onCameraChanged` | 已有正式入口 | native `cameraChanged` 事件已打通到 Flutter，并采用节流语义 |
| `onCameraIdle` | 已有正式入口 | Android SDK 与 Flutter 事件桥接已打通 |
| `onRenderStats` | 已有雏形 | 每秒 `stats` 回传 |
| `onError` | 已有雏形 | native `error` 事件已打通到 Flutter |

## 当前仓库最接近 V1 的现有契约

当前最接近 V1 的实际 channel 契约是：

- 命令：
  - `updateCamera`
  - `clearMemory`
- 事件：
  - `stats`

所以如果现在开始 SDK 化，最合理的做法不是新发明一大套 API，而是：

1. 先把这套最小命令/事件稳定下来
2. 再把 `error` 事件收敛成正式错误类型
3. 再补真实的交互选项接口
4. 再引入统一交互配置对象

## V1 明确延后项

这些能力建议明确放到 V2 或更后面：

- `animateCamera`
- `moveCamera`
- `fitBounds`
- `pick`
- `loadScene`
- `addTileset`
- `removeTileset`
- `setImagerySource`
- `setTerrainSource`
- `snapshot`
- `screenToWorld`
- `worldToScreen`
- `setDebugFlags`

原因很简单：

- 它们要么当前代码没有稳定基础
- 要么容易绑定未来的实现细节
- 要么会放大版本兼容成本

## V1 实施建议

如果按这个契约推进，代码实现顺序建议是：

1. 把当前 `updateCamera / clearMemory / stats` 重新包装成 SDK 语义
2. 新增正式的 `getCameraState`
3. 新增正式的 `onMapReady`
4. 增加统一错误回传面
5. 把 stats payload 收敛成正式 `CesiumRenderStats`

## 一句话结论

V1 不追求完整，而追求稳。  
真正应该先承诺给宿主的，是“可创建、可控相机、可清内存、可拿统计、可接错误”这一小圈能力。

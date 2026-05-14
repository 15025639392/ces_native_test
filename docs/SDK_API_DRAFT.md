# SDK API 草案

这份文档给出的是第一版 API 草案，目标不是一步到位，而是先形成稳定边界。

这里优先定义：

- Android SDK 面向宿主的 API
- Flutter 适配层面向 Flutter 宿主的 API

## Android SDK 草案

### 核心类型

建议至少有这些公开类型：

- `CesiumMapView`
- `CesiumMapFragment`
- `CesiumMapController`
- `CesiumCameraState`
- `CesiumGestureOptions`
- `CesiumSceneOptions`
- `CesiumRenderStats`

## Android API 草案

```kotlin
interface CesiumMapController {
    fun setCamera(camera: CesiumCameraState)
    fun animateCamera(camera: CesiumCameraState, durationMs: Long = 300)
    fun moveCamera(camera: CesiumCameraState)
    fun getCameraState(): CesiumCameraState

    fun setInteractionEnabled(enabled: Boolean)
    fun setGestureOptions(options: CesiumGestureOptions)
    fun setInteractionMode(mode: InteractionMode)

    fun loadScene(options: CesiumSceneOptions)
    fun addTileset(source: TilesetSource)
    fun removeTileset(id: String)
    fun setImagerySource(source: ImagerySource)
    fun setTerrainSource(source: TerrainSource)
    fun clearMemory()

    fun getStats(): CesiumRenderStats
    fun setListener(listener: CesiumMapListener?)
}
```

```kotlin
interface CesiumMapListener {
    fun onMapReady()
    fun onCameraChanged(state: CesiumCameraState)
    fun onSelectionChanged(selection: MapSelection?)
    fun onRenderStats(stats: CesiumRenderStats)
    fun onError(error: CesiumMapError)
}
```

## Android 数据类型草案

```kotlin
data class CesiumCameraState(
    val longitude: Double,
    val latitude: Double,
    val height: Double,
    val heading: Double = 0.0,
    val pitch: Double = -45.0,
    val roll: Double = 0.0,
)
```

```kotlin
data class CesiumGestureOptions(
    val panEnabled: Boolean = true,
    val zoomEnabled: Boolean = true,
    val rotateEnabled: Boolean = true,
    val tiltEnabled: Boolean = true,
    val inertiaEnabled: Boolean = true,
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

## Flutter 适配层草案

Flutter 建议暴露：

- `CesiumMapWidget`
- `CesiumMapController`
- `CesiumMapListener`
- `CesiumCameraState`
- `CesiumGestureOptions`
- `CesiumRenderStats`

## Flutter API 草案

```dart
class CesiumMapWidget extends StatefulWidget {
  const CesiumMapWidget({
    super.key,
    this.initialCamera,
    this.gestureOptions,
    this.onMapReady,
    this.onCameraChanged,
    this.onRenderStats,
    this.overlayBuilder,
  });

  final CesiumCameraState? initialCamera;
  final CesiumGestureOptions? gestureOptions;
  final VoidCallback? onMapReady;
  final ValueChanged<CesiumCameraState>? onCameraChanged;
  final ValueChanged<CesiumRenderStats>? onRenderStats;
  final WidgetBuilder? overlayBuilder;
}
```

```dart
abstract class CesiumMapController {
  Future<void> setCamera(CesiumCameraState camera);
  Future<void> animateCamera(CesiumCameraState camera, {Duration? duration});
  Future<void> moveCamera(CesiumCameraState camera);
  Future<CesiumCameraState> getCameraState();

  Future<void> setInteractionEnabled(bool enabled);
  Future<void> setGestureOptions(CesiumGestureOptions options);
  Future<void> clearMemory();

  Future<CesiumRenderStats> getStats();
}
```

## Flutter 设计原则

1. Flutter API 尽量贴近 Android SDK 的语义。
2. Flutter 主要做封装和参数映射，不发明第二套地图语义。
3. Overlay 能力是集成方式，不要让 Overlay 成为地图内核依赖。

## 第一阶段最小 API

为了降低实现风险，第一阶段建议只正式承诺这些能力：

- 相机设置和读取
- 手势开关
- 基础场景加载
- stats 回传
- 错误回传
- 清理内存

先不要急着承诺：

- 复杂选取系统
- 富图层编排
- 高级动画轨迹
- 复杂调试工具

## 后续可以补充的 API

- `pickScreenPoint`
- `screenToWorld`
- `worldToScreen`
- `snapshot`
- `setDebugFlags`
- `setPerformanceMode`
- `addOverlayAnchor`

## 一句话结论

第一版 SDK API 应该小、稳、对齐原生能力边界，先让宿主能稳定接入和驱动地图，再逐步扩展。

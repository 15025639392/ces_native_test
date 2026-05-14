# android-sdk

这里存放正式 Android SDK 的主路径源码。

当前已纳入这里的内容：

- 对外 Android 宿主入口：
  - `CesiumMapView`
  - `CesiumMapFragment`
  - `CesiumCameraState`
  - `CesiumMapListener`
  - `CesiumMapError`
  - `CesiumRenderStats`
- 渲染内部实现：
  - `internal/render/CesiumMapRenderSurface`
  - `NativeCesiumBridge`

Flutter 专用 `PlatformView` 适配层现在已经迁回 `sdk/flutter-plugin/android`，`android-sdk` 不再依赖 Flutter 编译环境。

当前已经可以直接在本目录构建：

```sh
./gradlew assemble
```

如需指定外部 Cesium Native 产物目录，可额外传入：

```sh
./gradlew assemble -PcesiumNativeRoot=/absolute/path/to/cesium-native-build
```

后续目标：

- 继续补齐正式 Android SDK API 面
- 让 `CesiumMapView` 成为 Android 宿主的一等入口
- 保持 Flutter `PlatformView` 只存在于 `flutter-plugin` 适配层内部

当前 `CesiumMapView` 已经具备的正式入口能力包括：

- `setCamera`
- `cameraState`
- `interactionEnabled`
- `gestureOptions`
- `getStats`
- `clearMemory`
- `setListener`
- `onCreate / onStart / onResume / onPause / onStop / onDestroy / onLowMemory`

当前 Android 宿主控制语义也开始统一到：

- `CesiumMapController`
- `CesiumMapView : CesiumMapController`
- `CesiumMapFragment.controller : CesiumMapController`

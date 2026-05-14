# 地球引擎 Bug 描述指南

这份文档用于把“我看到地图不对劲”翻译成 AI 或工程师能快速定位的问题描述。适用于 C++ / Cesium Native 内核、Android 原生 SDK、Flutter 适配层、3D 地球、瓦片地图、地形、3D Tiles、标注、相机和渲染问题。

## 通用 Bug 描述模板

```text
问题现象：
【用一句话描述你看到的异常，例如：缩放地球时瓦片闪烁；点击标注没有响应；相机飞行结束后位置偏移。】

发生场景：
【在哪个页面、哪种地图模式、加载了哪些图层或数据。】

复现步骤：
1. 打开【页面/场景】。
2. 加载【影像/地形/3D Tiles/标注/轨迹】。
3. 执行【旋转/缩放/点击/FlyTo/切换图层/进入后台再回来】。
4. 观察到【异常现象】。

预期结果：
【正常应该是什么样。】

实际结果：
【实际看到什么。】

影响范围：
【必现/偶现；Android/iOS/Flutter；某个机型；某个数据源；某个缩放级别；某个相机角度。】

相关模块：
【相机 / 渲染 / 瓦片 / 图层 / 拾取 / 标注 / 地形 / 3D Tiles / 生命周期 / Flutter 适配层 / JNI / 线程】

初步怀疑：
【如果不确定，可以写：怀疑与瓦片调度、深度测试、坐标转换、生命周期恢复、线程同步有关。】
```

## 更完整的 Bug 报告模板

当问题比较复杂时，用这个版本。它适合直接丢给 AI、同事或 issue 系统。

```text
标题：
【模块 + 现象 + 触发条件】
例如：Android Flutter 场景下，App resume 后地图黑屏，疑似 Surface/GL context 恢复异常

环境：
- SDK 类型：【Android Native SDK / Flutter SDK / C++ Core 单测 / Demo App】
- SDK 版本：【版本号 / commit id】
- 设备：【品牌、型号、CPU/GPU】
- 系统：【Android 版本】
- 渲染后端：【OpenGL ES / Vulkan / Metal / WebGL / 未知】
- Flutter 嵌入方式：【PlatformView / Texture / Hybrid Composition / 不涉及 Flutter】
- 数据源：【XYZ / WMTS / WMS / 3D Tiles / GeoJSON / 本地数据】
- 是否启用地形：【是/否】
- 是否启用 3D Tiles：【是/否】

问题现象：
【描述肉眼看到的异常，不要只写“异常”或“不对”。】

复现步骤：
1. 【启动页面】
2. 【加载数据】
3. 【执行操作】
4. 【观察异常】

预期结果：
【正常应该怎样。】

实际结果：
【实际怎样，最好包含截图/视频/日志。】

发生概率：
【必现 / 高概率 / 偶现 / 只在某机型出现】

影响范围：
【只影响 Flutter / 只影响 Android 原生 / 只影响某数据源 / 只影响某相机高度 / 只影响开启地形时】

相关状态：
- Camera position：
- Heading / Pitch / Roll：
- Zoom level / camera height：
- Layer count：
- Visible tile count：
- Pending request count：
- Tile cache size：
- Render FPS：
- Memory：

已有日志：
【贴 logcat、native crash stack、tile request log、render state log 等关键片段】

初步判断：
【怀疑属于相机/瓦片/渲染/拾取/生命周期/Flutter 适配层/线程同步/资源释放等模块】

希望 AI 帮忙：
【判断模块归属 / 给排查顺序 / 设计日志点 / 分析代码 / 写修复方案 / 生成最小复现】
```

## 快速判断属于哪个模块

| 你看到的现象 | 可能所属模块 | 专业描述 |
| --- | --- | --- |
| 地球拖不动、缩放不灵 | 相机模块 / 手势模块 | Camera Controller / Gesture Recognizer 异常 |
| 飞到某地后位置不准 | 相机模块 / 坐标模块 | FlyTo 目标坐标转换或相机插值异常 |
| 点击地图返回经纬度不对 | 拾取模块 / 坐标模块 | Ray Picking 或 Screen-to-Ray 反算异常 |
| 点击标注没反应 | 拾取模块 / 事件模块 | Object Picking 或 Event Dispatching 异常 |
| 地图瓦片加载不出来 | 瓦片模块 / 网络模块 | Tile Request / Tile Provider / Cache 异常 |
| 缩放时瓦片闪烁 | 瓦片模块 / 渲染模块 | Tile LOD 切换、Tile Refinement 或纹理绑定异常 |
| 远处瓦片突然消失 | 瓦片模块 / 裁剪模块 | Frustum Culling、Horizon Culling 或 SSE 计算异常 |
| 地形有裂缝 | 地形模块 | Terrain Mesh LOD seam、Tile Skirt 或 crack fixing 异常 |
| 模型被地面遮住或漂浮 | 3D Tiles / 坐标 / 高程模块 | Height Reference、model matrix 或 terrain sampling 异常 |
| 标注被错误遮挡 | 标注模块 / 深度模块 | Depth Test、Occlusion Policy 或 Billboard depth state 异常 |
| 文字标签重叠 | 标注模块 | Label Collision Avoidance 异常 |
| 黑屏、白屏、花屏 | 渲染模块 / 生命周期 | Rendering Context、Surface、Render Thread 或 GPU 资源异常 |
| App 回到前台后地图不动 | 生命周期 / 渲染宿主 | Surface restore、render loop resume 或 GL context 恢复异常 |
| Flutter 页面切换后崩溃 | Flutter 适配层 / 原生 SDK | PlatformView lifecycle、Texture detach 或 MethodChannel 调用时序异常 |
| 偶发崩溃 | 线程 / JNI / 资源释放 | Race Condition、Use-after-free、JNI reference 或生命周期释放异常 |

## 模块归属速查表

| Bug 关键词 | 一级模块 | 二级模块 | 常见根因 |
| --- | --- | --- | --- |
| 黑屏 | 渲染模块 / 生命周期 | Surface、GL Context、Render Loop | Surface 丢失、上下文未恢复、渲染线程暂停后未恢复、Framebuffer 无效 |
| 白屏 | 渲染模块 / 数据加载 | Clear Pass、Layer Loading | 没有可渲染图层、瓦片未加载、相机看向空区域、背景清除后没有 draw command |
| 花屏 | 渲染模块 | Texture、Buffer、Shader | 纹理上传错、buffer 越界、shader precision 问题、GPU 资源已释放仍被使用 |
| 闪烁 | 瓦片模块 / 渲染模块 | LOD、Refinement、Texture Binding | 父子瓦片切换时机错、纹理未就绪、缓存淘汰过早、透明混合顺序异常 |
| 抖动 | 坐标模块 / 渲染模块 | Precision、Matrix、Depth | 浮点精度不足、相机矩阵抖动、z-fighting、缺少 relative-to-center |
| 偏移 | 坐标模块 | CRS、Transform、Projection | WGS84/GCJ-02/BD-09 混用、lon/lat 顺序错、model matrix 错、投影参数错 |
| 错位 | 瓦片模块 / 坐标模块 | Tile Scheme、UV、Projection | XYZ/TMS y 轴反了、瓦片范围计算错、纹理 UV 错、WebMercator 转换错 |
| 消失 | 裁剪模块 / LOD | Frustum、Horizon、Occlusion、SSE | 裁剪误判、包围体错误、SSE 阈值过严、相机 far plane 不合理 |
| 穿透 | 相机模块 / 深度模块 | Collision、Depth Test | 相机碰撞检测缺失、depth test 关闭、depth write 错、near/far plane 不合理 |
| 点不中 | 拾取模块 / 事件模块 | Ray Picking、Hit Test、Event Dispatch | 射线反算错、pick pass 漏对象、事件被 Flutter 或父 View 拦截 |
| 卡顿 | 性能模块 | Main Thread、Render Thread、Tile Loading | 主线程阻塞、纹理上传集中、draw call 太多、瓦片解析没有放后台 |
| 崩溃 | 稳定性模块 | Native、JNI、Lifecycle、Threading | 空指针、use-after-free、线程竞态、JNI 引用失效、资源释放时序错 |
| 内存涨 | 资源模块 | Cache、Texture、Native Object | 瓦片缓存不淘汰、纹理未释放、监听器泄漏、native 对象生命周期不清 |

## 按模块采集什么信息

| 模块 | 需要记录的状态 | 推荐日志关键词 |
| --- | --- | --- |
| 相机模块 | position、direction、up、heading、pitch、roll、height、target、view matrix、projection matrix | CameraState、FlyTo、GestureDelta、ViewMatrix、ProjectionMatrix |
| 坐标模块 | 输入 lon/lat/height、输出 ECEF、CRS、投影类型、地形高度、model matrix | CoordinateTransform、WGS84、ECEF、CRS、ModelMatrix、TerrainHeight |
| 拾取模块 | screen x/y、ray origin、ray direction、hit object id、hit lon/lat/height、pick pass 是否执行 | Picking、ScreenToRay、RayHit、DepthPick、ColorPick、HitTest |
| 瓦片模块 | z/x/y、tile state、request URL、HTTP code、retry count、priority、SSE、visible tile count | TileRequest、TileState、TileCache、TileSelection、SSE、Refinement |
| 图层模块 | layer id、type、visible、opacity、order、provider、style、数据源 URL | LayerManager、LayerState、LayerOrder、Opacity、DataProvider |
| 渲染模块 | render backend、frame id、draw call、render pass、shader compile、depth/blend state、framebuffer status | Renderer、RenderPass、DrawCommand、DepthState、BlendState、Framebuffer、Shader |
| 地形模块 | terrain provider、tile level、height range、mesh vertex count、skirt、sampling result | TerrainProvider、TerrainMesh、ElevationSample、TileSkirt、HeightRange |
| 3D Tiles 模块 | tileset URL、tile id、geometric error、transform、bounding volume、content state、memory usage | Tileset、TileContent、GeometricError、BoundingVolume、RTC、glTF |
| Android 宿主 | Activity/Fragment/View lifecycle、Surface created/destroyed、render thread start/stop | MapView、SurfaceCreated、SurfaceDestroyed、onResume、onPause、RenderThread |
| Flutter 适配层 | PlatformView attach/detach、Texture id、MethodChannel 调用、EventChannel 订阅、route 生命周期 | PlatformView、TextureId、MethodChannel、EventChannel、FlutterLifecycle |
| 线程模块 | 当前线程名、任务队列长度、锁等待、回调线程、native pointer 生命周期 | Threading、TaskQueue、Mutex、CallbackThread、NativeHandle |

## 排查顺序建议

### 黑屏 / 白屏 / 花屏

1. 先确认生命周期：`onResume`、`SurfaceCreated`、`RenderThread` 是否正常恢复。
2. 再确认渲染上下文：GL/Vulkan context 是否有效，Framebuffer 是否 complete。
3. 检查相机：相机是否看向地球，near/far plane 是否合理。
4. 检查图层：是否有 visible layer，瓦片是否 loaded。
5. 检查渲染命令：当前帧 draw command 数量是否为 0。
6. 检查 shader 和纹理：shader 是否编译成功，纹理是否上传成功。

### 瓦片不加载 / 错位 / 闪烁

1. 打印 tile URL，确认 z/x/y 是否正确。
2. 确认瓦片方案：XYZ、TMS、WMTS 的坐标规则是否匹配。
3. 检查 HTTP 状态码、超时、重试和取消请求。
4. 打印 tile state，从 unloaded 到 loaded 的状态流是否完整。
5. 检查 LOD 选择：SSE、camera height、zoom level 映射是否合理。
6. 检查 refinement：父瓦片是否过早隐藏，子瓦片是否未就绪就替换。
7. 检查缓存：tile cache 是否过小或淘汰策略过激。

### 坐标偏移 / 点击不准 / 标注偏移

1. 确认输入坐标顺序是 lon/lat 还是 lat/lon。
2. 确认坐标系：WGS84、GCJ-02、BD-09、WebMercator 是否混用。
3. 打印 lon/lat/height 到 ECEF 的转换结果。
4. 检查 model matrix、local ENU transform、RTC center 是否正确。
5. 对拾取问题，打印 screen-to-ray 的 origin 和 direction。
6. 对地形拾取，比较 ray-ellipsoid 和 ray-terrain 的结果差异。
7. 如果只在中国底图上偏移，优先怀疑火星坐标系转换问题。

### 标注显示异常 / 被遮挡 / 重叠

1. 确认标注使用的是地理坐标、屏幕坐标还是混合坐标。
2. 检查 height reference：absolute、clamp to ground、relative to ground。
3. 检查 depth test 是否开启，是否需要 disable depth test distance。
4. 检查 billboard 是否始终面向相机。
5. 检查 label collision avoidance 是否开启或误判。
6. 检查聚合 clustering 是否在当前 zoom 下错误合并。
7. 检查 Flutter Overlay 与原生地图层级是否正确。

### 崩溃 / 偶现 / 生命周期问题

1. 先看堆栈：区分 Java/Kotlin 崩溃、Dart 崩溃、native crash。
2. 如果是 native crash，优先检查空指针、use-after-free、数组越界和线程竞态。
3. 如果发生在页面退出，检查 destroy 后是否仍有 native 回调。
4. 如果发生在后台恢复，检查 Surface、Texture、GL context 的释放和重建。
5. 如果 Flutter 场景出现，检查 PlatformView detach、Texture release、MethodChannel 回调时序。
6. 打印 native handle 创建/销毁日志，确认没有重复释放或释放后使用。
7. 对偶现问题，加线程名、对象 id、生命周期阶段和任务队列日志。

## 日志设计模板

### 相机日志

```text
[CameraState]
frameId=12345
positionEcef=(x, y, z)
lonLatHeight=(lon, lat, height)
heading=...
pitch=...
roll=...
near=...
far=...
viewMatrix=...
projectionMatrix=...
```

### 瓦片日志

```text
[TileRequest]
tileId=z/x/y
url=...
priority=...
sse=...
state=loading -> loaded
httpCode=200
fromCache=false
cancelled=false
loadTimeMs=...
```

### 渲染日志

```text
[RenderFrame]
frameId=12345
backend=OpenGLES
surfaceValid=true
framebufferComplete=true
drawCommandCount=...
visibleTileCount=...
renderPasses=globe,terrain,tileset,billboard,overlay
depthTest=true
blend=true
fps=...
```

### 拾取日志

```text
[Picking]
screen=(x, y)
rayOrigin=(x, y, z)
rayDirection=(x, y, z)
hit=true
hitType=terrain/model/billboard/ellipsoid
objectId=...
lonLatHeight=(lon, lat, height)
```

### 生命周期日志

```text
[MapLifecycle]
event=onResume/onPause/onDestroy/surfaceCreated/surfaceDestroyed
mapViewId=...
nativeHandle=...
surfaceValid=...
renderThreadState=running/stopped
textureId=...
```

## 常见根因词典

| 英文术语 | 中文含义 | 常见表现 |
| --- | --- | --- |
| Race Condition | 线程竞态 | 偶现崩溃、偶现状态错乱、日志顺序不稳定 |
| Use-after-free | 释放后使用 | 页面销毁后崩溃、native 指针异常、随机崩溃 |
| Null Pointer | 空指针 | 稳定崩溃，堆栈通常比较明确 |
| Memory Leak | 内存泄漏 | 多次进入退出后内存持续增长 |
| Resource Leak | 资源泄漏 | 纹理、buffer、Surface、文件句柄没有释放 |
| State Desync | 状态不同步 | Flutter UI 状态和原生地图状态不一致 |
| Stale State | 旧状态残留 | 切换图层后旧瓦片、旧标注、旧事件还存在 |
| Invalid Context | 无效渲染上下文 | 黑屏、花屏、resume 后无法渲染 |
| Precision Loss | 精度损失 | 大尺度场景抖动、偏移、z-fighting |
| Coordinate Mismatch | 坐标系不匹配 | 底图和标注不重合、模型漂移 |
| Incorrect Culling | 裁剪误判 | 对象突然消失、远处瓦片缺失 |
| Wrong Render State | 渲染状态错误 | 遮挡错、透明错、深度错、混合错 |
| Cache Invalidation Bug | 缓存失效错误 | 切换数据后旧数据残留或新数据不刷新 |
| Backpressure | 背压 | 事件太多处理不过来，导致延迟、丢事件、卡顿 |

## 最小复现应该怎么缩小

| 原始复杂场景 | 缩小方式 |
| --- | --- |
| 多图层同时闪烁 | 只保留一个影像图层，关闭地形、标注和 3D Tiles |
| 3D Tiles 和地形高度不对 | 只加载地形，再只加载 3D Tiles，分别验证高度 |
| Flutter 页面崩溃 | 先用 Android 原生 Demo 复现，再接入 Flutter Plugin |
| 点击标注不响应 | 先只放一个 Marker，关闭聚合、Overlay、复杂手势 |
| 瓦片错位 | 固定一个 z/x/y，直接打印 URL 和瓦片边界 |
| 卡顿 | 关闭 3D Tiles、关闭地形、关闭标注，逐个打开找触发源 |
| 偶现崩溃 | 加生命周期和线程日志，循环进入退出页面 50 次 |
| 黑屏 | 用纯色 clear + 一个三角形验证渲染宿主，再加载地球 |

## 相机类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 拖动地球方向反了 | Orbit control direction is inverted | 拖拽地球时，相机 orbit 方向与手势方向相反，怀疑 Camera Controller 的旋转增量符号或坐标轴约定错误。 |
| 缩放忽快忽慢 | Zoom speed is unstable | 双指缩放或滚轮缩放时 zoom speed 不稳定，怀疑缩放倍率与相机高度、地表距离或手势 delta 映射不合理。 |
| 飞行动画绕远路 | Camera flight takes an unexpected path | FlyTo 动画路径异常，疑似 heading/pitch 插值、地球曲面插值或目标点计算错误。 |
| 相机穿进地球 | Camera clips through the globe | 相机可以穿入地球或地形，怀疑缺少 camera collision detection 或 terrain collision avoidance。 |
| 倾斜到某个角度卡住 | Camera pitch is clamped incorrectly | pitch 限制异常，导致倾斜视角无法继续调整或突然跳变。 |
| 飞行结束有偏移 | Camera final position is offset | FlyTo 完成后相机没有准确对准目标经纬度，怀疑 WGS84 到 ECEF 转换、目标高度或 view matrix 更新有误。 |

## 坐标与拾取类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 点击位置经纬度偏移 | Coordinate picking returns offset lon/lat | 点击地球表面返回的经纬度与实际位置有偏移，怀疑 screen-to-ray、ray-ellipsoid intersection 或相机矩阵没有同步。 |
| 高度返回不对 | Picked height is incorrect | 坐标拾取返回的 height 不符合地形高度，怀疑 ray-terrain intersection 或 terrain elevation sampling 异常。 |
| 点标注落点偏了 | Marker is geospatially misaligned | 点标注显示位置与输入经纬度不一致，怀疑 lon/lat 顺序、坐标系、ECEF 转换或 model matrix 错误。 |
| 中国地图偏移 | Map imagery has China offset issue | 国内地图底图与 WGS84 数据存在偏移，可能涉及 GCJ-02、BD-09、WGS84 坐标系转换问题。 |
| 点击模型选不中 | 3D object picking fails | 点击 3D Tiles 或模型无法拾取对象，怀疑 object picking、color picking、depth picking 或 pick pass 没有包含该对象。 |

## 瓦片与图层类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 瓦片空白 | Tile is blank or missing | 部分地图瓦片显示为空白，怀疑 tile URL、z/x/y 坐标、TMS/XYZ y 轴方向、网络请求或缓存异常。 |
| 瓦片错位 | Tile misalignment | 相邻瓦片边界对不上，怀疑 tile scheme、projection、tile coordinate 或 texture UV 计算错误。 |
| 缩放级别不对 | Wrong zoom level selected | 当前相机高度下加载的瓦片层级不正确，怀疑 LOD selection、screen space error 或 zoom level mapping 异常。 |
| 瓦片频繁闪烁 | Tile flickering during navigation | 相机移动时瓦片频繁闪烁，怀疑 tile refinement、父子瓦片替换时机、缓存淘汰或纹理上传时序异常。 |
| 旧瓦片残留 | Stale tiles remain visible | 切换图层或移动相机后旧瓦片仍然残留，怀疑 tile cache invalidation 或 layer visibility state 没有正确刷新。 |
| 图层顺序错乱 | Layer order is incorrect | 多个图层叠加顺序不正确，怀疑 layer z-order、render pass order 或 blending state 异常。 |
| 透明度无效 | Layer opacity is ignored | 设置图层透明度后没有生效，怀疑 material alpha、blending state 或 shader uniform 没有更新。 |

## 渲染类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 黑屏 | Render output is black | 地图区域黑屏，怀疑 rendering context、shader compilation、framebuffer、camera frustum 或 render loop 异常。 |
| 白屏 | Render output is blank white | 地图区域白屏，怀疑 Surface 创建、背景清除、图层未加载或 GPU resource 初始化失败。 |
| 花屏 | Rendering artifacts / visual corruption | 出现花屏或异常纹理，怀疑 texture upload、GPU buffer、shader precision 或资源生命周期错误。 |
| 模型前后遮挡错 | Incorrect depth ordering | 三维对象前后遮挡关系错误，怀疑 depth test、depth write、depth function 或 render pass 顺序异常。 |
| 半透明物体显示错 | Transparency sorting issue | 半透明图层或模型显示顺序异常，怀疑 alpha blending、transparent object sorting 或 depth write 设置错误。 |
| 地球边缘抖动 | Z-fighting or precision jitter | 地球边缘或贴地对象抖动，怀疑 floating-point precision、z-fighting、logarithmic depth buffer 或 relative-to-center rendering 问题。 |
| 贴地线闪烁 | Ground polyline z-fighting | 贴地线与地形表面发生 z-fighting，怀疑深度偏移、polygon offset 或贴地高度计算问题。 |
| 标注被地球挡住 | Billboard depth test issue | Billboard 或 Label 被地球错误遮挡，怀疑 depth test state、disableDepthTestDistance 或 occlusion policy 设置问题。 |

## 地形与 3D Tiles 类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 地形裂开 | Terrain cracks between LOD tiles | 不同 LOD 地形瓦片边界出现裂缝，怀疑 tile skirt、crack fixing 或边界高程采样不一致。 |
| 地形高度异常 | Terrain elevation is incorrect | 山体高度明显不对，怀疑 DEM 解码、height scale、height offset 或 terrain provider 数据格式解析错误。 |
| 模型漂浮 | 3D Tileset floats above terrain | 3D Tiles 模型漂浮在地表上方，怀疑 tileset transform、height reference、terrain sampling 或坐标系转换错误。 |
| 模型陷入地下 | 3D Tileset is below terrain | 模型低于地面，怀疑 model matrix、RTC center、georeference 或高度基准不一致。 |
| 3D Tiles 加载很慢 | 3D Tiles streaming is slow | 3D Tiles 加载慢，怀疑 request scheduler、SSE threshold、cache size、网络并发或 glTF 解码性能问题。 |
| 倾斜摄影破洞 | Reality mesh has holes | 倾斜摄影模型出现破洞，可能是瓦片缺失、LOD 切换、纹理缺失或几何解码失败。 |

## Android 原生 SDK 类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 进入后台再回来黑屏 | Black screen after resume | App 从后台恢复后地图黑屏，怀疑 Surface/Texture、GL context、render thread 或 native resource restore 异常。 |
| 旋转屏幕后崩溃 | Crash after configuration change | 屏幕旋转或 Activity 重建后崩溃，怀疑 MapView lifecycle、native pointer、Surface 释放和重建时序异常。 |
| 销毁页面后还在渲染 | Render loop continues after destroy | 页面销毁后渲染线程仍在运行，怀疑 lifecycle binding、render loop stop 或 native resource release 缺失。 |
| 多次进入页面内存增长 | Memory leak after repeated navigation | 多次进入退出地图页面后内存持续增长，怀疑 native resource、texture、tile cache 或 listener 没有释放。 |
| 手势和外层滚动冲突 | Gesture conflict with parent view | 地图手势与外层 ScrollView/ViewPager 冲突，怀疑 touch event interception 或 gesture arbitration 策略不正确。 |

## Flutter 适配层类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| Flutter 页面返回后崩溃 | Crash after Flutter route pop | Flutter 路由 pop 后原生地图崩溃，怀疑 PlatformView detach、native MapView destroy 或 MethodChannel 回调时序问题。 |
| 地图上方 Flutter UI 点不动 | Flutter overlay does not receive touch events | Flutter Overlay 无法接收点击事件，怀疑 PlatformView 命中测试、手势竞争或事件透传策略异常。 |
| 地图手势被 Flutter 抢走 | Map gestures are intercepted by Flutter | 地图拖拽或缩放被 Flutter 外层组件拦截，怀疑 gesture arena、PlatformView gesture recognizers 或触摸事件分发配置问题。 |
| Flutter 热重载后地图异常 | Map state is broken after hot reload | Flutter hot reload 后地图状态异常，怀疑插件生命周期、native view 复用或 channel 绑定状态没有恢复。 |
| MethodChannel 调用无响应 | MethodChannel call hangs or times out | Flutter 调用原生地图 API 无响应，怀疑 channel 没有注册、线程切换错误或 native 方法阻塞。 |
| EventChannel 事件丢失 | EventChannel events are dropped | 相机变化、点击、加载状态事件偶发丢失，怀疑 listener 注册时机、线程调度或事件背压处理不正确。 |

## 崩溃与性能类 Bug

| 现象 | 专业说法 | 可复制描述 |
| --- | --- | --- |
| 偶发 native 崩溃 | Intermittent native crash | 偶发 C++ native 崩溃，怀疑 use-after-free、null pointer、race condition 或 JNI 生命周期问题。 |
| Java/Kotlin 崩溃 | Android runtime crash | Android 层崩溃，需提供 logcat 堆栈，重点检查 MapView 生命周期、线程调用和空对象访问。 |
| ANR | Application Not Responding | 地图操作导致 ANR，怀疑主线程执行瓦片解析、网络请求、同步等待或重型 JNI 调用。 |
| 掉帧 | Frame drops / jank | 操作地图时掉帧，怀疑 tile loading、texture upload、draw call 过多、shader 编译或主线程阻塞。 |
| 发热严重 | Thermal throttling / high GPU usage | 长时间使用地图发热严重，怀疑 render loop 未限帧、不可见时仍渲染、瓦片加载过多或 GPU 负载过高。 |
| 内存暴涨 | Memory spike / memory leak | 地图加载后内存持续增长，怀疑 tile cache、texture cache、3D Tiles geometry、native object 或 listener 泄漏。 |
| 首屏慢 | Slow first render | 地图首次显示很慢，怀疑初始化链路、shader warmup、首批瓦片请求、地形加载或主线程阻塞。 |

## 给 AI 的 Bug 分析提示词

### 通用分析

```text
下面是一个移动端 3D 地球引擎 Bug。请从模块边界角度分析可能原因，重点区分 C++/Cesium Native Core、Android Native SDK、Flutter Adapter、渲染模块、瓦片模块、相机模块、拾取模块和生命周期模块各自可能的问题。请给出排查顺序、需要增加的日志、可能的修复方向和最小复现建议。

Bug 描述：
【粘贴你的问题现象和复现步骤】
```

### 渲染问题

```text
这是一个 3D 地球引擎渲染 Bug。请重点检查 rendering pipeline、render state、depth test、depth write、blending、framebuffer、shader、texture upload、render pass order、GPU resource lifecycle 和 Surface/GL context 生命周期。请判断问题最可能属于哪个渲染子模块，并给出排查步骤。
```

### 瓦片问题

```text
这是一个地图瓦片加载或显示 Bug。请重点检查 tile coordinates、XYZ/TMS scheme、tile provider、request scheduler、tile cache、LOD selection、screen space error、tile refinement、visible tile set、texture binding 和 layer state。请给出可能原因和日志点。
```

### 相机和拾取问题

```text
这是一个相机或拾取 Bug。请重点检查 camera state、view matrix、projection matrix、screen-to-ray、ray-ellipsoid intersection、ray-terrain intersection、coordinate conversion、heading/pitch/roll、FlyTo interpolation 和 gesture mapping。请给出可能原因和验证方法。
```

### Android / Flutter 生命周期问题

```text
这是一个 Android 原生 SDK 与 Flutter 适配层的生命周期 Bug。架构是 Native-first：C++/Cesium Native Core 是内核，Android Native SDK 是一等公民，Flutter 只是适配层。请重点检查 MapView lifecycle、Surface/Texture attach/detach、render thread start/stop、native resource ownership、PlatformView lifecycle、MethodChannel/EventChannel 调用时序和页面销毁后的回调问题。
```

## 描述 Bug 时尽量提供的信息

| 信息 | 为什么重要 |
| --- | --- |
| 设备型号和 Android 版本 | GPU、系统 WebView、Surface 行为可能不同 |
| SDK 版本和 Git commit | 方便对照最近改动 |
| 是否 Flutter 场景 | 区分原生 SDK 问题还是 Flutter 适配问题 |
| 使用 PlatformView 还是 Texture | 影响渲染宿主和事件分发 |
| 是否启用地形 | 地形会影响拾取、高度、遮挡和相机碰撞 |
| 是否加载 3D Tiles | 影响内存、渲染、深度、拾取和 LOD |
| 数据源 URL 或瓦片规则 | 排查 XYZ/TMS/WMTS 坐标问题 |
| 相机位置和缩放级别 | 影响 LOD、裁剪、SSE 和精度 |
| 复现视频或截图 | 视觉类 Bug 很依赖现象判断 |
| logcat / native crash stack | 崩溃和 ANR 必须看堆栈 |
| 是否必现 | 区分逻辑错误、竞态问题或资源时序问题 |

## 不够专业的描述和更好的描述

| 不够专业 | 更好的描述 |
| --- | --- |
| 地图坏了 | 地图区域黑屏，怀疑渲染上下文或 Surface 恢复异常 |
| 点不准 | 经纬度标注显示位置偏移，怀疑坐标系转换或 lon/lat 顺序错误 |
| 图闪 | 缩放时瓦片频繁闪烁，怀疑 LOD 切换或父子瓦片 refinement 时机错误 |
| 点不了 | 点击标注没有触发事件，怀疑 object picking 或事件分发异常 |
| 卡 | 拖拽地图时出现 jank，怀疑主线程阻塞、纹理上传或瓦片解析过重 |
| 乱了 | 图层叠加顺序异常，怀疑 layer order、render pass order 或 blending state 错误 |
| 回来黑了 | App 从后台恢复后地图黑屏，怀疑 Surface/GL context 生命周期恢复异常 |

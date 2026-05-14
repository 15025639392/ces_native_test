# 地球引擎开发 AI 术语表

这份文档用于把“我想做一个地球引擎”翻译成 AI 更容易理解的专业描述。你可以把相关段落直接复制到开发提示词里。

## 一句话描述

我想开发一个类似 Cesium、Google Earth 或 Mapbox Globe 的 3D 地球引擎，支持地球球体渲染、经纬度坐标、地图瓦片、地形高程、相机控制、图层叠加、标注、轨迹、测量和大规模地理数据可视化。

## 核心专业名词

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 地球引擎 | Earth Engine / Globe Engine | 一个用于渲染和交互 3D 地球的图形与地理信息引擎 |
| 数字地球 | Digital Earth | 将地球表面、地形、影像、矢量数据和实时数据整合到可交互三维场景中 |
| 三维地球 | 3D Globe | 基于球体或椭球体模型显示地球 |
| 地理信息系统 | GIS, Geographic Information System | 用于处理、分析和展示地理空间数据的系统 |
| 地理空间数据 | Geospatial Data | 带有地理位置坐标的数据，例如点、线、面、影像、地形 |
| 坐标参考系统 | CRS, Coordinate Reference System | 定义坐标如何对应到地球表面的规则 |
| 地图投影 | Map Projection | 将地球曲面转换为平面地图的数学方法 |
| 椭球体模型 | Ellipsoid Model | 用近似椭球体表示地球形状，常用 WGS84 |
| 瓦片地图 | Tiled Map | 将地图切成多级小图片或数据块，按需加载 |
| 层级细节 | LOD, Level of Detail | 根据相机距离动态切换数据精度 |
| 四叉树 | Quadtree | 地图瓦片和地形瓦片常用的空间分层结构 |
| 地形高程 | Terrain Elevation | 地表高度数据，用于生成山脉、峡谷等起伏地形 |
| 正射影像 | Orthophoto / Orthophoto Imagery | 经过几何校正的卫星或航拍影像 |
| 影像图层 | Imagery Layer | 显示卫星图、街道图、夜景图等图片瓦片的图层 |
| 矢量图层 | Vector Layer | 显示点、线、面、行政区、道路、边界等矢量数据的图层 |
| 3D Tiles | 3D Tiles | Cesium 提出的三维地理空间数据流式加载格式，适合城市模型、倾斜摄影、BIM |
| 倾斜摄影 | Oblique Photogrammetry | 通过多角度航拍生成真实城市三维模型 |
| 点云 | Point Cloud | 由大量三维点组成的数据，常来自激光雷达 LiDAR |
| 相机控制 | Camera Controls | 控制视角旋转、缩放、平移、飞行定位 |
| 拾取 | Picking | 鼠标点击场景对象并获取对象信息 |
| 视锥裁剪 | Frustum Culling | 只渲染相机可见范围内的对象 |
| 地平线裁剪 | Horizon Culling | 在地球曲面背后的对象不渲染，提高性能 |
| 遮挡剔除 | Occlusion Culling | 被其他物体挡住的对象不渲染 |
| 流式加载 | Streaming | 根据视角和距离动态加载远程瓦片、地形或模型 |
| 空间索引 | Spatial Index | 用于快速查询地理对象位置关系的数据结构 |

## 引擎架构相关术语

| 中文 | 英文 | 用途 |
| --- | --- | --- |
| 渲染引擎 | Rendering Engine | 负责 WebGL、WebGPU、Three.js 或原生图形 API 渲染 |
| 场景图 | Scene Graph | 管理地球、图层、模型、标注、光照、相机等对象 |
| 地球场景 | Globe Scene | 以地球为核心的三维场景 |
| 图层系统 | Layer System | 管理影像、地形、矢量、模型、标注等不同数据层 |
| 数据源适配器 | Data Source Adapter | 接入不同地图服务和数据格式的模块 |
| 瓦片调度器 | Tile Scheduler | 决定哪些瓦片需要加载、卸载、缓存和渲染 |
| 资源缓存 | Resource Cache | 缓存图片、地形、模型、纹理，减少重复请求 |
| 任务队列 | Task Queue | 控制异步加载、解析、解码、渲染任务 |
| 事件系统 | Event System | 处理鼠标、触摸、键盘、相机变化、图层变化等事件 |
| 插件系统 | Plugin System | 允许扩展测量、绘制、分析、标注等功能 |

## 坐标与数学

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| 经纬度 | Longitude / Latitude | 地理坐标，通常为 lon、lat |
| 高度 | Altitude / Height | 离椭球面或海平面的高度 |
| WGS84 | WGS84 | 最常见的全球坐标系统 |
| ECEF 坐标 | Earth-Centered, Earth-Fixed | 以地球中心为原点的三维笛卡尔坐标 |
| ENU 坐标 | East-North-Up | 以某个地表点为局部原点的东、北、上坐标系 |
| 笛卡尔坐标 | Cartesian Coordinates | x、y、z 三维坐标 |
| 球面坐标 | Spherical Coordinates | 半径、方位角、俯仰角形式的坐标 |
| 大圆距离 | Great-circle Distance | 球面两点之间最短路径距离 |
| 地理包围盒 | Geographic Bounding Box | 经纬度范围，例如 west、south、east、north |
| 包围球 | Bounding Sphere | 用球体包围对象，用于裁剪和碰撞检测 |
| 包围盒 | Bounding Box | 用矩形盒包围对象，用于空间查询和裁剪 |
| 矩阵变换 | Matrix Transform | 用矩阵实现平移、旋转、缩放、坐标转换 |
| 四元数 | Quaternion | 用于稳定表示三维旋转 |

## 地图瓦片与服务

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| XYZ 瓦片 | XYZ Tiles | 常见地图瓦片规则，使用 z/x/y 访问 |
| TMS 瓦片 | Tile Map Service | 与 XYZ 类似，但 y 轴方向可能相反 |
| WMTS | Web Map Tile Service | OGC 标准地图瓦片服务 |
| WMS | Web Map Service | OGC 标准动态地图图片服务 |
| WFS | Web Feature Service | OGC 标准矢量要素服务 |
| GeoJSON | GeoJSON | 常见地理矢量数据格式 |
| TopoJSON | TopoJSON | 拓扑压缩后的地理矢量格式 |
| KML | Keyhole Markup Language | Google Earth 常用地理标注格式 |
| CZML | Cesium Language | Cesium 用于动态场景和时间序列数据的 JSON 格式 |
| MVT | Mapbox Vector Tile | Mapbox 矢量瓦片格式 |
| DEM | Digital Elevation Model | 数字高程模型 |
| DSM | Digital Surface Model | 包含建筑、树木等地表物体高度的表面模型 |
| DTM | Digital Terrain Model | 更偏裸地地形的数字地形模型 |

## 渲染相关术语

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| WebGL | WebGL | 浏览器里的 GPU 图形 API |
| WebGPU | WebGPU | 新一代浏览器 GPU API |
| Three.js | Three.js | 常用 Web 3D 渲染库 |
| 着色器 | Shader | 在 GPU 上运行的渲染程序 |
| 顶点着色器 | Vertex Shader | 处理顶点位置、法线、纹理坐标 |
| 片元着色器 | Fragment Shader | 计算像素颜色 |
| 纹理映射 | Texture Mapping | 把地图影像贴到地球或地形表面 |
| 法线 | Normal | 表示表面朝向，用于光照计算 |
| 大气散射 | Atmospheric Scattering | 模拟地球大气层蓝色边缘和日照效果 |
| 昼夜效果 | Day-night Terminator | 根据太阳方向显示白天和黑夜分界 |
| 深度缓冲 | Depth Buffer | 判断前后遮挡关系 |
| 对数深度缓冲 | Logarithmic Depth Buffer | 缓解大尺度场景的深度精度问题 |
| 浮点精度问题 | Floating-point Precision Issue | 地球尺度很大时，GPU 坐标容易抖动或闪烁 |
| 原点重定位 | Origin Rebasing | 将局部坐标原点移动到相机附近，减少精度问题 |
| 相对中心渲染 | Relative-to-center Rendering | 大坐标拆分成中心点加局部偏移，提高精度 |

## 地形与三维数据

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| 地形网格 | Terrain Mesh | 由高程数据生成的三角网格 |
| 高程采样 | Elevation Sampling | 查询某个经纬度处的地形高度 |
| 地形夸张 | Terrain Exaggeration | 放大地形起伏，便于观察 |
| 裂缝修补 | Crack Fixing / Skirt | 不同 LOD 地形瓦片边界之间的缝隙处理 |
| 瓦片裙边 | Tile Skirt | 地形瓦片边缘向下延伸的几何体，用于遮住裂缝 |
| 三角网 | TIN, Triangulated Irregular Network | 用不规则三角形表达地形表面 |
| 城市白模 | White Model / Massing Model | 简化建筑体块模型 |
| 实景三维 | Reality Mesh / 3D Reality Model | 基于摄影测量生成的真实三维场景 |
| BIM 模型 | Building Information Model | 建筑信息模型，可与地理场景融合 |

## 交互功能术语

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| 旋转地球 | Orbit / Rotate Globe | 鼠标拖拽旋转地球 |
| 缩放 | Zoom | 调整相机距离 |
| 平移 | Pan | 移动观察中心 |
| 飞行定位 | FlyTo / Camera Flight | 平滑飞到指定经纬度或对象 |
| 视角锁定 | Camera Lock | 相机跟随某个对象 |
| 鼠标拾取 | Mouse Picking | 点击获取对象、坐标或图层信息 |
| 坐标拾取 | Coordinate Picking | 点击地球表面获得经纬度和高度 |
| 距离测量 | Distance Measurement | 测量两点或多点路径长度 |
| 面积测量 | Area Measurement | 测量多边形面积 |
| 高度测量 | Height Measurement | 测量垂直高度或相对高度 |
| 绘制工具 | Drawing Tools | 绘制点、线、面、圆、矩形等 |
| 标注 | Annotation / Label | 在地球上显示文字、图标或信息牌 |
| 轨迹回放 | Track Playback | 按时间播放飞机、车辆、船舶等移动轨迹 |

## 具体模块专业名词

这一节适合在写开发提示词、模块设计、类命名、接口命名时使用。

### 相机模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 相机系统 | Camera System | 管理地球场景中的观察位置、朝向、投影和交互 |
| 相机控制器 | Camera Controller | 处理旋转、缩放、平移、飞行、惯性等相机行为 |
| 相机状态 | Camera State | 保存相机 position、direction、up、right、heading、pitch、roll、zoom 等信息 |
| 视图矩阵 | View Matrix | 将世界坐标转换到相机坐标的矩阵 |
| 投影矩阵 | Projection Matrix | 将相机空间转换到裁剪空间的矩阵 |
| 透视投影 | Perspective Projection | 近大远小的三维投影方式 |
| 正交投影 | Orthographic Projection | 无透视缩放的投影方式，常用于特殊地图视角 |
| 视锥体 | View Frustum | 相机可见空间范围，用于裁剪瓦片和对象 |
| 近裁剪面 | Near Plane | 离相机最近的可见裁剪面 |
| 远裁剪面 | Far Plane | 离相机最远的可见裁剪面 |
| 相机位置 | Camera Position | 相机在世界坐标或 ECEF 坐标中的位置 |
| 观察目标 | Look Target / Focus Point | 相机当前看向的地球表面点或对象 |
| 视线方向 | View Direction | 相机朝向方向向量 |
| 上方向 | Up Vector | 相机局部坐标中的向上方向 |
| 右方向 | Right Vector | 相机局部坐标中的右方向 |
| 航向角 | Heading / Yaw | 水平方向旋转角 |
| 俯仰角 | Pitch | 相机上下倾斜角 |
| 翻滚角 | Roll | 相机绕视线方向旋转角 |
| 视场角 | FOV, Field of View | 相机可见角度范围 |
| 相机高度 | Camera Height | 相机距离椭球面或地形表面的高度 |
| 地表距离 | Ground Distance | 相机视点与地表目标之间的距离 |
| 轨道控制 | Orbit Control | 围绕地球或目标点旋转观察 |
| 自由视角 | Free Camera | 不固定观察目标的自由飞行相机 |
| 第一人称相机 | First-person Camera | 类似人眼视角的相机模式 |
| 俯视相机 | Top-down Camera | 从上往下看的地图视角 |
| 倾斜视角 | Oblique View | 带俯仰角观察地表或城市模型 |
| 飞行相机 | Camera Flight / FlyTo | 平滑插值移动到指定位置或对象 |
| 相机动画 | Camera Animation | 相机位置和朝向随时间变化 |
| 相机惯性 | Camera Inertia | 拖拽结束后保留短暂运动惯性 |
| 缩放限制 | Zoom Constraints | 限制最小和最大相机高度或距离 |
| 俯仰限制 | Pitch Constraints | 限制相机最大俯仰角，防止穿地或翻转 |
| 碰撞检测 | Camera Collision Detection | 防止相机穿入地形、地球或三维模型 |
| 地形避让 | Terrain Collision Avoidance | 根据地形高度调整相机高度 |
| 屏幕坐标转射线 | Screen-to-Ray | 从屏幕点击位置生成世界空间射线 |
| 射线投地球 | Ray-Ellipsoid Intersection | 射线与 WGS84 椭球相交，得到经纬度 |
| 射线投地形 | Ray-Terrain Intersection | 射线与真实地形网格相交，得到地形高度点 |

### 场景模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 场景 | Scene | 地球、相机、图层、光照、标注、模型等对象的总容器 |
| 场景管理器 | Scene Manager | 管理场景对象创建、更新、渲染和销毁 |
| 场景图 | Scene Graph | 以树或层级结构组织渲染对象 |
| 节点 | Node | 场景图中的一个对象或分组 |
| 根节点 | Root Node | 场景图最顶层节点 |
| 变换节点 | Transform Node | 带有位置、旋转、缩放的场景节点 |
| 可渲染对象 | Renderable Object | 可以提交给渲染管线绘制的对象 |
| 场景状态 | Scene State | 当前图层、相机、时间、光照、加载状态等全局状态 |
| 更新循环 | Update Loop | 每帧更新相机、动画、瓦片、资源和场景状态 |
| 渲染循环 | Render Loop | 每帧执行绘制命令的循环 |
| 帧状态 | Frame State | 单帧内使用的相机、时间、视锥、可见对象等信息 |
| 时间系统 | Clock / Time System | 管理当前时间、时间倍率、轨迹回放和动态数据 |
| 光照系统 | Lighting System | 管理太阳光、环境光、阴影和昼夜效果 |
| 太阳方向 | Sun Direction | 用于光照和昼夜分界的太阳方向向量 |
| 大气层 | Atmosphere | 地球外部的大气视觉效果 |
| 天空盒 | Skybox | 远处天空或星空背景 |
| 后处理 | Post-processing | 渲染后进行色彩、泛光、抗锯齿等效果处理 |

### 渲染模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 渲染器 | Renderer | 负责将场景对象绘制到屏幕或纹理 |
| 渲染管线 | Rendering Pipeline | 从场景更新到提交 GPU 绘制命令的完整流程 |
| 渲染后端 | Rendering Backend | OpenGL ES、Vulkan、Metal、WebGL、WebGPU 等底层图形 API |
| 渲染上下文 | Rendering Context | 图形 API 的上下文对象 |
| 渲染线程 | Render Thread | 专门执行图形渲染的线程 |
| 主线程 | Main Thread / UI Thread | 处理 UI、生命周期和用户输入的线程 |
| 命令队列 | Render Command Queue | 将渲染命令从逻辑层传递到渲染线程 |
| 渲染目标 | Render Target | 屏幕、离屏纹理或 Framebuffer |
| 帧缓冲 | Framebuffer | GPU 渲染输出的缓冲区 |
| 顶点缓冲 | Vertex Buffer | 存储顶点数据的 GPU 缓冲 |
| 索引缓冲 | Index Buffer | 存储三角形索引的 GPU 缓冲 |
| Uniform Buffer | Uniform Buffer | 向 Shader 传递矩阵、颜色、参数等常量数据 |
| 材质 | Material | 控制对象表面颜色、纹理、透明度、光照响应 |
| Mesh | Mesh | 由顶点、索引和材质组成的网格对象 |
| Draw Command | Draw Command | 一条可提交给 GPU 的绘制命令 |
| Render Pass | Render Pass | 一次渲染阶段，例如地球、地形、模型、透明物体、后处理 |
| 深度测试 | Depth Test | 根据深度判断像素遮挡关系 |
| 混合 | Blending | 处理透明物体、半透明图层和叠加效果 |
| 抗锯齿 | Anti-aliasing | 减少边缘锯齿 |
| MSAA | Multisample Anti-aliasing | 多重采样抗锯齿 |
| FXAA | Fast Approximate Anti-aliasing | 快速近似抗锯齿后处理 |

### 瓦片模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 瓦片系统 | Tile System | 将地图、地形、模型数据拆分成可按需加载的小块 |
| 瓦片坐标 | Tile Coordinates | z/x/y 或 level/x/y 表示的瓦片索引 |
| 瓦片层级 | Tile Level / Zoom Level | 地图瓦片的缩放级别 |
| 根瓦片 | Root Tile | 最粗级别的起始瓦片 |
| 子瓦片 | Child Tile | 从父瓦片细分出的更高精度瓦片 |
| 父瓦片 | Parent Tile | 当前瓦片上一层级的瓦片 |
| 瓦片树 | Tile Tree | 由父子瓦片组成的层级结构 |
| 瓦片选择 | Tile Selection | 根据视角、距离和误差决定当前要显示哪些瓦片 |
| 瓦片细分 | Tile Refinement | 使用更高层级瓦片替换低层级瓦片 |
| 替换细化 | Replacement Refinement | 子瓦片加载完成后替换父瓦片 |
| 叠加细化 | Additive Refinement | 子瓦片和父瓦片同时显示，常用于 3D Tiles |
| 瓦片加载队列 | Tile Load Queue | 等待下载或解析的瓦片队列 |
| 瓦片优先级 | Tile Priority | 按屏幕重要性、距离、误差决定加载顺序 |
| 瓦片缓存 | Tile Cache | 缓存已加载瓦片，避免重复下载和解析 |
| 瓦片状态 | Tile State | unloaded、loading、loaded、failed、visible 等状态 |
| 可见瓦片集 | Visible Tile Set | 当前帧需要渲染的瓦片集合 |
| 屏幕空间误差 | Screen Space Error | 衡量瓦片在屏幕上的几何误差，用于 LOD |
| 几何误差 | Geometric Error | 瓦片数据自身的空间误差 |
| 瓦片包围体 | Tile Bounding Volume | 用于裁剪和距离计算的包围盒、包围球或区域 |
| 瓦片请求调度 | Tile Request Scheduling | 控制网络并发、优先级、取消和重试 |
| 瓦片取消加载 | Tile Request Cancellation | 相机移动后取消不再需要的瓦片请求 |

### 图层模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 图层管理器 | Layer Manager | 管理图层添加、删除、排序、显示隐藏和透明度 |
| 图层 | Layer | 地图中的一类可显示数据 |
| 影像图层 | Imagery Layer | 卫星图、街道图、夜景图等图片瓦片图层 |
| 地形图层 | Terrain Layer | DEM 高程或地形网格图层 |
| 矢量图层 | Vector Layer | 点线面等矢量要素图层 |
| 模型图层 | Model Layer | glTF、GLB、3D Tiles、BIM 等三维模型图层 |
| 标注图层 | Annotation Layer | 文字、图标、气泡、信息牌等标注 |
| 热力图层 | Heatmap Layer | 根据点密度或数值渲染热力图 |
| 栅格图层 | Raster Layer | 基于像素图片或栅格数据的图层 |
| 动态图层 | Dynamic Layer | 随时间变化的数据图层 |
| 图层顺序 | Layer Order / Z-order | 控制图层叠加显示顺序 |
| 图层透明度 | Layer Opacity | 控制图层透明程度 |
| 图层可见性 | Layer Visibility | 控制图层显示或隐藏 |
| 图层样式 | Layer Style | 控制颜色、线宽、填充、图标、标签等视觉规则 |
| 图层过滤 | Layer Filter | 根据属性、时间或空间范围过滤要素 |
| 数据源 | Data Source | 图层背后的数据来源，例如 URL、数据库、文件 |
| 数据提供器 | Data Provider | 负责读取、请求、解析某类数据的对象 |

### 拾取与交互模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 拾取系统 | Picking System | 根据鼠标或触摸位置查询地球坐标或场景对象 |
| 对象拾取 | Object Picking | 点击场景对象并返回对象 ID、属性和位置信息 |
| 坐标拾取 | Coordinate Picking | 点击地球表面返回经纬度和高度 |
| 深度拾取 | Depth Picking | 使用深度缓冲反算点击位置的三维坐标 |
| 颜色拾取 | Color Picking | 用隐藏颜色编码渲染对象 ID，再读取像素识别对象 |
| 射线拾取 | Ray Picking | 从屏幕点生成射线，与地球、地形、模型求交 |
| 命中测试 | Hit Testing | 判断点击或射线是否命中对象 |
| 触摸手势 | Touch Gesture | 单指拖拽、双指缩放、双指旋转等移动端手势 |
| 手势识别器 | Gesture Recognizer | 识别用户触摸行为并转换为地图操作 |
| 手势冲突 | Gesture Conflict | 地图手势与外层 Flutter/Android UI 手势发生竞争 |
| 手势仲裁 | Gesture Arbitration | 决定当前触摸事件由地图还是外层 UI 处理 |
| 事件分发 | Event Dispatching | 将点击、拖拽、缩放、相机变化等事件发送给业务层 |
| 输入管理器 | Input Manager | 统一处理鼠标、触摸、键盘、手柄等输入 |

### 标注与覆盖物模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 覆盖物 | Overlay | 叠加在地图上的点、线、面、图标、气泡或 UI |
| 地图覆盖物 | Map Overlay | 与地理坐标绑定的覆盖物 |
| 屏幕覆盖物 | Screen Overlay | 固定在屏幕坐标上的 UI 覆盖物 |
| 点标注 | Point Marker | 经纬度位置上的点或图标 |
| 文字标签 | Text Label | 地图上的文字标注 |
| 信息窗 | Info Window / Callout | 点击对象后显示的信息弹窗 |
| 广告牌 | Billboard | 始终面向相机的二维图片标注 |
| 折线 | Polyline | 由多个经纬度点组成的线 |
| 多边形 | Polygon | 由经纬度点围成的面 |
| 贴地线 | Ground Polyline | 沿地形表面绘制的线 |
| 贴地面 | Ground Polygon | 贴合地形表面的多边形 |
| 高度模式 | Height Reference | 绝对高度、贴地、相对地形高度等模式 |
| 遮挡策略 | Occlusion Policy | 标注被地球、地形或模型遮挡时如何显示 |
| 聚合 | Clustering | 远距离下将多个标注合并为一个聚合点 |
| 避让 | Label Collision Avoidance | 防止多个标签互相重叠 |

### 动画与时间模块

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 动画系统 | Animation System | 管理相机、对象、轨迹、时间序列的动画 |
| 时间轴 | Timeline | 展示和控制时间变化的 UI 或逻辑组件 |
| 时钟 | Clock | 当前模拟时间和播放速度 |
| 时间倍率 | Time Multiplier | 控制时间加速或减速 |
| 轨迹 | Trajectory / Track | 移动对象按时间排列的位置序列 |
| 轨迹插值 | Track Interpolation | 在离散轨迹点之间计算连续位置 |
| 关键帧 | Keyframe | 动画中定义状态的时间点 |
| 插值 | Interpolation | 在两个状态之间平滑过渡 |
| 缓动函数 | Easing Function | 控制动画速度变化曲线 |
| 轨迹回放 | Track Playback | 按时间播放车辆、飞机、船舶等移动对象 |
| 动态实体 | Dynamic Entity | 位置、姿态或样式随时间变化的对象 |
| 姿态 | Orientation / Attitude | 对象的朝向，通常由 heading、pitch、roll 或四元数表示 |

## 性能优化术语

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| 按需加载 | On-demand Loading | 只加载当前视角需要的数据 |
| 预加载 | Preloading | 预测相机移动方向提前加载数据 |
| 缓存淘汰 | Cache Eviction | 超出缓存容量时移除旧资源 |
| 屏幕空间误差 | SSE, Screen Space Error | 用屏幕误差决定瓦片 LOD 精度 |
| 视锥剔除 | Frustum Culling | 不渲染相机外对象 |
| 批处理 | Batching | 合并多个对象减少 draw call |
| 实例化渲染 | Instanced Rendering | 大量相同对象共享几何体高效渲染 |
| 纹理压缩 | Texture Compression | 减少显存和网络传输 |
| 几何压缩 | Geometry Compression | 减少模型体积，例如 Draco 压缩 |
| Web Worker | Web Worker | 在后台线程解析数据，避免阻塞 UI |
| GPU Instancing | GPU Instancing | 使用 GPU 一次绘制多个实例 |
| Draw Call | Draw Call | CPU 向 GPU 发起的一次绘制命令 |

## 常见开发技术选型

| 方向 | 可用技术 | 适合场景 |
| --- | --- | --- |
| 快速做三维地球 | CesiumJS | 最成熟，适合 GIS、三维城市、地形、3D Tiles |
| 自研渲染层 | Three.js | 自由度高，适合做定制视觉和轻量地球 |
| 高性能新架构 | WebGPU | 适合未来高性能浏览器图形，但生态仍在发展 |
| 二维地图叠加 | Mapbox GL / MapLibre GL | 适合矢量地图、平面地图、部分 globe 模式 |
| 空间分析 | Turf.js | 适合距离、面积、缓冲区、相交等 GIS 分析 |
| 坐标转换 | proj4js | 适合不同坐标系统之间转换 |
| 后端地图服务 | GeoServer | 发布 WMS、WMTS、WFS 服务 |
| 空间数据库 | PostGIS | 存储和查询地理空间数据 |
| 三维模型格式 | glTF / GLB | Web 端三维模型常用格式 |

## 移动端 SDK 架构定位

如果目标是做真正可长期演进的地图引擎 SDK，不建议把 Flutter 作为内核宿主。更合理的分层是：

```text
C++ / Cesium Native Core
  -> Android Native SDK Host
    -> Flutter SDK Adapter
      -> Flutter Overlay / UI Integration
```

核心原则：

1. C++ / Cesium Native 是地图引擎内核，负责真正的地理空间能力和渲染能力。
2. Android 原生 SDK 是一等公民，负责页面宿主、渲染宿主、手势、生命周期和平台接口。
3. Flutter SDK 只是上层适配层，用来把原生 SDK 暴露给 Flutter 应用，并集成 Overlay/UI。
4. Flutter 不应该成为地图内核宿主，也不应该承载瓦片调度、渲染循环、场景管理、资源缓存等核心逻辑。
5. SDK 的优先级应该是 Native SDK first，Flutter wrapper second。

### 分层职责

| 层级 | 英文名 | 职责 |
| --- | --- | --- |
| C++ 地图内核 | C++ Globe Engine Core / Cesium Native Core | 坐标系统、瓦片调度、地形、3D Tiles、资源缓存、场景状态、渲染数据准备 |
| 原生渲染桥接 | Native Rendering Bridge | 将 C++ 内核输出接入 Android 图形环境，例如 OpenGL ES、Vulkan、Surface、Texture |
| Android 原生 SDK | Android Native SDK | 提供 MapView、生命周期绑定、手势识别、相机控制、权限、线程调度、平台网络和存储接口 |
| 平台接口层 | Platform Abstraction Layer | 抽象文件系统、网络请求、线程、日志、设备信息、GPU 能力 |
| Flutter 适配层 | Flutter SDK Adapter / Flutter Plugin | 通过 PlatformView、Texture、MethodChannel、EventChannel 或 FFI 调用原生 SDK |
| Flutter UI 层 | Flutter Overlay / UI Layer | 做按钮、面板、弹窗、业务标注 UI、状态展示，不承载地图内核 |

### 相关专业名词

| 中文 | 英文 | 给 AI 的描述方式 |
| --- | --- | --- |
| 原生优先架构 | Native-first Architecture | SDK 的核心能力首先在 Android/iOS 原生层成立，Flutter 只是适配入口 |
| Flutter 适配层 | Flutter Adapter Layer | 对原生 SDK 的封装，不拥有核心地图状态 |
| 原生 SDK 壳 | Native SDK Shell / Native Host | 承载地图页面、渲染视图、生命周期、手势和平台接口 |
| 渲染宿主 | Rendering Host | 管理地图渲染 Surface、渲染线程和图形上下文 |
| 页面宿主 | View Host / Page Host | 管理 MapView 或地图页面的创建、销毁、暂停、恢复 |
| 平台视图 | PlatformView | Flutter 嵌入 Android/iOS 原生 View 的机制 |
| 纹理桥接 | Texture Bridge | 原生层渲染到纹理，再交给 Flutter 合成显示 |
| 方法通道 | MethodChannel | Flutter 调用原生同步或异步方法的通道 |
| 事件通道 | EventChannel | 原生层向 Flutter 持续推送事件的通道 |
| FFI | Foreign Function Interface | Flutter/Dart 直接调用 C/C++ 动态库的机制 |
| JNI | Java Native Interface | Android Java/Kotlin 与 C++ 通信的接口 |
| 生命周期绑定 | Lifecycle Binding | 将地图引擎与 Activity、Fragment、View 生命周期关联 |
| 手势仲裁 | Gesture Arbitration | 处理 Flutter 手势与原生地图手势之间的冲突 |
| Overlay 集成 | Overlay Integration | Flutter UI 覆盖在原生地图视图之上 |

### 推荐模块边界

| 模块 | 应该放在哪里 | 不应该放在哪里 |
| --- | --- | --- |
| 瓦片调度 | C++ Core | Flutter |
| LOD / SSE 计算 | C++ Core | Flutter |
| 坐标转换 | C++ Core，可在 Native SDK 暴露轻量 API | Flutter 业务层重复实现 |
| 地形和 3D Tiles 解析 | C++ Core / Cesium Native | Flutter |
| 资源缓存 | C++ Core + Native 平台存储适配 | Flutter |
| 渲染循环 | Native Rendering Host | Flutter Widget 树 |
| 手势识别 | Android Native SDK，可向 Flutter 暴露事件 | Flutter 单独接管全部地图手势 |
| 生命周期 | Android Native SDK | Flutter 插件内部随意管理 |
| 业务按钮和面板 | Flutter Overlay | C++ Core |
| 弹窗、图层面板、搜索框 | Flutter UI | C++ Core |

### 可以直接复制给 AI 的架构边界提示词

```text
我要开发一个移动端 3D 地球地图 SDK。架构必须是 Native-first，而不是 Flutter-first。

核心分层如下：
1. C++ / Cesium Native Core 是真正的地图引擎内核，负责 WGS84 坐标、瓦片调度、LOD、地形、3D Tiles、资源缓存、场景状态和渲染数据准备。
2. Android Native SDK 是 SDK 的一等公民，负责 MapView、渲染宿主、Surface/Texture、手势、相机控制、生命周期、线程、网络、存储和平台接口。
3. Flutter SDK 只是适配层，用于把 Android 原生 SDK 暴露给 Flutter 应用，并负责 Flutter Overlay/UI 集成。

请不要把 Flutter 设计成地图内核宿主。Flutter 不应该承载瓦片调度、渲染循环、场景管理、资源缓存、地形解析或 3D Tiles 解析。Flutter 只负责调用原生能力、展示业务 UI、接收事件、叠加 Overlay。

请基于这个边界设计 SDK 架构、模块划分、Android API、Flutter Plugin API、JNI/FFI/PlatformView/Texture 的通信方式，以及生命周期和手势冲突处理方案。
```

## 可以直接复制给 AI 的开发提示词

### 基础版

```text
我想开发一个 Web 端 3D 地球引擎，类似 CesiumJS 或 Google Earth。请帮我设计技术架构，核心能力包括 WGS84 经纬度坐标、3D Globe 渲染、XYZ/WMTS 影像瓦片加载、DEM 地形高程、LOD 层级细节、相机 Orbit/Zoom/Pan/FlyTo 控制、鼠标拾取经纬度、图层管理、标注和测量工具。请优先考虑性能优化，例如四叉树瓦片调度、视锥裁剪、地平线裁剪、缓存管理和 Web Worker 异步解析。
```

### Three.js 自研版

```text
我想基于 Three.js 自研一个轻量级 3D 地球引擎。请设计模块结构和实现路线，包括 Ellipsoid/WGS84 坐标模型、经纬度到 ECEF/Cartesian 坐标转换、球体网格或瓦片网格生成、影像纹理贴图、XYZ 瓦片加载、LOD 四叉树、相机控制、鼠标射线拾取、地球表面坐标反算、图层系统和资源缓存。请注意大尺度场景的浮点精度问题，并说明是否需要 origin rebasing 或 relative-to-center rendering。
```

### CesiumJS 应用版

```text
我想基于 CesiumJS 开发一个数字地球应用，不从零实现渲染引擎。请帮我设计应用架构，包括 Viewer 初始化、ImageryLayer 影像图层、TerrainProvider 地形、3D Tiles 城市模型、Entity/Primitive 标注体系、Camera FlyTo 定位、ScreenSpaceEventHandler 鼠标交互、GeoJSON/KML/CZML 数据加载、距离面积测量、轨迹回放和图层面板管理。
```

### 数据平台版

```text
我想开发一个地理空间数据可视化平台，前端是 3D 地球引擎，后端负责管理地图服务和空间数据。请设计前后端架构：前端支持 3D Globe、影像瓦片、地形、矢量图层、3D Tiles、点云和标注；后端使用 PostGIS 存储地理数据，GeoServer 发布 WMS/WMTS/WFS 服务，对大规模数据做切片、缓存和权限控制。请给出数据库表设计、API 设计和瓦片服务方案。
```

## 需求描述模板

```text
项目目标：
我要开发一个【Web/桌面/移动端】地球引擎，用于【行业场景，例如城市管理、遥感影像、军事态势、物流轨迹、气象可视化】。

核心功能：
1. 3D 地球渲染，支持 WGS84 经纬度。
2. 加载 XYZ/WMTS 影像瓦片。
3. 加载 DEM 地形高程，并生成 3D 地形。
4. 支持 GeoJSON/KML/CZML 等地理数据。
5. 支持点、线、面、标签、轨迹和模型叠加。
6. 支持相机旋转、缩放、平移、FlyTo。
7. 支持鼠标拾取经纬度和对象信息。
8. 支持距离、面积、高度测量。
9. 支持 LOD、瓦片缓存、视锥裁剪等性能优化。

技术约束：
前端使用【CesiumJS/Three.js/WebGL/WebGPU/React/Vue】。
后端使用【Node.js/Java/Python/Go】。
数据来源包括【天地图/高德/Mapbox/自建 GeoServer/本地 GeoJSON/3D Tiles】。

请输出：
1. 技术架构。
2. 模块拆分。
3. 数据格式建议。
4. 核心算法说明。
5. 开发里程碑。
6. 最小可运行版本 MVP。
```

## MVP 建议

如果你还在早期，建议先不要一次做完整引擎。可以先做这个最小版本：

1. 显示一个可旋转缩放的 3D 地球。
2. 支持 WGS84 经纬度到三维坐标转换。
3. 支持加载一层 XYZ 影像瓦片。
4. 支持点击地球获取经纬度。
5. 支持添加点标注和线轨迹。
6. 支持 FlyTo 到指定城市。
7. 初步实现瓦片缓存和 LOD。

对应提示词：

```text
请帮我实现一个 3D 地球引擎 MVP。技术栈使用 Three.js。功能包括：创建 WGS84 椭球体或近似球体地球、加载 XYZ 影像瓦片作为纹理、实现 Orbit 相机控制、经纬度转三维坐标、点击地球反算经纬度、添加点标注、绘制经纬度轨迹线、FlyTo 到指定经纬度。代码要模块化，后续能扩展地形、图层系统和 LOD 瓦片调度。
```

## 和 AI 沟通时不要只说

```text
帮我做一个地球。
```

更好的说法是：

```text
帮我设计一个 3D Globe/GIS 可视化引擎，核心是 WGS84 坐标、影像瓦片、地形高程、图层系统、相机交互和大规模地理数据的 LOD 流式渲染。
```

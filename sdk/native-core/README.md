# native-core

这里存放正式 C++ / Cesium Native 内核源码。

当前已纳入这里的内容：

- `cesium_bridge.cpp`
- 渲染资源准备与批处理代码
- 相关头文件与 `CMakeLists.txt`

当前 `android/app` 仍然通过 `externalNativeBuild.cmake.path` 引用这里的 CMake 工程进行构建。

后续目标：

- 让 native core 成为 Android SDK、未来 iOS SDK、以及其他平台适配层共同依赖的核心实现

# flutter-demo

这个目录是当前 Flutter 示例工程。

它的职责已经收敛为：

- 演示如何消费 `sdk/flutter-plugin`
- 提供 Flutter Android 宿主壳工程
- 承担联调与 APK 构建验证

它不再承载 Flutter SDK 实现本体。

当前真实结构：

- `lib/main.dart`：示例页面与 overlay 控件
- `android/app`：示例 Android 宿主
- `../../sdk/flutter-plugin`：Flutter 侧 SDK 适配层
- `../../sdk/android-sdk`：Android 原生 SDK
- `../../sdk/native-core`：C++ / Cesium Native 核心

原生 Android 直接消费 `sdk/android-sdk` 的验证页已经单独拆到：

- `../android-native-demo`

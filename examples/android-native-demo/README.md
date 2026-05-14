# android-native-demo

这个目录是独立的原生 Android 示例工程。

它只负责验证两件事：

- `sdk/android-sdk` 能否被原生 Android app 直接消费
- `CesiumMapView` 和 `CesiumMapFragment` 两条宿主路径是否都能独立工作

## 构建

```sh
cd examples/android-native-demo
./gradlew :app:assembleDebug
```

## 安装后启动

```sh
adb shell am start -n \
  com.example.cesiumpoc.native_demo/.MainActivity

adb shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkMapActivity

adb shell am start -n \
  com.example.cesiumpoc.native_demo/.NativeSdkFragmentActivity
```

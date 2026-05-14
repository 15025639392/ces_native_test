# Session Log

Append new entries at the top. This file is for continuity across project-based AI sessions.

## 2026-05-14 15:12 Asia/Shanghai - Reduce Motion-Time Tile Pressure With Temporary Coarser Detail

- Changed:
  - Extended `CesiumPerformanceOptions` with `movementMaximumScreenSpaceError`.
  - Made the Android render surface temporarily use a coarser `maximumScreenSpaceError` while the camera is moving, orbiting, or flinging.
  - Restored the base `maximumScreenSpaceError` automatically once the camera reaches idle again.
  - Kept the implementation inside the existing Android-side performance pipeline by reusing the current native `setMaximumScreenSpaceError` bridge.
- Verified:
  - Not yet verified in this entry. Build and device validation should confirm that movement-time tile count and cache pressure drop without harming settled image quality.
- Notes / risks:
  - This targets heat and GPU pressure during active interaction rather than static idle.
  - The default movement-time override is intentionally conservative: it only makes detail coarser while moving, then returns to the settled quality target on idle.

## 2026-05-14 14:58 Asia/Shanghai - Reduce Heat By Pausing Hidden Rendering And Throttling Stable Frames

- Changed:
  - Added Android host lifecycle gating so `CesiumMapView.onResume()` re-enables rendering and `onPause()` disables it.
  - Added internal render-surface visibility gating so the EGL loop no longer keeps drawing when the host window is detached, hidden, or paused.
  - Changed the EGL render thread from a fixed `60fps` target to a dynamic cadence driven by runtime state.
  - Added native `recommendedFrameIntervalNanos` so the render loop now stays fast while the camera is moving or tiles are still settling, drops to about `30fps` while selection/load work is still active, and falls back to about `5fps` once the scene is stable and idle.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
- Not verified:
  - Long-session on-device temperature, battery drain, and background frame suppression were not measured yet after this change.
  - The native demo foreground behavior on the current device is still noisy during adb-based validation, so runtime heat numbers still need a cleaner device pass.
- Notes / risks:
  - This directly addresses the strongest heat suspects: unconditional steady-state rendering and continued drawing while not meaningfully visible.
  - It does not yet solve the separate high-detail pressure issue by itself; scenes with very high tile counts can still be expensive while actively loading or moving.

## 2026-05-14 14:38 Asia/Shanghai - Add Native Demo MSE Quality Presets For On-Device Comparison

- Changed:
  - Added visible `MSE 4 / 8 / 12` quality preset buttons to the direct `CesiumMapView` native demo page.
  - Added the same `MSE 4 / 8 / 12` quality preset buttons to the `CesiumMapFragment` native demo page.
  - Added a top-panel `detail  mse ...` status line so current detail pressure is visible during profiling runs.
  - Wired the demo controls to the new public `performanceOptions` SDK surface instead of touching native internals directly.
- Verified:
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `./gradlew assemble` passed from `sdk/android-sdk`.
- Not verified:
  - A stable foreground screenshot of the new control row was not captured yet because the device returned to the launcher during screenshot collection.
  - The expected on-device metric deltas between `MSE 4 / 8 / 12` were not measured yet.
- Notes / risks:
  - This turns the new performance API into a practical profiling surface, but we still need the next measurement pass to establish the recommended default band for V1.

## 2026-05-14 14:20 Asia/Shanghai - Add First Formal SDK Performance Options Surface

- Changed:
  - Added public `CesiumPerformanceOptions` to the Android SDK and Flutter plugin.
  - Added `performanceOptions` to the Android `CesiumMapController` contract and the Flutter controller bridge.
  - Wired `maximumScreenSpaceError` through Flutter -> Android host -> JNI -> native core.
  - Applied performance option changes on the render thread by requesting a controlled tileset rebuild instead of replacing the tileset directly from the UI thread.
- Verified:
  - Not yet verified in this entry. Build and runtime validation should confirm that `maximumScreenSpaceError` changes still render correctly and that higher values reduce tile/detail pressure on device.
- Notes / risks:
  - This is intentionally the first narrow SDK performance knob rather than a broad config dump. It gives the runtime a formal detail-pressure control without overcommitting the V1 API shape.
  - Changing `maximumScreenSpaceError` now rebuilds the tileset selection state, so callers should expect a short reload period after updating the option.

## 2026-05-14 14:02 Asia/Shanghai - Remove Render Loop Self-Throttling And Rebaseline Device FPS

- Changed:
  - Replaced the old unconditional `sleep(16)` pacing in `CesiumMapRenderSurface`'s EGL render thread with budget-aware frame pacing against a `16.67ms` target.
  - Let the render thread sleep only for remaining frame budget and yield when already over budget, so draw + swap time no longer gets a second fixed delay stacked on top.
  - Locked the Android native demo activities to portrait to keep device profiling free from relaunch and rotation noise during repeated `adb` measurements.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - Reinstalled and relaunched `com.example.cesiumpoc.native_demo/.NativeSdkMapActivity` on adb device `7e045e39`.
  - Fresh on-device overlay after the fix showed about `58.7 FPS`, `4.6ms draw`, `847/847 tiles`, `333.7MB cache`.
  - Fresh `dumpsys gfxinfo` after reset and stable idle showed `50th percentile: 5ms`, `90th percentile: 17ms`, `95th percentile: 22ms`, `99th percentile: 38ms`, compared with the earlier `50th percentile: 40ms` and `90th percentile: 48ms`.
- Not verified:
  - Long-session memory stability and interaction-period frame pacing under heavy pan / zoom / rotate input were not profiled yet.
- Notes / risks:
  - This confirms the earlier low-FPS feel was primarily caused by the render loop self-throttling rather than Android UI-thread draw cost.
  - The next likely bottleneck is tile/detail pressure rather than basic frame pacing: the current scene still reports `847` visible / cached tiles and about `333.7MB` cache on the validation screen.

## 2026-05-14 04:35 Asia/Shanghai - Add Native Two-Finger Rotate And Tilt Gestures

- Changed:
  - Wired `CesiumGestureOptions.rotateEnabled` to a real two-finger rotate gesture on Android.
  - Wired `CesiumGestureOptions.tiltEnabled` to a real two-finger tilt gesture on Android.
  - Kept the new gestures inside the existing native camera/update/idle pipeline so they reuse throttled camera callbacks and idle signaling.
  - Prevented single-finger pan logic from competing with multi-touch rotate and tilt handling.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
- Not verified:
  - On-device gesture tuning for rotate and tilt was not validated interactively yet.
- Notes / risks:
  - The first cut uses a conservative gesture model: rotation comes from two-finger angle change, and tilt comes from two-finger vertical focus movement. It is functional, but likely still wants tuning after hands-on testing.

## 2026-05-14 04:20 Asia/Shanghai - Make bearing/pitch Affect The Native Cesium Camera

- Changed:
  - Extended the Kotlin-to-JNI native camera bridge so `bearing` and `pitch` now flow into C++.
  - Updated the Cesium native view-state and draw-camera basis calculation so `bearing` and `pitch` now change the actual camera direction.
  - Added Flutter demo sliders for `bearing` and `pitch`.
  - Added native Android demo controls for turning and pitching the camera in both the `View` and `Fragment` validation pages.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - On-device visual validation of the new `bearing / pitch` camera behavior was not rerun yet.
- Notes / risks:
  - `bearing / pitch` now affect the native camera basis, but rotate and tilt gestures are still not implemented as first-class host gestures.

## 2026-05-14 04:00 Asia/Shanghai - Improve Camera Event Semantics And Gesture Precision

- Changed:
  - Extended `CesiumCameraState` on Android and Flutter with `bearing` and `pitch` as forward-compatible camera contract fields.
  - Added `onCameraMoveStarted` and `onCameraIdle` to the Android SDK listener surface and bridged the same events through the Flutter plugin.
  - Changed `onCameraChanged` semantics to a throttled in-progress callback instead of an every-step raw emission.
  - Improved native gesture feel by adding zoom-focus anchoring and slightly reduced pan sensitivity.
  - Kept native runtime behavior conservative: `bearing` and `pitch` are stored and bridged, while the current Cesium native camera path still mainly consumes lon/lat/zoom.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
- Not verified:
  - On-device gesture tuning and callback cadence were not validated interactively yet.
- Notes / risks:
  - This change intentionally upgrades the contract shape before full `bearing / pitch` rendering behavior exists. That keeps future API evolution cleaner, but those two fields should still be described as forward-compatible rather than fully effective today.

## 2026-05-14 03:35 Asia/Shanghai - Land First Native Gesture Engine Cut In android-sdk

- Changed:
  - Added Android host gesture handling directly inside `CesiumMapRenderSurface`.
  - Wired single-finger pan into camera longitude and latitude updates.
  - Wired pinch zoom and double-tap zoom into camera zoom updates.
  - Added optional fling inertia when `CesiumGestureOptions.inertiaEnabled` is true.
  - Made manual gesture interaction stop `autoOrbit` so host input wins immediately.
  - Updated Android SDK docs to reflect that pan and zoom are now real native behavior, while rotate and tilt are still reserved.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
- Not verified:
  - On-device gesture feel and camera motion tuning were not validated yet.
- Notes / risks:
  - This is the first formal native gesture cut. Pan and zoom are implemented with a conservative geographic conversion model, so feel tuning is still expected before treating it as final SDK behavior.

## 2026-05-14 03:20 Asia/Shanghai - Make sdk/android-sdk Buildable As A Standalone Gradle Project

- Changed:
  - Added `sdk/android-sdk/settings.gradle.kts` with plugin and dependency repositories.
  - Added `sdk/android-sdk/gradle.properties` for AndroidX and Gradle JVM settings.
  - Added a local Gradle wrapper under `sdk/android-sdk` so the Android SDK can build from its own directory.
  - Updated `sdk/android-sdk/README.md` with direct standalone build commands.
- Verified:
  - `./gradlew assemble` passed from `sdk/android-sdk`.
- Not verified:
  - Maven publication or external consumer integration was not added yet.
- Notes / risks:
  - The Android SDK is now independently buildable as a Gradle library project, but publish-ready artifact metadata and dependency packaging still need a deliberate next step.

## 2026-05-14 03:05 Asia/Shanghai - Decouple android-sdk From Flutter Build Inputs

- Changed:
  - Moved the Flutter-only `PlatformView` bridge classes out of `sdk/android-sdk` and into `sdk/flutter-plugin/android`.
  - Updated the Flutter plugin entry to register its own internal factory from the plugin module.
  - Removed `sdk/android-sdk`'s compile-only `flutter.jar` dependency and the read of `examples/flutter-demo/android/local.properties`.
  - Updated SDK structure docs so `android-sdk` now reads as a pure Android host SDK again.
- Verified:
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - On-device launch after the module split was not rerun.
- Notes / risks:
  - This removes the biggest structural coupling, but publishability still depends on how we package `native-core` and the external Cesium Native binary dependency.

## 2026-05-14 02:45 Asia/Shanghai - Split Native Validation Into examples/android-native-demo

- Changed:
  - Added a standalone Android app under `examples/android-native-demo`.
  - Moved native validation responsibility out of the Flutter demo shell and into that new module.
  - Added a native launcher page plus direct `CesiumMapView` and `CesiumMapFragment` demo pages in the new module.
  - Removed the old native validation activities from `examples/flutter-demo/android/app`.
  - Updated repo docs and handoff notes so the native and Flutter example roles are now clearly separated.
- Verified:
  - `./gradlew :app:assembleDebug` passed from `examples/android-native-demo`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - The standalone native demo APK was not installed and launched on-device after this code change.
- Notes / risks:
  - `sdk/android-sdk` still reads Flutter SDK location from `examples/flutter-demo/android/local.properties` for its compile-only Flutter embedding dependency. That is acceptable in the current workspace, but it should be decoupled before publishing the Android SDK as a standalone external artifact.

## 2026-05-14 02:30 Asia/Shanghai - Add Android SDK Quickstart And Public API KDoc

- Changed:
  - Added `docs/SDK_ANDROID_QUICKSTART.md` with direct `CesiumMapView` and `CesiumMapFragment` integration examples.
  - Added KDoc comments to the public Android SDK surface so the current V1 contracts read more like a real consumable SDK API.
  - Linked the Android quickstart doc from the main repo index and handoff entrypoints.
- Verified:
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - The new quickstart snippets were not exercised as a standalone fresh consumer project yet.
- Notes / risks:
  - The quickstart intentionally reflects the current V1 implementation truth. It should be updated again once a separate native demo module or published Android artifact shape is introduced.

## 2026-05-14 02:20 Asia/Shanghai - Add A Minimal Native Fragment SDK Example Page

- Changed:
  - Added `NativeSdkFragmentActivity` under the demo Android shell as a direct consumer of `CesiumMapFragment`.
  - Built the fragment example page as a native `FragmentActivity` with programmatic controls for camera move, zoom, orbit, memory clear, and stats display.
  - Registered that activity in the demo app manifest and documented its adb launch command alongside the direct `CesiumMapView` example page.
- Verified:
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - The native fragment SDK example page was not launched on-device after this code change.
- Notes / risks:
  - This page is also a verification surface rather than a polished UX. Its purpose is to prove the `CesiumMapFragment` host path is consumable directly by an Android app.

## 2026-05-14 02:10 Asia/Shanghai - Add A Minimal Native Android SDK Example Page

- Changed:
  - Added `NativeSdkMapActivity` under the demo Android shell as a direct consumer of `CesiumMapView`.
  - Built the example page entirely on the Android side with programmatic controls for camera move, zoom, orbit, memory clear, and stats display.
  - Registered that activity in the demo app manifest so it can be launched directly with adb for SDK validation outside Flutter UI.
  - Updated READMEs and handoff docs with the adb launch command for the native SDK example page.
- Verified:
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - The native SDK example page was not launched on-device after this code change.
- Notes / risks:
  - This page is intentionally a verification surface, not a polished product UI. Its job is to prove the Android SDK can be consumed directly without going through Flutter.

## 2026-05-14 02:00 Asia/Shanghai - Make Interaction Config Readable And Let interactionEnabled Affect Native Touch

- Changed:
  - Added Flutter getters for `getInteractionEnabled` and `getGestureOptions`.
  - Added native channel support for reading current interaction state and gesture options.
  - Updated the Android render surface so `interactionEnabled` now affects native touch dispatch instead of being only a stored flag.
  - Added `CesiumGestureOptions.fromMap` on Flutter so interaction config can round-trip cleanly across the bridge.
- Verified:
  - `dart format` passed for the touched Dart files.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - `interactionEnabled` now has a real native host effect, but `gestureOptions` still represents a conservative configuration surface rather than a complete gesture-engine implementation.

## 2026-05-14 01:50 Asia/Shanghai - Align Flutter And Android V1 Interaction Surface

- Changed:
  - Added public `CesiumGestureOptions` on Android and Flutter.
  - Extended Android `CesiumMapController` with `interactionEnabled` and `gestureOptions` properties.
  - Extended Flutter `CesiumMapController` with `setInteractionEnabled` and `setGestureOptions`.
  - Updated the Flutter `CesiumMapWidget` and Android `PlatformView` bridge so interaction config now flows through creation params and method-channel calls.
  - Kept the implementation intentionally conservative: current behavior stores and forwards interaction config, but does not yet claim a full native gesture system behind it.
- Verified:
  - `dart format` passed for the touched Dart files.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - Android uses Kotlin property syntax for `interactionEnabled` and `gestureOptions`, while Flutter uses setter-style async methods; this difference is intentional because Kotlin/JVM property accessors would otherwise collide with same-name methods at the bytecode layer.

## 2026-05-14 01:35 Asia/Shanghai - Reintroduce A Small Public CesiumMapController Surface

- Changed:
  - Added a new public `CesiumMapController` contract for Android hosts.
  - Updated `CesiumMapView` to implement that controller directly.
  - Added `CesiumMapFragmentController` and exposed `CesiumMapFragment.controller` so fragment hosts can use the same control semantics as view hosts.
  - Kept the controller intentionally small for V1: camera state, set camera, clear memory, stats, and listener attachment.
- Verified:
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - The Android controller contract now matches the current code reality better than the older draft, but interaction toggles and gesture options are still deliberately absent from the implementation.

## 2026-05-14 01:25 Asia/Shanghai - Add A Minimal CesiumMapFragment Entry For Android Hosts

- Changed:
  - Added public `CesiumMapFragment` as the second Android host entry next to `CesiumMapView`.
  - Wired the fragment to create and own `CesiumMapView`, forward fragment lifecycle calls, and support pending initial camera plus listener attachment.
  - Added `androidx.fragment:fragment-ktx` to both the Android SDK module and the Flutter plugin Android module so shared source compilation remains consistent.
- Verified:
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - `CesiumMapFragment` is intentionally minimal for now; it establishes the official fragment host shape, but it does not yet add saved-state restoration, richer lifecycle policy, or custom XML attributes.

## 2026-05-14 01:15 Asia/Shanghai - Add Listener And Lifecycle Surface To CesiumMapView

- Changed:
  - Added public `CesiumMapListener` and `CesiumMapError` to start shaping a real Android SDK callback surface.
  - Updated `CesiumMapView` so it now owns listener dispatch for map ready, camera changes, render stats, and errors.
  - Added basic Android host lifecycle-style methods on `CesiumMapView`: `onCreate`, `onStart`, `onResume`, `onPause`, `onStop`, `onDestroy`, and `onLowMemory`.
  - Kept the existing Flutter adapter path working by routing its internal callbacks through `CesiumMapView` instead of directly through the render surface.
- Verified:
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - The new lifecycle methods are currently thin host-facing entry points; they establish the SDK surface now, but most are still no-op wrappers until richer Android page or fragment hosting is introduced.

## 2026-05-14 01:05 Asia/Shanghai - Start Splitting Public Android MapView From Internal Flutter PlatformView

- Changed:
  - Added public `CesiumMapView` and `CesiumRenderStats` under `sdk/android-sdk` as the start of a cleaner Android SDK surface.
  - Extracted the old `TextureView` and EGL render host into `internal/render/CesiumMapRenderSurface`.
  - Moved Flutter-only `PlatformView` classes into `internal/flutter`, so they now read as adapter internals instead of Android public API.
  - Simplified the old internal control boundary by deleting `CesiumMapController.kt`; `PlatformView` now forwards directly into `CesiumMapView`.
  - Updated the Flutter Android plugin imports so auto-registration now points at the new internal Flutter bridge package.
- Verified:
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `./gradlew :app:compileDebugKotlin` passed from `examples/flutter-demo/android`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - `NativeCesiumBridge` source file now lives under `internal/render`, but its Kotlin package name intentionally stays unchanged to preserve existing JNI symbol names.

## 2026-05-14 00:50 Asia/Shanghai - Upgrade sdk/flutter-plugin To A Real Android Auto-Registering Flutter Plugin

- Changed:
  - Added Flutter plugin metadata to `sdk/flutter-plugin/pubspec.yaml` for Android.
  - Added `sdk/flutter-plugin/android` with `CesiumMapSdkPlugin.kt` so FlutterEngine now auto-registers the PlatformView factory.
  - Reused the existing Android SDK and native core sources from the plugin Android module instead of keeping registration logic in the demo host.
  - Removed the demo app's manual PlatformView registration from `MainActivity.kt`.
  - Removed the demo Android shell's direct `:sdk-android` module dependency so the Flutter plugin now owns Android-side integration.
- Verified:
  - `flutter pub get` passed from `sdk/flutter-plugin`.
  - `flutter pub get` passed from `examples/flutter-demo`.
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - `sdk/android-sdk` still exists as a first-class native SDK module, but the Flutter plugin Android module now compiles against the same source tree directly so plugin auto-registration can remain self-contained for Flutter consumers.

## 2026-05-14 00:35 Asia/Shanghai - Land sdk/flutter-plugin And Make Demo Consume It

- Changed:
  - Added a real Flutter package under `sdk/flutter-plugin`.
  - Moved Flutter-side SDK models, controller wrapper, and `CesiumMapWidget` out of `examples/flutter-demo/lib/main.dart`.
  - Updated the demo app so it now consumes `package:cesium_map_sdk/cesium_map_sdk.dart` instead of owning the platform-channel integration inline.
  - Added `getStats` support on the Android platform-view channel so the Flutter controller contract is internally consistent.
  - Updated current docs and READMEs to reflect the new Flutter package boundary.
- Verified:
  - `flutter analyze` passed from `sdk/flutter-plugin`.
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - Flutter-side auto-registration is still demo-host-driven through `MainActivity`; the Flutter package is now real, but it is not yet a fully self-registering Flutter plugin on Android.

## 2026-05-14 00:20 Asia/Shanghai - Move Flutter Demo Under examples/flutter-demo

- Changed:
  - Moved the active Flutter demo project files into `examples/flutter-demo`.
  - Fixed the demo Android shell so it resolves `sdk/android-sdk` and `sdk/native-core` from the new location.
  - Updated current docs and run instructions so Flutter commands now run from `examples/flutter-demo`.
- Verified:
  - `flutter analyze` passed from `examples/flutter-demo`.
  - `flutter build apk --debug --target-platform android-arm64` passed from `examples/flutter-demo`.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - The repo root is now primarily a workspace and docs root; future automation and docs should avoid assuming the Flutter app still lives there.

## 2026-05-14 00:05 Asia/Shanghai - Start Formal SDK Directory Split

- Changed:
  - Moved active Android host code into `sdk/android-sdk/src/main/kotlin`.
  - Moved active C++ core code into `sdk/native-core/src/main/cpp`.
  - Updated `android/app/build.gradle.kts` so the current demo app shell builds against those new SDK directories through `sourceSets` and `externalNativeBuild`.
  - Added `sdk/android-sdk/README.md`, `sdk/native-core/README.md`, and `examples/flutter-demo/README.md` to make the new directory roles explicit.
  - Updated current docs so they describe the new source layout instead of the old `android/app/src/main/*` layout.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - The repo is now physically split at the source level, but the Flutter demo still lives at the repository root and `android/app` is still a demo shell rather than a standalone SDK distribution module.

## 2026-05-13 03:35 Asia/Shanghai - Remove Historical Canvas Reference Code And Keep Only Active Surface

- Changed:
  - Removed the unused historical Canvas renderer files `GaodeCanvasTileRenderer.kt` and `NativeMapRenderer.kt`.
  - Introduced `CesiumCameraState.kt` and moved the active Kotlin host path fully onto `Cesium*` naming.
  - Cleaned current architecture docs so they no longer describe the deleted reference code as part of the active tree.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - Historical session log entries still mention the removed files because they record what existed at the time; this is intentional and not a sign that the deleted code is still active.

## 2026-05-13 03:25 Asia/Shanghai - Rename Flutter-Side Demo Types Toward SDK Terms

- Changed:
  - Renamed Flutter-side helper and model types from generic or probe-style names toward SDK terms, including `CesiumCameraState`, `CesiumRenderStats`, `CesiumMapError`, and `_CesiumMapControllerAdapter`.
  - Kept the runtime behavior unchanged while making the demo code read more like a thin SDK consumer.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - Some Kotlin-side legacy names such as `NativeCameraState` and `GaodeCanvasTileRenderer` still remain because they are either internal compatibility helpers or historical reference code.

## 2026-05-13 03:20 Asia/Shanghai - Rename Native Host Classes Toward CesiumMap Terms

- Changed:
  - Renamed the active native PlatformView host files and classes from `GaodeSatellite*` to `CesiumMap*`.
  - Updated `MainActivity.kt`, the factory, the platform view host, and current architecture docs to use the new names.
  - Kept historical log entries unchanged so past session records still reflect the names that were true at the time.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - The codebase still contains other historical names such as `GaodeCanvasTileRenderer`, so the naming convergence is still in progress.

## 2026-05-13 03:15 Asia/Shanghai - Start Converging PoC Naming Toward SDK Terms

- Changed:
  - Added `CesiumMapConstants.kt` and introduced `cesium_map_view` as the new primary PlatformView type.
  - Kept the legacy `gaode_satellite_view` registration for compatibility while routing both view types through the same native implementation.
  - Updated the Flutter demo to use `cesium_map_view` and renamed the top-level demo app/page classes toward SDK-oriented naming.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - Native class/file names still contain historical Gaode naming, so the naming convergence is only partially complete.

## 2026-05-13 03:10 Asia/Shanghai - Add Camera Changed Event For SDK V1

- Changed:
  - Added native `cameraChanged` event emission when the current camera state is updated.
  - Updated the Flutter side to receive `cameraChanged` and sync the local camera state through the controller wrapper.
  - Refreshed `docs/SDK_API_V1.md` so the V1 capability mapping now includes landed `cameraChanged` support.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - `cameraChanged` currently emits on camera writes from the current host path and is not yet a richer change stream with source metadata or animation lifecycle semantics.

## 2026-05-13 03:05 Asia/Shanghai - Add Map Ready And Error Events For SDK V1

- Changed:
  - Added native `mapReady` and `error` event emission to the current platform view path.
  - Updated the Flutter side to receive and track ready/error state through the small controller wrapper.
  - Refreshed `docs/SDK_API_V1.md` so the capability mapping now reflects landed `getCameraState`, `mapReady`, and `error` event support.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - `onCameraChanged` is still not a formal event stream yet, and the error surface is still a lightweight payload rather than a finalized SDK error type.

## 2026-05-13 02:55 Asia/Shanghai - Start Landing SDK API V1 In Code

- Changed:
  - Added internal `CesiumMapController` on Android to give the current `TextureView` host a more SDK-like control boundary.
  - Updated `GaodeSatellitePlatformView.kt` to route channel commands through that controller and added `getCameraState`.
  - Added a small Dart-side `_NativeMapController` wrapper plus `CameraState` mapping so Flutter no longer talks to the raw `MethodChannel` inline for core map commands.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
- Not verified:
  - Android runtime was not relaunched after this code change.
- Notes / risks:
  - This only lands the first slice of the V1 contract. `onMapReady`, unified error events, and real interaction options are still missing.

## 2026-05-13 02:45 Asia/Shanghai - Freeze A Minimal SDK API V1 Contract

- Changed:
  - Added `docs/SDK_API_V1.md` to turn the broader API draft into a smaller first-pass formal contract.
  - Mapped the proposed V1 API to current code capabilities and explicitly listed deferred items for V2+.
  - Linked the V1 contract doc from `docs/PROJECT_INDEX.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - The V1 contract intentionally mirrors current `zoom`-based camera input rather than a richer camera model so it can stay close to today's implementation.

## 2026-05-13 02:40 Asia/Shanghai - Add SDK Blueprint And API Draft

- Changed:
  - Added `docs/SDK_BLUEPRINT.md` to define the target SDK product structure around `native-core`, `android-sdk`, `flutter-plugin`, and demo apps.
  - Added `docs/SDK_API_DRAFT.md` with a first-pass Android and Flutter API sketch.
  - Linked both SDK docs from `docs/PROJECT_INDEX.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - The API draft is intentionally small and directional; it still needs to be checked against real lifecycle, gesture, and rendering constraints before being treated as a formal contract.

## 2026-05-13 02:35 Asia/Shanghai - Add Migration Path To Native Map Page Plus Flutter Overlay

- Changed:
  - Added `docs/ARCHITECTURE_MIGRATION.md` with a phased migration plan from the current embedded map structure to `native map page + Flutter overlay`.
  - Linked the migration doc from `docs/PROJECT_INDEX.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - The migration plan is intentionally phased and conservative; it should be updated once real implementation constraints appear in the native page and overlay setup.

## 2026-05-13 02:30 Asia/Shanghai - Switch Ideal Architecture To Native Map Page Plus Flutter Overlay

- Changed:
  - Updated `docs/ARCHITECTURE_IDEAL.md` to make the recommended target structure `native map page + Flutter overlay`.
  - Reworked the ideal diagrams so the native page becomes the map scene host and Flutter becomes the business overlay layer above it.
  - Clarified that map touches belong to the native page by default, while Flutter overlay only consumes touches in its own hit regions.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - The current implementation still uses a Flutter page with an embedded native map view, so this remains a target-state architecture, not today's exact runtime structure.

## 2026-05-13 02:25 Asia/Shanghai - Clarify Responsibilities For Product Interaction And Map Gestures

- Changed:
  - Tightened `docs/ARCHITECTURE_IDEAL.md` so Flutter is described as the product interaction and orchestration layer, not the map gesture layer.
  - Moved map-level gesture responsibilities such as pan, zoom, rotate, tilt, and inertia into the native host layer in the ideal architecture.
  - Updated the ideal architecture diagrams to show a separate native gesture and control-input stage.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - The current codebase does not yet contain a fully explicit native gesture subsystem matching the ideal diagram.

## 2026-05-13 02:20 Asia/Shanghai - Add Ideal Architecture Diagram In Chinese

- Changed:
  - Added `docs/ARCHITECTURE_IDEAL.md` with a Chinese ideal-state architecture diagram and layer responsibilities.
  - Linked the new architecture doc from `docs/PROJECT_INDEX.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - This document describes the target architecture, not a promise that every detail already exists in the current code.

## 2026-05-13 02:15 Asia/Shanghai - Align Docs With Current Cesium Renderer Path

- Changed:
  - Updated `README.md`, `docs/PROJECT_INDEX.md`, and `docs/AI_HANDOFF.md` to match the current runtime architecture.
  - Reframed the main path as `Flutter shell -> Android PlatformView/TextureView/EGL host -> C++ cesium_bridge renderer`.
  - Marked `NativeMapRenderer` and `GaodeCanvasTileRenderer` as earlier probe-stage artifacts instead of the active renderer path.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation update.
- Notes / risks:
  - File names and some historical comments still reflect the earlier Gaode probe stage, so future sessions should continue trusting current code over older wording.

## 2026-05-13 02:00 Asia/Shanghai - Add Fast PoC Execution Rule

- Changed:
  - Added guidance that early PoC tasks should take large practical steps toward runnable end-to-end capability.
  - Updated `docs/PROJECT_INDEX.md`, `docs/HANDOFF_PROTOCOL.md`, and `docs/AI_HANDOFF.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Keep enough verification to catch obvious breakage, but record non-blocking risks instead of slowing early PoC progress.

## 2026-05-13 01:55 Asia/Shanghai - Link Real Cesium Native

- Changed:
  - Made Android CMake require a real Cesium Native package instead of allowing JNI-only fallback.
  - Defaulted `cesiumNativeRoot` to `/Users/ldy/Desktop/work/globe/third_party/cesium-native/build-android-arm64-v8a`.
  - Added Cesium Native install and vcpkg roots to CMake package/library/include lookup.
  - Replaced handwritten WGS84 ECEF math with `CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian`.
  - Added native per-frame tick stats through the Cesium bridge.
  - Updated docs from `cesiumNativeDir` to `cesiumNativeRoot`.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - APK `libcesium_bridge.so` contains `CesiumGeospatial` symbols and `cesium-native`, with no `jni-bridge` string.
  - APK installed and launched on adb device `7e045e39` after force-stop.
  - Screenshot `/tmp/cesium_native_real_link_restart_probe.png` showed satellite tiles and `cesium linked` in stats.
  - Logcat scan found no app crash, fatal exception, or `UnsatisfiedLinkError`.
- Not verified:
  - The visible imagery is still the Kotlin Canvas Gaode tile renderer, not Cesium-rendered 3D tiles.
- Notes / risks:
  - The app now depends on the local Cesium Native Android arm64 build root. A missing or stale root fails the native build immediately.

## 2026-05-13 01:35 Asia/Shanghai - Add Native Renderer Boundary

- Changed:
  - Added `NativeMapRenderer.kt` with `NativeMapRenderer`, `NativeCameraState`, and `NativeRenderStats`.
  - Moved Gaode tile loading, bitmap cache, Canvas drawing, and renderer cleanup into `GaodeCanvasTileRenderer.kt`.
  - Slimmed `GaodeSatellitePlatformView.kt` so `GaodeSatelliteView` now coordinates the frame loop, renderer, Cesium JNI bridge, stats, and lifecycle.
  - Updated README and handoff docs to describe the renderer boundary.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - APK still contains `lib/arm64-v8a/libcesium_bridge.so`.
  - APK installed and launched on adb device `7e045e39`.
  - Screenshot `/tmp/cesium_renderer_abstraction_probe.png` showed satellite tiles and `cesium jni-bridge` in stats.
  - Logcat scan found no app crash, fatal exception, or `UnsatisfiedLinkError`.
- Not verified:
  - Automated tests were not run because this PoC currently has no `test/` directory by project rule.
  - Did not link a real Cesium Native install yet; no local `cesium-native` package path was provided.
- Notes / risks:
  - Rendering still uses the Kotlin Canvas tile renderer; the new boundary is the insertion point for a Cesium-backed renderer or owner.

## 2026-05-13 01:30 Asia/Shanghai - Remove Early PoC Test Code

- Changed:
  - Removed the Flutter widget smoke check and deleted the empty `test` directory.
  - Removed the obsolete Flutter testing dev dependency and refreshed package resolution.
  - Updated handoff docs so early PoC verification uses `flutter analyze`, APK build, and emulator/device validation instead of automated test code.
- Verified:
  - `flutter pub get` passed.
  - `flutter analyze` passed.
- Not verified:
  - APK build and Android runtime were not relaunched for this documentation/dependency cleanup.
- Notes / risks:
  - Keep avoiding automated test files until the project stabilizes enough that they reduce, rather than add, AI handoff burden.

## 2026-05-13 01:20 Asia/Shanghai - Add Android Cesium Native JNI Bridge

- Changed:
  - Added Android CMake build under `android/app/src/main/cpp`.
  - Added `libcesium_bridge.so` JNI layer with native camera/cache lifecycle calls and ECEF probe stats.
  - Added `NativeCesiumBridge.kt` to own the native handle from the Android `PlatformView`.
  - Mirrored Flutter camera updates and clear-memory commands into the native bridge.
  - Added Cesium bridge status to Flutter stats as `cesium linked` or `cesium jni-bridge`.
  - Added optional Gradle property `-PcesiumNativeDir=/absolute/path` to link an installed Cesium Native CMake package.
  - Updated README and handoff docs.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - APK installed and launched on adb device `7e045e39`.
  - Screenshot `/tmp/cesium_native_bridge_probe.png` showed satellite tiles and `cesium jni-bridge` in stats.
  - Logcat scan found no app crash, fatal exception, or `UnsatisfiedLinkError`.
- Not verified:
  - Did not link a real Cesium Native install yet; no local `cesium-native` package path was provided.
- Notes / risks:
  - Rendering still uses the Kotlin Canvas tile probe.
  - The new bridge is the attachment point for Cesium Native ownership, not the final Cesium renderer.

## 2026-05-13 00:55 Asia/Shanghai - Add Lean Modular Code Rule

- Changed:
  - Added project guidance to keep code small, modular, and easy for future AI sessions to understand.
  - Updated `docs/PROJECT_INDEX.md`, `docs/HANDOFF_PROTOCOL.md`, and `docs/AI_HANDOFF.md`.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Future changes should avoid speculative abstractions and only split modules around real boundaries.

## 2026-05-13 00:50 Asia/Shanghai - Make Low-Token Handoff The Default

- Changed:
  - Updated `README.md`, `docs/PROJECT_INDEX.md`, `docs/HANDOFF_PROTOCOL.md`, and `docs/AI_HANDOFF.md` so low-token handoff is the project default, not something the user must request each time.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Future project-based AI sessions should start with `PROJECT_INDEX.md` plus recent `SESSION_LOG.md` entries automatically.

## 2026-05-13 00:45 Asia/Shanghai - Add Low-Token Handoff Entry

- Changed:
  - Added `docs/PROJECT_INDEX.md` as the short first-read entry point.
  - Updated `docs/HANDOFF_PROTOCOL.md` to read only the index, recent log entries, and task-related files by default.
  - Updated `README.md` and `docs/AI_HANDOFF.md` to point to the low-token path.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - Future sessions should avoid loading full docs/code unless the task requires it.

## 2026-05-13 00:40 Asia/Shanghai - Add Cross-Session Handoff Docs

- Changed:
  - Added `docs/HANDOFF_PROTOCOL.md` with start/end-of-session rules.
  - Added this `docs/SESSION_LOG.md` for future change records.
  - Updated `docs/AI_HANDOFF.md` to point future sessions to both files.
- Verified:
  - Documentation-only change; no build required.
- Not verified:
  - Android runtime was not relaunched for this documentation-only update.
- Notes / risks:
  - The project directory may not be a git repository, so future sessions should inspect files directly and use this log as the continuity layer.

## 2026-05-13 00:35 Asia/Shanghai - Initial Android Tile Probe And Handoff

- Changed:
  - Created Flutter Android PoC at `/Users/ldy/Desktop/work/cesium_native_android_poc`.
  - Added Android `PlatformView` rendering Gaode satellite XYZ tiles with Kotlin `Canvas`.
  - Added Flutter controls for camera sync, zoom, orbit, cache clear, and stats display.
  - Restricted Android build to `arm64-v8a`.
  - Added `docs/AI_HANDOFF.md` and updated `README.md`.
- Verified:
  - `flutter analyze` passed.
  - `flutter build apk --debug --target-platform android-arm64` passed.
  - Installed on adb device `7e045e39`; satellite tiles rendered successfully with about `60fps`, `63/63 tiles`, `0 errors`.
- Not verified:
  - No Cesium Native integration yet.
  - iOS and HarmonyOS not scaffolded.
- Notes / risks:
  - Direct Gaode tile endpoint is for development validation only and needs licensing/production review.

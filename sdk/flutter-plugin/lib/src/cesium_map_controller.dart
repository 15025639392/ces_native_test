import 'package:flutter/services.dart';

import 'cesium_camera_state.dart';
import 'cesium_gesture_options.dart';
import 'cesium_performance_options.dart';
import 'cesium_render_stats.dart';

abstract class CesiumMapController {
  Future<void> setCamera(CesiumCameraState camera);

  Future<CesiumCameraState> getCameraState();

  Future<bool> getInteractionEnabled();

  Future<CesiumGestureOptions> getGestureOptions();

  Future<CesiumPerformanceOptions> getPerformanceOptions();

  Future<void> setInteractionEnabled(bool enabled);

  Future<void> setGestureOptions(CesiumGestureOptions options);

  Future<void> setPerformanceOptions(CesiumPerformanceOptions options);

  Future<void> clearMemory();

  Future<CesiumRenderStats> getStats();
}

class MethodChannelCesiumMapController implements CesiumMapController {
  const MethodChannelCesiumMapController(this.channel);

  final MethodChannel channel;

  @override
  Future<void> setCamera(CesiumCameraState camera) async {
    await channel.invokeMethod<void>('updateCamera', camera.toMap());
  }

  @override
  Future<CesiumCameraState> getCameraState() async {
    final data = Map<String, Object?>.from(
      await channel.invokeMethod<Map<Object?, Object?>>('getCameraState') ??
          const {},
    );
    return CesiumCameraState.fromMap(data);
  }

  @override
  Future<bool> getInteractionEnabled() async {
    return await channel.invokeMethod<bool>('getInteractionEnabled') ?? true;
  }

  @override
  Future<CesiumGestureOptions> getGestureOptions() async {
    final data = Map<String, Object?>.from(
      await channel.invokeMethod<Map<Object?, Object?>>('getGestureOptions') ??
          const {},
    );
    return CesiumGestureOptions.fromMap(data);
  }

  @override
  Future<CesiumPerformanceOptions> getPerformanceOptions() async {
    final data = Map<String, Object?>.from(
      await channel.invokeMethod<Map<Object?, Object?>>(
            'getPerformanceOptions',
          ) ??
          const {},
    );
    return CesiumPerformanceOptions.fromMap(data);
  }

  @override
  Future<void> setInteractionEnabled(bool enabled) async {
    await channel.invokeMethod<void>('setInteractionEnabled', enabled);
  }

  @override
  Future<void> setGestureOptions(CesiumGestureOptions options) async {
    await channel.invokeMethod<void>('setGestureOptions', options.toMap());
  }

  @override
  Future<void> setPerformanceOptions(CesiumPerformanceOptions options) async {
    await channel.invokeMethod<void>('setPerformanceOptions', options.toMap());
  }

  @override
  Future<void> clearMemory() async {
    await channel.invokeMethod<void>('clearMemory');
  }

  @override
  Future<CesiumRenderStats> getStats() async {
    final data = Map<String, Object?>.from(
      await channel.invokeMethod<Map<Object?, Object?>>('getStats') ?? const {},
    );
    return CesiumRenderStats.fromMap(data);
  }
}

import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'cesium_camera_state.dart';
import 'cesium_gesture_options.dart';
import 'cesium_map_controller.dart';
import 'cesium_map_error.dart';
import 'cesium_performance_options.dart';
import 'cesium_render_stats.dart';

const _cesiumMapViewType = 'cesium_map_view';

class CesiumMapWidget extends StatefulWidget {
  const CesiumMapWidget({
    super.key,
    this.initialCamera = const CesiumCameraState(
      longitude: 116.397389,
      latitude: 39.908722,
      zoom: 15,
      autoOrbit: false,
    ),
    this.interactionEnabled = true,
    this.gestureOptions = const CesiumGestureOptions(),
    this.performanceOptions = const CesiumPerformanceOptions(),
    this.onMapCreated,
    this.onMapReady,
    this.onCameraMoveStarted,
    this.onCameraChanged,
    this.onCameraIdle,
    this.onRenderStats,
    this.onError,
    this.unsupportedBuilder,
  });

  final CesiumCameraState initialCamera;
  final bool interactionEnabled;
  final CesiumGestureOptions gestureOptions;
  final CesiumPerformanceOptions performanceOptions;
  final ValueChanged<CesiumMapController>? onMapCreated;
  final VoidCallback? onMapReady;
  final ValueChanged<CesiumCameraState>? onCameraMoveStarted;
  final ValueChanged<CesiumCameraState>? onCameraChanged;
  final ValueChanged<CesiumCameraState>? onCameraIdle;
  final ValueChanged<CesiumRenderStats>? onRenderStats;
  final ValueChanged<CesiumMapError>? onError;
  final WidgetBuilder? unsupportedBuilder;

  @override
  State<CesiumMapWidget> createState() => _CesiumMapWidgetState();
}

class _CesiumMapWidgetState extends State<CesiumMapWidget> {
  bool get _canShowAndroidView => !kIsWeb && Platform.isAndroid;

  Future<void> _onPlatformViewCreated(int id) async {
    final controller = MethodChannelCesiumMapController(
      MethodChannel('${_cesiumMapViewType}_$id'),
    );
    final channel = controller.channel;
    channel.setMethodCallHandler((call) async {
      if (!mounted) return;

      if (call.method == 'stats') {
        final data = Map<String, Object?>.from(call.arguments as Map);
        widget.onRenderStats?.call(CesiumRenderStats.fromMap(data));
      } else if (call.method == 'mapReady') {
        widget.onMapReady?.call();
      } else if (call.method == 'error') {
        final data = Map<String, Object?>.from(call.arguments as Map);
        widget.onError?.call(CesiumMapError.fromMap(data));
      } else if (call.method == 'cameraMoveStarted') {
        final data = Map<String, Object?>.from(call.arguments as Map);
        widget.onCameraMoveStarted?.call(CesiumCameraState.fromMap(data));
      } else if (call.method == 'cameraChanged') {
        final data = Map<String, Object?>.from(call.arguments as Map);
        widget.onCameraChanged?.call(CesiumCameraState.fromMap(data));
      } else if (call.method == 'cameraIdle') {
        final data = Map<String, Object?>.from(call.arguments as Map);
        widget.onCameraIdle?.call(CesiumCameraState.fromMap(data));
      }
    });

    widget.onMapCreated?.call(controller);
    await controller.setInteractionEnabled(widget.interactionEnabled);
    await controller.setGestureOptions(widget.gestureOptions);
    await controller.setPerformanceOptions(widget.performanceOptions);
    await controller.setCamera(widget.initialCamera);
  }

  @override
  Widget build(BuildContext context) {
    if (!_canShowAndroidView) {
      return widget.unsupportedBuilder?.call(context) ??
          const Center(child: Text('该 Flutter 适配层当前仅支持 Android PlatformView。'));
    }

    return AndroidView(
      viewType: _cesiumMapViewType,
      onPlatformViewCreated: _onPlatformViewCreated,
      creationParams: {
        ...widget.initialCamera.toMap(),
        'interactionEnabled': widget.interactionEnabled,
        ...widget.gestureOptions.toMap(),
        ...widget.performanceOptions.toMap(),
      },
      creationParamsCodec: const StandardMessageCodec(),
    );
  }
}

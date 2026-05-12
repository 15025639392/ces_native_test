import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

void main() {
  runApp(const GaodeTileProbeApp());
}

class GaodeTileProbeApp extends StatelessWidget {
  const GaodeTileProbeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Gaode Tile Probe',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xff1c6b5a),
          brightness: Brightness.dark,
        ),
        scaffoldBackgroundColor: const Color(0xff111614),
        useMaterial3: true,
      ),
      home: const TileProbePage(),
    );
  }
}

class TileProbePage extends StatefulWidget {
  const TileProbePage({super.key});

  @override
  State<TileProbePage> createState() => _TileProbePageState();
}

class _TileProbePageState extends State<TileProbePage> {
  MethodChannel? _viewChannel;
  double _longitude = 116.397389;
  double _latitude = 39.908722;
  double _zoom = 15;
  bool _autoOrbit = false;
  TileStats _stats = const TileStats();

  bool get _canShowAndroidView =>
      !kIsWeb && Platform.isAndroid;

  Future<void> _onPlatformViewCreated(int id) async {
    final channel = MethodChannel('gaode_satellite_view_$id');
    channel.setMethodCallHandler((call) async {
      if (call.method == 'stats') {
        final data = Map<String, Object?>.from(call.arguments as Map);
        setState(() => _stats = TileStats.fromMap(data));
      }
    });
    _viewChannel = channel;
    await _sendCamera();
  }

  Future<void> _sendCamera() async {
    await _viewChannel?.invokeMethod<void>('updateCamera', {
      'longitude': _longitude,
      'latitude': _latitude,
      'zoom': _zoom,
      'autoOrbit': _autoOrbit,
    });
  }

  Future<void> _clearNativeMemory() async {
    await _viewChannel?.invokeMethod<void>('clearMemory');
  }

  void _move(double lonDelta, double latDelta) {
    setState(() {
      _longitude = (_longitude + lonDelta).clamp(-180.0, 180.0);
      _latitude = (_latitude + latDelta).clamp(-85.0, 85.0);
    });
    _sendCamera();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Column(
          children: [
            Expanded(
              child: Stack(
                children: [
                  Positioned.fill(
                    child: _canShowAndroidView
                        ? AndroidView(
                            viewType: 'gaode_satellite_view',
                            onPlatformViewCreated: _onPlatformViewCreated,
                            creationParams: {
                              'longitude': _longitude,
                              'latitude': _latitude,
                              'zoom': _zoom,
                              'autoOrbit': _autoOrbit,
                            },
                            creationParamsCodec:
                                const StandardMessageCodec(),
                          )
                        : const _UnsupportedPlatform(),
                  ),
                  Positioned(
                    left: 12,
                    top: 12,
                    right: 12,
                    child: _StatsBar(stats: _stats),
                  ),
                ],
              ),
            ),
            _Controls(
              longitude: _longitude,
              latitude: _latitude,
              zoom: _zoom,
              autoOrbit: _autoOrbit,
              onZoomChanged: (value) {
                setState(() => _zoom = value);
                _sendCamera();
              },
              onMove: _move,
              onAutoOrbitChanged: (value) {
                setState(() => _autoOrbit = value);
                _sendCamera();
              },
              onClearMemory: _clearNativeMemory,
            ),
          ],
        ),
      ),
    );
  }
}

class _Controls extends StatelessWidget {
  const _Controls({
    required this.longitude,
    required this.latitude,
    required this.zoom,
    required this.autoOrbit,
    required this.onZoomChanged,
    required this.onMove,
    required this.onAutoOrbitChanged,
    required this.onClearMemory,
  });

  final double longitude;
  final double latitude;
  final double zoom;
  final bool autoOrbit;
  final ValueChanged<double> onZoomChanged;
  final void Function(double lonDelta, double latDelta) onMove;
  final ValueChanged<bool> onAutoOrbitChanged;
  final VoidCallback onClearMemory;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: const BoxDecoration(
        color: Color(0xff17201d),
        border: Border(top: BorderSide(color: Color(0xff2d3934))),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    'Lon ${longitude.toStringAsFixed(5)}  Lat ${latitude.toStringAsFixed(5)}',
                    style: Theme.of(context).textTheme.labelLarge,
                  ),
                ),
                IconButton.filledTonal(
                  tooltip: 'Clear native tile cache',
                  onPressed: onClearMemory,
                  icon: const Icon(Icons.memory),
                ),
                const SizedBox(width: 8),
                FilterChip(
                  label: const Text('Orbit'),
                  selected: autoOrbit,
                  onSelected: onAutoOrbitChanged,
                ),
              ],
            ),
            Row(
              children: [
                const Text('Zoom'),
                Expanded(
                  child: Slider(
                    value: zoom,
                    min: 3,
                    max: 19,
                    divisions: 16,
                    label: zoom.toStringAsFixed(0),
                    onChanged: onZoomChanged,
                  ),
                ),
                SizedBox(
                  width: 32,
                  child: Text(zoom.toStringAsFixed(0)),
                ),
              ],
            ),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                IconButton.filledTonal(
                  tooltip: 'West',
                  onPressed: () => onMove(-0.01, 0),
                  icon: const Icon(Icons.arrow_back),
                ),
                const SizedBox(width: 8),
                IconButton.filledTonal(
                  tooltip: 'South',
                  onPressed: () => onMove(0, -0.01),
                  icon: const Icon(Icons.arrow_downward),
                ),
                const SizedBox(width: 8),
                IconButton.filledTonal(
                  tooltip: 'North',
                  onPressed: () => onMove(0, 0.01),
                  icon: const Icon(Icons.arrow_upward),
                ),
                const SizedBox(width: 8),
                IconButton.filledTonal(
                  tooltip: 'East',
                  onPressed: () => onMove(0.01, 0),
                  icon: const Icon(Icons.arrow_forward),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _StatsBar extends StatelessWidget {
  const _StatsBar({required this.stats});

  final TileStats stats;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xdd101513),
        border: Border.all(color: const Color(0xff33413b)),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        child: Wrap(
          spacing: 16,
          runSpacing: 6,
          children: [
            _Metric(label: 'fps', value: stats.fps.toStringAsFixed(1)),
            _Metric(label: 'draw', value: '${stats.drawMs.toStringAsFixed(1)}ms'),
            _Metric(label: 'tiles', value: '${stats.visibleTiles}/${stats.cachedTiles}'),
            _Metric(label: 'loads', value: '${stats.loadedTiles}'),
            _Metric(label: 'errors', value: '${stats.failedTiles}'),
            _Metric(label: 'cache', value: '${stats.cacheMb.toStringAsFixed(1)}MB'),
            _Metric(
              label: 'cesium',
              value: stats.cesiumLinked ? 'linked' : stats.cesiumBackend,
            ),
          ],
        ),
      ),
    );
  }
}

class _Metric extends StatelessWidget {
  const _Metric({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Text.rich(
      TextSpan(
        children: [
          TextSpan(
            text: '$label ',
            style: const TextStyle(color: Color(0xff9aa8a1)),
          ),
          TextSpan(
            text: value,
            style: const TextStyle(fontWeight: FontWeight.w700),
          ),
        ],
      ),
    );
  }
}

class _UnsupportedPlatform extends StatelessWidget {
  const _UnsupportedPlatform();

  @override
  Widget build(BuildContext context) {
    return const Center(
      child: Text('This probe uses Android PlatformView and runs on Android only.'),
    );
  }
}

class TileStats {
  const TileStats({
    this.fps = 0,
    this.drawMs = 0,
    this.visibleTiles = 0,
    this.cachedTiles = 0,
    this.loadedTiles = 0,
    this.failedTiles = 0,
    this.cacheMb = 0,
    this.cesiumBackend = 'cesium-native',
    this.cesiumLinked = false,
    this.cesiumCameraUpdates = 0,
    this.cesiumMemoryClears = 0,
    this.cesiumFrameUpdates = 0,
    this.cesiumCameraDirty = false,
    this.cesiumFrameDt = 0,
    this.cesiumEcefX = 0,
    this.cesiumEcefY = 0,
    this.cesiumEcefZ = 0,
  });

  final double fps;
  final double drawMs;
  final int visibleTiles;
  final int cachedTiles;
  final int loadedTiles;
  final int failedTiles;
  final double cacheMb;
  final String cesiumBackend;
  final bool cesiumLinked;
  final int cesiumCameraUpdates;
  final int cesiumMemoryClears;
  final int cesiumFrameUpdates;
  final bool cesiumCameraDirty;
  final double cesiumFrameDt;
  final double cesiumEcefX;
  final double cesiumEcefY;
  final double cesiumEcefZ;

  factory TileStats.fromMap(Map<String, Object?> map) {
    double asDouble(String key) => (map[key] as num?)?.toDouble() ?? 0;
    int asInt(String key) => (map[key] as num?)?.toInt() ?? 0;

    return TileStats(
      fps: asDouble('fps'),
      drawMs: asDouble('drawMs'),
      visibleTiles: asInt('visibleTiles'),
      cachedTiles: asInt('cachedTiles'),
      loadedTiles: asInt('loadedTiles'),
      failedTiles: asInt('failedTiles'),
      cacheMb: asDouble('cacheMb'),
      cesiumBackend: map['cesiumBackend'] as String? ?? 'cesium-native',
      cesiumLinked: map['cesiumLinked'] as bool? ?? false,
      cesiumCameraUpdates: asInt('cesiumCameraUpdates'),
      cesiumMemoryClears: asInt('cesiumMemoryClears'),
      cesiumFrameUpdates: asInt('cesiumFrameUpdates'),
      cesiumCameraDirty: map['cesiumCameraDirty'] as bool? ?? false,
      cesiumFrameDt: asDouble('cesiumFrameDt'),
      cesiumEcefX: asDouble('cesiumEcefX'),
      cesiumEcefY: asDouble('cesiumEcefY'),
      cesiumEcefZ: asDouble('cesiumEcefZ'),
    );
  }
}

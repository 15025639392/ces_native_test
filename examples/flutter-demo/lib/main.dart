import 'package:cesium_map_sdk/cesium_map_sdk.dart';
import 'package:flutter/material.dart';

void main() {
  runApp(const CesiumMapSdkDemoApp());
}

class CesiumMapSdkDemoApp extends StatelessWidget {
  const CesiumMapSdkDemoApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Cesium Map SDK Demo',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xff1c6b5a),
          brightness: Brightness.dark,
        ),
        scaffoldBackgroundColor: const Color(0xff111614),
        useMaterial3: true,
      ),
      home: const CesiumMapDemoPage(),
    );
  }
}

class CesiumMapDemoPage extends StatefulWidget {
  const CesiumMapDemoPage({super.key});

  @override
  State<CesiumMapDemoPage> createState() => _CesiumMapDemoPageState();
}

class _CesiumMapDemoPageState extends State<CesiumMapDemoPage> {
  static const ImagerySource _imagerySource = UrlTemplateImagerySource(
    id: 'osm',
    urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
  );
  static const TerrainSource _terrainSource = QuantizedMeshTerrainSource(
    id: 'swisstopo_terrain',
    layerJsonUrl:
        'https://3d.geo.admin.ch/ch.swisstopo.terrain.3d/v1/20250101/layer.json',
  );

  double _longitude = 7.75;
  double _latitude = 46.02;
  double _altitudeMeters = 80000.0;
  double _bearing = 25;
  double _pitch = 55;
  bool _autoOrbit = false;
  bool _mapReady = false;
  String? _lastErrorMessage;
  CesiumRenderStats _stats = const CesiumRenderStats();

  CesiumCameraState get _camera => CesiumCameraState(
    longitude: _longitude,
    latitude: _latitude,
    altitudeMeters: _altitudeMeters,
    autoOrbit: _autoOrbit,
    bearing: _bearing,
    pitch: _pitch,
  );

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Stack(
          children: [
            Positioned.fill(
              child: CesiumMapWidget(
                initialCamera: _camera,
                imagerySource: _imagerySource,
                terrainSource: _terrainSource,
                onMapReady: () {
                  if (!mounted) return;
                  setState(() {
                    _mapReady = true;
                    _lastErrorMessage = null;
                  });
                },
                onCameraChanged: (camera) {
                  if (!mounted) return;
                  setState(() {
                    _longitude = camera.longitude;
                    _latitude = camera.latitude;
                    _altitudeMeters = camera.altitudeMeters;
                    _bearing = camera.bearing;
                    _pitch = camera.pitch;
                    _autoOrbit = camera.autoOrbit;
                  });
                },
                onRenderStats: (stats) {
                  if (!mounted) return;
                  setState(() => _stats = stats);
                },
                onError: (error) {
                  if (!mounted) return;
                  setState(() => _lastErrorMessage = error.message);
                  debugPrint(
                    'Native map error [${error.code}]: '
                    '${error.message} (${error.details ?? 'no details'})',
                  );
                },
              ),
            ),
            Positioned(
              left: 12,
              top: 12,
              right: 12,
              child: _StatsBar(
                stats: _stats,
                mapReady: _mapReady,
                errorMessage: _lastErrorMessage,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _StatsBar extends StatelessWidget {
  const _StatsBar({
    required this.stats,
    required this.mapReady,
    required this.errorMessage,
  });

  final CesiumRenderStats stats;
  final bool mapReady;
  final String? errorMessage;

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
            _Metric(
              label: 'draw',
              value: '${stats.drawMs.toStringAsFixed(1)}ms',
            ),
            _Metric(
              label: 'tiles',
              value: '${stats.visibleTiles}/${stats.cachedTiles}',
            ),
            _Metric(label: 'loads', value: '${stats.loadedTiles}'),
            _Metric(label: 'errors', value: '${stats.failedTiles}'),
            _Metric(
              label: 'cache',
              value: '${stats.cacheMb.toStringAsFixed(1)}MB',
            ),
            _Metric(
              label: 'cesium',
              value: stats.cesiumLinked ? 'linked' : stats.cesiumBackend,
            ),
            _Metric(label: 'ready', value: mapReady ? 'yes' : 'no'),
            const _Metric(label: 'terrain', value: 'swisstopo'),
            if (errorMessage != null && errorMessage!.isNotEmpty)
              _Metric(label: 'error', value: errorMessage!),
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

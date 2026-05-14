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
  CesiumMapController? _mapController;
  double _longitude = 116.397389;
  double _latitude = 39.908722;
  double _zoom = 15;
  double _bearing = 0;
  double _pitch = 0;
  bool _autoOrbit = false;
  bool _mapReady = false;
  String? _lastErrorMessage;
  CesiumRenderStats _stats = const CesiumRenderStats();

  CesiumCameraState get _camera => CesiumCameraState(
    longitude: _longitude,
    latitude: _latitude,
    zoom: _zoom,
    autoOrbit: _autoOrbit,
    bearing: _bearing,
    pitch: _pitch,
  );

  Future<void> _sendCamera() async {
    await _mapController?.setCamera(_camera);
  }

  Future<void> _syncCameraFromNative() async {
    final camera = await _mapController?.getCameraState();
    if (!mounted || camera == null) return;
    setState(() {
      _longitude = camera.longitude;
      _latitude = camera.latitude;
      _zoom = camera.zoom;
      _bearing = camera.bearing;
      _pitch = camera.pitch;
      _autoOrbit = camera.autoOrbit;
    });
  }

  Future<void> _clearNativeMemory() async {
    await _mapController?.clearMemory();
    await _syncCameraFromNative();
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
                    child: CesiumMapWidget(
                      initialCamera: _camera,
                      onMapCreated: (controller) => _mapController = controller,
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
                          _zoom = camera.zoom;
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
            _Controls(
              longitude: _longitude,
              latitude: _latitude,
              zoom: _zoom,
              bearing: _bearing,
              pitch: _pitch,
              autoOrbit: _autoOrbit,
              onZoomChanged: (value) {
                setState(() => _zoom = value);
                _sendCamera();
              },
              onBearingChanged: (value) {
                setState(() => _bearing = value);
                _sendCamera();
              },
              onPitchChanged: (value) {
                setState(() => _pitch = value);
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
    required this.bearing,
    required this.pitch,
    required this.autoOrbit,
    required this.onZoomChanged,
    required this.onBearingChanged,
    required this.onPitchChanged,
    required this.onMove,
    required this.onAutoOrbitChanged,
    required this.onClearMemory,
  });

  final double longitude;
  final double latitude;
  final double zoom;
  final double bearing;
  final double pitch;
  final bool autoOrbit;
  final ValueChanged<double> onZoomChanged;
  final ValueChanged<double> onBearingChanged;
  final ValueChanged<double> onPitchChanged;
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
                    'Lon ${longitude.toStringAsFixed(5)}  '
                    'Lat ${latitude.toStringAsFixed(5)}  '
                    'Bearing ${bearing.toStringAsFixed(0)}  '
                    'Pitch ${pitch.toStringAsFixed(0)}',
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
                SizedBox(width: 32, child: Text(zoom.toStringAsFixed(0))),
              ],
            ),
            Row(
              children: [
                const Text('Bearing'),
                Expanded(
                  child: Slider(
                    value: bearing,
                    min: 0,
                    max: 360,
                    label: bearing.toStringAsFixed(0),
                    onChanged: onBearingChanged,
                  ),
                ),
                SizedBox(width: 32, child: Text(bearing.toStringAsFixed(0))),
              ],
            ),
            Row(
              children: [
                const Text('Pitch'),
                Expanded(
                  child: Slider(
                    value: pitch,
                    min: 0,
                    max: 85,
                    label: pitch.toStringAsFixed(0),
                    onChanged: onPitchChanged,
                  ),
                ),
                SizedBox(width: 32, child: Text(pitch.toStringAsFixed(0))),
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

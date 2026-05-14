class CesiumRenderStats {
  const CesiumRenderStats({
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

  factory CesiumRenderStats.fromMap(Map<String, Object?> map) {
    double asDouble(String key) => (map[key] as num?)?.toDouble() ?? 0;
    int asInt(String key) => (map[key] as num?)?.toInt() ?? 0;

    return CesiumRenderStats(
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

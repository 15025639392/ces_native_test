class CesiumCameraState {
  const CesiumCameraState({
    required this.longitude,
    required this.latitude,
    required this.zoom,
    required this.autoOrbit,
    this.bearing = 0,
    this.pitch = 0,
  });

  final double longitude;
  final double latitude;
  final double zoom;
  final bool autoOrbit;
  final double bearing;
  final double pitch;

  Map<String, Object?> toMap() => {
    'longitude': longitude,
    'latitude': latitude,
    'zoom': zoom,
    'autoOrbit': autoOrbit,
    'bearing': bearing,
    'pitch': pitch,
  };

  factory CesiumCameraState.fromMap(Map<String, Object?> map) {
    double asDouble(String key) => (map[key] as num?)?.toDouble() ?? 0;

    return CesiumCameraState(
      longitude: asDouble('longitude'),
      latitude: asDouble('latitude'),
      zoom: asDouble('zoom'),
      autoOrbit: map['autoOrbit'] as bool? ?? false,
      bearing: asDouble('bearing'),
      pitch: asDouble('pitch'),
    );
  }
}

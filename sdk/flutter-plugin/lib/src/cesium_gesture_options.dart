class CesiumGestureOptions {
  const CesiumGestureOptions({
    this.panEnabled = true,
    this.zoomEnabled = true,
    this.rotateEnabled = false,
    this.tiltEnabled = false,
    this.inertiaEnabled = false,
  });

  final bool panEnabled;
  final bool zoomEnabled;
  final bool rotateEnabled;
  final bool tiltEnabled;
  final bool inertiaEnabled;

  Map<String, Object?> toMap() => {
    'panEnabled': panEnabled,
    'zoomEnabled': zoomEnabled,
    'rotateEnabled': rotateEnabled,
    'tiltEnabled': tiltEnabled,
    'inertiaEnabled': inertiaEnabled,
  };

  factory CesiumGestureOptions.fromMap(Map<String, Object?> map) {
    return CesiumGestureOptions(
      panEnabled: map['panEnabled'] as bool? ?? true,
      zoomEnabled: map['zoomEnabled'] as bool? ?? true,
      rotateEnabled: map['rotateEnabled'] as bool? ?? false,
      tiltEnabled: map['tiltEnabled'] as bool? ?? false,
      inertiaEnabled: map['inertiaEnabled'] as bool? ?? false,
    );
  }
}

class CesiumPerformanceOptions {
  const CesiumPerformanceOptions({
    this.maximumScreenSpaceError = 4.0,
  }) : assert(maximumScreenSpaceError > 0.0);

  final double maximumScreenSpaceError;

  Map<String, Object?> toMap() => {
    'maximumScreenSpaceError': maximumScreenSpaceError,
  };

  factory CesiumPerformanceOptions.fromMap(Map<String, Object?> map) {
    return CesiumPerformanceOptions(
      maximumScreenSpaceError:
          (map['maximumScreenSpaceError'] as num?)?.toDouble() ?? 4.0,
    );
  }
}

class CesiumPerformanceOptions {
  const CesiumPerformanceOptions({
    this.maximumScreenSpaceError = 4.0,
    this.movementMaximumScreenSpaceError = 8.0,
  }) : assert(maximumScreenSpaceError > 0.0),
       assert(
         movementMaximumScreenSpaceError == null ||
             movementMaximumScreenSpaceError > 0.0,
       );

  final double maximumScreenSpaceError;
  final double? movementMaximumScreenSpaceError;

  Map<String, Object?> toMap() => {
    'maximumScreenSpaceError': maximumScreenSpaceError,
    'movementMaximumScreenSpaceError': movementMaximumScreenSpaceError,
  };

  factory CesiumPerformanceOptions.fromMap(Map<String, Object?> map) {
    return CesiumPerformanceOptions(
      maximumScreenSpaceError:
          (map['maximumScreenSpaceError'] as num?)?.toDouble() ?? 4.0,
      movementMaximumScreenSpaceError:
          (map['movementMaximumScreenSpaceError'] as num?)?.toDouble() ?? 8.0,
    );
  }
}

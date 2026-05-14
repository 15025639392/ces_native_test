class CesiumMapError {
  const CesiumMapError({
    required this.code,
    required this.message,
    this.details,
  });

  final String code;
  final String message;
  final String? details;

  factory CesiumMapError.fromMap(Map<String, Object?> map) {
    return CesiumMapError(
      code: map['code'] as String? ?? 'unknown_error',
      message: map['message'] as String? ?? 'Unknown native map error.',
      details: map['details'] as String?,
    );
  }
}

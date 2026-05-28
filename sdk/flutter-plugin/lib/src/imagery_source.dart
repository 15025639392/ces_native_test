sealed class ImagerySource {
  const ImagerySource({
    required this.id,
  });

  final String id;

  Map<String, Object?> toMap();

  static ImagerySource? fromMap(Map<Object?, Object?>? map) {
    if (map == null || map.isEmpty) return null;
    final normalized = Map<String, Object?>.from(map);
    final type = normalized['type'] as String?;
    switch (type) {
      case UrlTemplateImagerySource.type:
        return UrlTemplateImagerySource.fromMap(normalized);
      default:
        return null;
    }
  }
}

class UrlTemplateImagerySource extends ImagerySource {
  const UrlTemplateImagerySource({
    required super.id,
    required this.urlTemplate,
  });

  static const String type = 'urlTemplate';

  final String urlTemplate;

  @override
  Map<String, Object?> toMap() {
    return <String, Object?>{
      'type': type,
      'id': id,
      'urlTemplate': urlTemplate,
    };
  }

  static UrlTemplateImagerySource fromMap(Map<String, Object?> map) {
    return UrlTemplateImagerySource(
      id: map['id'] as String? ?? '',
      urlTemplate: map['urlTemplate'] as String? ?? '',
    );
  }
}

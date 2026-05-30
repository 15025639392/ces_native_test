sealed class TerrainSource {
  const TerrainSource({required this.id});

  final String id;

  Map<String, Object?> toMap();

  static TerrainSource? fromMap(Map<Object?, Object?>? data) {
    if (data == null) return null;
    switch (data['type']) {
      case 'quantizedMesh':
        return QuantizedMeshTerrainSource(
          id: data['id'] as String? ?? '',
          layerJsonUrl: data['layerJsonUrl'] as String? ?? '',
        );
      default:
        return null;
    }
  }
}

class QuantizedMeshTerrainSource extends TerrainSource {
  const QuantizedMeshTerrainSource({
    required super.id,
    required this.layerJsonUrl,
  });

  final String layerJsonUrl;

  @override
  Map<String, Object?> toMap() {
    return {
      'type': 'quantizedMesh',
      'id': id,
      'layerJsonUrl': layerJsonUrl,
    };
  }
}

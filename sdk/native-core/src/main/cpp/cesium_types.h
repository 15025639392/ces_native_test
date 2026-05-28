#pragma once

#include <GLES3/gl3.h>
#include <jni.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace cesium_poc {

constexpr double kPi = 3.14159265358979323846264338327950288;

struct CameraState {
    double longitudeDegrees = 104.0;
    double latitudeDegrees = 35.0;
    double altitudeMeters = 3535534.0;
    bool autoOrbit = false;
    double bearingDegrees = 0.0;
    double pitchDegrees = 35.0;
};

struct EcefPosition {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct SelectedTile {
    double west = 0.0;
    double south = 0.0;
    double east = 0.0;
    double north = 0.0;
    uint32_t level = 0;
    uint32_t x = 0;
    uint32_t y = 0;
};

struct GpuTexture {
    GLuint id = 0;
    size_t bytes = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct RasterAttachment {
    std::shared_ptr<GpuTexture> textureResource;
    int32_t overlayTextureCoordinateID = -1;
    glm::dvec2 translation = glm::dvec2(0.0);
    glm::dvec2 scale = glm::dvec2(1.0);
};

struct OverlayVertexBuffer {
    int32_t overlayTextureCoordinateID = -1;
    GLuint vertexBuffer = 0;
    std::vector<float> cpuVertexData;
};

struct GpuPrimitive {
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    bool buffersUploaded = false;
    glm::dvec3 originEcef = glm::dvec3(0.0);
    std::vector<float> cpuVertexData;
    std::vector<float> baseCpuVertexData;
    std::vector<uint32_t> cpuIndexData;
    std::vector<OverlayVertexBuffer> overlayVertexBuffers;
    std::vector<RasterAttachment> rasterAttachments;
    GLsizei indexCount = 0;
    GLenum indexType = GL_UNSIGNED_SHORT;
};

enum class RenderResourceKind : uint32_t {
    MainThreadGpu = 0x47505552u,
};

struct RenderResourceHeader {
    RenderResourceKind kind;
};

struct GpuTileResources {
    RenderResourceHeader header{RenderResourceKind::MainThreadGpu};
    std::vector<GpuPrimitive> primitives;
    size_t bytes = 0;
    bool queuedForUpload = false;
    uint64_t uploadPriority = 0;
};

struct ProgramLocations {
    GLint projection = -1;
    GLint originEye = -1;
    GLint right = -1;
    GLint up = -1;
    GLint backward = -1;
    GLint texture = -1;
    GLint uvTranslation = -1;
    GLint uvScale = -1;
    GLint discardOutsideUv = -1;
};

} // namespace cesium_poc

#pragma once

#include <GLES3/gl3.h>
#include <jni.h>

#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace cesium_poc {

constexpr double kPi = 3.14159265358979323846264338327950288;

inline float levelColor(uint32_t level, int channel) {
    const float t = static_cast<float>((level * (channel == 0 ? 37 : channel == 1 ? 53 : 71)) % 255) / 255.0f;
    return 0.35f + t * 0.55f;
}

struct CameraState {
    double longitudeDegrees = 116.397389;
    double latitudeDegrees = 39.908722;
    double zoom = 15.0;
    bool autoOrbit = false;
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

struct ImageRgba {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

struct GpuTexture {
    GLuint id = 0;
    size_t bytes = 0;
};

struct GpuPrimitive {
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint texture = 0;
    std::shared_ptr<GpuTexture> textureResource;
    glm::dvec3 originEcef = glm::dvec3(0.0);
    std::vector<float> cpuVertexData;
    std::vector<uint32_t> cpuIndexData;
    std::shared_ptr<const ImageRgba> image;
    GLsizei indexCount = 0;
    GLenum indexType = GL_UNSIGNED_SHORT;
};

struct ImageryTileResource {
    std::shared_ptr<const ImageRgba> image;
    uint32_t z = 0;
    uint32_t x = 0;
    uint32_t y = 0;
};

enum class RenderResourceKind : uint32_t {
    LoadThread = 0x4c4f4144u,
    MainThreadGpu = 0x47505552u,
};

struct RenderResourceHeader {
    RenderResourceKind kind;
};

struct LoadTileResources {
    RenderResourceHeader header{RenderResourceKind::LoadThread};
    std::vector<ImageryTileResource> imageryTiles;
};

struct GpuTileResources {
    RenderResourceHeader header{RenderResourceKind::MainThreadGpu};
    std::vector<GpuPrimitive> primitives;
    size_t bytes = 0;
};

struct ProgramLocations {
    GLint projection = -1;
    GLint originEye = -1;
    GLint right = -1;
    GLint up = -1;
    GLint backward = -1;
    GLint texture = -1;
};

struct BatchResources {
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint textureArray = 0;
    glm::dvec3 originEcef = glm::dvec3(0.0);
    GLsizei indexCount = 0;
    size_t primitiveCount = 0;
    size_t textureLayers = 0;
    bool valid = false;
};

} // namespace cesium_poc

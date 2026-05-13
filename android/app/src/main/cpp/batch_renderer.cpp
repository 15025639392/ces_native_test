#include "batch_renderer.h"

#include <GLES3/gl3.h>

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace cesium_poc {
namespace {

bool isBatchablePrimitive(const GpuPrimitive& primitive) {
    return primitive.image &&
        primitive.image->width == 256 &&
        primitive.image->height == 256 &&
        !primitive.image->pixels.empty() &&
        !primitive.cpuVertexData.empty() &&
        !primitive.cpuIndexData.empty();
}

} // namespace

void freeBatchResources(
    BatchResources& batch,
    std::unordered_set<const GpuPrimitive*>& batchedPrimitives) {
    if (batch.vertexBuffer != 0) glDeleteBuffers(1, &batch.vertexBuffer);
    if (batch.indexBuffer != 0) glDeleteBuffers(1, &batch.indexBuffer);
    if (batch.textureArray != 0) glDeleteTextures(1, &batch.textureArray);
    batch = BatchResources();
    batchedPrimitives.clear();
}

void rebuildSelectedBatchResources(
    const std::vector<const GpuTileResources*>& selectedResources,
    GLuint batchProgram,
    BatchResources& batch,
    std::unordered_set<const GpuPrimitive*>& batchedPrimitives) {
    freeBatchResources(batch, batchedPrimitives);
    if (batchProgram == 0) return;

    GLint maxLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    if (maxLayers <= 0) return;

    std::vector<const GpuPrimitive*> primitives;
    primitives.reserve(selectedResources.size());
    for (const GpuTileResources* resources : selectedResources) {
        if (!resources) continue;
        for (const GpuPrimitive& primitive : resources->primitives) {
            if (isBatchablePrimitive(primitive)) {
                primitives.push_back(&primitive);
                if (static_cast<GLint>(primitives.size()) >= maxLayers) break;
            }
        }
        if (static_cast<GLint>(primitives.size()) >= maxLayers) break;
    }
    if (primitives.empty()) return;

    batch.originEcef = primitives.front()->originEcef;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    for (size_t layer = 0; layer < primitives.size(); ++layer) {
        const GpuPrimitive& primitive = *primitives[layer];
        const uint32_t baseVertex = static_cast<uint32_t>(vertices.size() / 6);
        const glm::dvec3 primitiveOffset = primitive.originEcef - batch.originEcef;

        for (size_t i = 0; i + 4 < primitive.cpuVertexData.size(); i += 5) {
            vertices.push_back(static_cast<float>(static_cast<double>(primitive.cpuVertexData[i]) + primitiveOffset.x));
            vertices.push_back(static_cast<float>(static_cast<double>(primitive.cpuVertexData[i + 1]) + primitiveOffset.y));
            vertices.push_back(static_cast<float>(static_cast<double>(primitive.cpuVertexData[i + 2]) + primitiveOffset.z));
            vertices.push_back(primitive.cpuVertexData[i + 3]);
            vertices.push_back(primitive.cpuVertexData[i + 4]);
            vertices.push_back(static_cast<float>(layer));
        }

        for (uint32_t index : primitive.cpuIndexData) {
            indices.push_back(baseVertex + index);
        }
        batchedPrimitives.insert(&primitive);
    }
    if (vertices.empty() || indices.empty()) {
        freeBatchResources(batch, batchedPrimitives);
        return;
    }

    glGenTextures(1, &batch.textureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, batch.textureArray);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        GL_RGBA,
        256,
        256,
        static_cast<GLsizei>(primitives.size()),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);
    for (size_t layer = 0; layer < primitives.size(); ++layer) {
        const ImageRgba& image = *primitives[layer]->image;
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            0,
            0,
            static_cast<GLint>(layer),
            256,
            256,
            1,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            image.pixels.data());
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenBuffers(1, &batch.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &batch.indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.indexBuffer);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    batch.indexCount = static_cast<GLsizei>(indices.size());
    batch.primitiveCount = primitives.size();
    batch.textureLayers = primitives.size();
    batch.valid = true;
}

} // namespace cesium_poc

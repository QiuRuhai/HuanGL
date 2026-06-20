#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>

namespace HuanGL {

class Texture;
class VertexArray;
class Buffer;

// Per-vertex data layout. Must match the attribute bindings set up in BuildMesh.
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
};

// CPU-side copy of mesh geometry, retained for systems that need triangles on
// the host (e.g. BVH construction for the reference path tracer). The realtime
// renderer never reads this; it draws from the GL buffers below. Null when a
// mesh was uploaded without retaining a host copy.
struct CpuGeometry {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
};

struct SubMesh {
    uint32_t indexOffset   = 0;
    uint32_t indexCount    = 0;
    uint32_t materialIndex = 0;
};

struct Mesh {
    std::shared_ptr<VertexArray> vao;
    std::shared_ptr<Buffer>      vbo;
    std::shared_ptr<Buffer>      ebo;
    std::vector<SubMesh>         subMeshes;
    std::shared_ptr<CpuGeometry> cpuGeometry; // optional host-side triangles
};

struct Material {
    std::shared_ptr<Texture> albedoMap;
    std::shared_ptr<Texture> normalMap;
    std::shared_ptr<Texture> roughnessMap;
    std::shared_ptr<Texture> metallicMap;
    glm::vec4 baseColorFactor = {1, 1, 1, 1};
    float     roughnessFactor = 1.0f;
    float     metallicFactor  = 0.0f;
    bool      packedMetallicRoughness = false; // glTF: G=roughness, B=metallic in one texture
};

struct DirectionalLight {
    glm::vec3 direction = {0.2f, -1.0f, -0.3f};
    glm::vec3 color     = {1.0f, 0.98f, 0.95f};
    float     intensity = 5.0f;
};

} // namespace HuanGL

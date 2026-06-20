#pragma once
#include "Bvh.h"
#include "../renderer/Buffer.h"
#include "../renderer/RenderSceneView.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>

namespace HuanGL {

// std430 layout; mirrored exactly in pathtrace.comp. vec4 everywhere to dodge
// the vec3 16-byte alignment trap.
struct GpuTri {
    glm::vec4  p0, p1, p2;  // .xyz position, .w unused
    glm::vec4  n0, n1, n2;  // .xyz normal,   .w unused
    glm::uvec4 mat;          // .x = material index, .yzw pad
};                           // 7*16 = 112 bytes
static_assert(sizeof(GpuTri) == 112, "GpuTri must be 112 bytes (std430)");

struct GpuNode {
    glm::vec4  aabbMin;  // .xyz min, .w unused
    glm::vec4  aabbMax;  // .xyz max, .w unused
    glm::ivec4 meta;     // .x = leftFirst, .y = triCount, .zw pad
};                       // 3*16 = 48 bytes
static_assert(sizeof(GpuNode) == 48, "GpuNode must be 48 bytes (std430)");

struct GpuMaterial {
    glm::vec4 baseColor;  // .rgb albedo, .a unused
    glm::vec4 mr;         // .x metallic, .y roughness, .zw pad
};                        // 2*16 = 32 bytes
static_assert(sizeof(GpuMaterial) == 32, "GpuMaterial must be 32 bytes (std430)");

class PathTracerScene {
public:
    // Gather world-space triangles, build BVH, pack GPU structs, upload SSBOs.
    void Build(const RenderSceneView& scene);

    // False when Build found no renderable with CPU geometry.
    bool Ready() const { return ready_; }

    // Bind nodes/tris/materials SSBOs to their fixed binding points.
    void BindSSBOs() const;

    uint32_t TriangleCount() const { return triCount_; }

    // SSBO binding points shared with pathtrace.comp.
    // 0, 1, 2 are taken by Camera/Lights/Time UBOs.
    static constexpr GLuint kNodeBinding = 3;
    static constexpr GLuint kTriBinding  = 4;
    static constexpr GLuint kMatBinding  = 5;

private:
    Bvh bvh_;

    std::unique_ptr<Buffer> nodeBuffer_;
    std::unique_ptr<Buffer> triBuffer_;
    std::unique_ptr<Buffer> matBuffer_;

    bool     ready_    = false;
    uint32_t triCount_ = 0;
};

} // namespace HuanGL

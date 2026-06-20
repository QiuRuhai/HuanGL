#include "PathTracerScene.h"
#include "../renderer/Schema.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <unordered_map>
#include <vector>
#include <cstdio>

namespace HuanGL {

void PathTracerScene::Build(const RenderSceneView& scene) {
    // Reset state from any previous build.
    ready_    = false;
    triCount_ = 0;

    // -----------------------------------------------------------------------
    // Step 1: Build the global material array, deduplicated by pointer.
    //
    // Every Renderable in a World shares the same materials pointer
    // (World::BuildRenderSceneView sets renderable.materials = &materials_ for
    // all), so naive per-renderable concatenation would duplicate the same list
    // once per entity.  We keep one entry per unique pointer and record the
    // base offset into gpuMaterials for each list.
    // -----------------------------------------------------------------------
    std::vector<GpuMaterial> gpuMaterials;
    std::unordered_map<const std::vector<Material>*, uint32_t> matBase;

    for (const Renderable& r : scene.renderables) {
        if (!r.materials) continue;
        if (matBase.count(r.materials)) continue; // already recorded

        uint32_t base = static_cast<uint32_t>(gpuMaterials.size());
        matBase[r.materials] = base;

        for (const Material& m : *r.materials) {
            GpuMaterial gm{};
            gm.baseColor = m.baseColorFactor;          // .rgb albedo, .a unused
            gm.mr        = glm::vec4(m.metallicFactor, // .x metallic
                                     m.roughnessFactor, // .y roughness
                                     0.0f, 0.0f);       // .zw pad
            gpuMaterials.push_back(gm);
        }
    }

    // -----------------------------------------------------------------------
    // Step 2: Gather world-space triangles from every renderable that has CPU
    // geometry.  Missing cpuGeometry is normal for GPU-only meshes — skip
    // silently.
    // -----------------------------------------------------------------------
    std::vector<BvhTri> tris;

    for (const Renderable& r : scene.renderables) {
        if (!r.mesh || !r.mesh->cpuGeometry) continue;

        const CpuGeometry& geo = *r.mesh->cpuGeometry;

        // Inverse-transpose of the model matrix transforms normals correctly
        // under non-uniform scale; same reason as the GBuffer normal matrix.
        const glm::mat3 normalMat =
            glm::mat3(glm::transpose(glm::inverse(r.modelMatrix)));

        uint32_t baseIdx = matBase.count(r.materials) ? matBase.at(r.materials) : 0u;

        for (const SubMesh& sm : r.mesh->subMeshes) {
            // Walk index triples over [indexOffset, indexOffset + indexCount).
            for (uint32_t i = sm.indexOffset; i < sm.indexOffset + sm.indexCount; i += 3) {
                uint32_t i0 = geo.indices[i + 0];
                uint32_t i1 = geo.indices[i + 1];
                uint32_t i2 = geo.indices[i + 2];

                const Vertex& v0 = geo.vertices[i0];
                const Vertex& v1 = geo.vertices[i1];
                const Vertex& v2 = geo.vertices[i2];

                BvhTri tri{};
                tri.p0 = glm::vec3(r.modelMatrix * glm::vec4(v0.position, 1.0f));
                tri.p1 = glm::vec3(r.modelMatrix * glm::vec4(v1.position, 1.0f));
                tri.p2 = glm::vec3(r.modelMatrix * glm::vec4(v2.position, 1.0f));

                tri.n0 = glm::normalize(normalMat * v0.normal);
                tri.n1 = glm::normalize(normalMat * v1.normal);
                tri.n2 = glm::normalize(normalMat * v2.normal);

                tri.materialIndex = baseIdx + sm.materialIndex;

                tris.push_back(tri);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Step 3: Guard — nothing to build if no CPU geometry was found.
    // -----------------------------------------------------------------------
    if (tris.empty()) {
        std::printf("PathTracerScene: active scene has no CPU geometry; "
                    "reference unavailable\n");
        return;
    }

    triCount_ = static_cast<uint32_t>(tris.size());

    // -----------------------------------------------------------------------
    // Step 4: Build the BVH over the gathered triangles.
    // -----------------------------------------------------------------------
    bvh_.Build(std::move(tris));

    // -----------------------------------------------------------------------
    // Step 5: Pack BVH nodes and reordered triangles into GPU structs.
    // -----------------------------------------------------------------------
    const auto& bvhNodes = bvh_.Nodes();
    const auto& bvhTris  = bvh_.Tris();

    std::vector<GpuNode> gpuNodes;
    gpuNodes.reserve(bvhNodes.size());
    for (const BvhNode& n : bvhNodes) {
        GpuNode gn{};
        gn.aabbMin = glm::vec4(n.aabbMin, 0.0f);
        gn.aabbMax = glm::vec4(n.aabbMax, 0.0f);
        gn.meta    = glm::ivec4(n.leftFirst, n.triCount, 0, 0);
        gpuNodes.push_back(gn);
    }

    std::vector<GpuTri> gpuTris;
    gpuTris.reserve(bvhTris.size());
    for (const BvhTri& t : bvhTris) {
        GpuTri gt{};
        gt.p0  = glm::vec4(t.p0, 0.0f);
        gt.p1  = glm::vec4(t.p1, 0.0f);
        gt.p2  = glm::vec4(t.p2, 0.0f);
        gt.n0  = glm::vec4(t.n0, 0.0f);
        gt.n1  = glm::vec4(t.n1, 0.0f);
        gt.n2  = glm::vec4(t.n2, 0.0f);
        gt.mat = glm::uvec4(t.materialIndex, 0u, 0u, 0u);
        gpuTris.push_back(gt);
    }

    // -----------------------------------------------------------------------
    // Step 6: Upload to SSBOs.
    // -----------------------------------------------------------------------
    nodeBuffer_ = std::make_unique<Buffer>(GL_SHADER_STORAGE_BUFFER);
    nodeBuffer_->Upload(gpuNodes.data(), gpuNodes.size() * sizeof(GpuNode));

    triBuffer_ = std::make_unique<Buffer>(GL_SHADER_STORAGE_BUFFER);
    triBuffer_->Upload(gpuTris.data(), gpuTris.size() * sizeof(GpuTri));

    matBuffer_ = std::make_unique<Buffer>(GL_SHADER_STORAGE_BUFFER);
    matBuffer_->Upload(gpuMaterials.data(), gpuMaterials.size() * sizeof(GpuMaterial));

    ready_ = true;

    // Temporary verification hook — removed in Task 9 cleanup.
    std::printf("PathTracerScene: %zu nodes, %u tris, %zu materials\n",
                gpuNodes.size(), TriangleCount(), gpuMaterials.size());
}

void PathTracerScene::BindSSBOs() const {
    if (!ready_) return;
    nodeBuffer_->BindBase(kNodeBinding);
    triBuffer_->BindBase(kTriBinding);
    matBuffer_->BindBase(kMatBinding);
}

} // namespace HuanGL

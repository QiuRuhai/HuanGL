#include "ModelScene.h"
#include "../resource/MeshLoader.h"
#include "../renderer/Buffer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <utility>

namespace HuanGL {

static std::shared_ptr<Mesh> BuildFloor(uint32_t materialIndex, float halfSize = 10.0f) {
    auto m = std::make_shared<Mesh>();
    m->vao = std::make_shared<VertexArray>();
    m->vbo = std::make_shared<Buffer>(GL_ARRAY_BUFFER);
    m->ebo = std::make_shared<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    float h = halfSize;
    std::vector<Vertex> verts = {
        {{-h, 0, -h}, {0, 1, 0}, {0, 0}, {1, 0, 0}},
        {{ h, 0, -h}, {0, 1, 0}, {1, 0}, {1, 0, 0}},
        {{ h, 0,  h}, {0, 1, 0}, {1, 1}, {1, 0, 0}},
        {{-h, 0,  h}, {0, 1, 0}, {0, 1}, {1, 0, 0}},
    };
    std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};

    m->vbo->Upload(verts.data(), verts.size() * sizeof(Vertex));
    m->ebo->Upload(idx.data(), idx.size() * sizeof(uint32_t));

    constexpr GLsizei s = sizeof(Vertex);
    m->vao->Bind();
    m->vbo->Bind();
    m->ebo->Bind();
    m->vao->BindVertexBuffer(0, m->vbo->GetID(), s, 0);
    m->vao->AddAttribute(0, 3, GL_FLOAT, GL_FALSE, s, offsetof(Vertex, position));
    m->vao->AddAttribute(1, 3, GL_FLOAT, GL_FALSE, s, offsetof(Vertex, normal));
    m->vao->AddAttribute(2, 2, GL_FLOAT, GL_FALSE, s, offsetof(Vertex, texCoord));
    m->vao->AddAttribute(3, 3, GL_FLOAT, GL_FALSE, s, offsetof(Vertex, tangent));
    m->vao->Unbind();

    SubMesh sub;
    sub.indexCount    = (uint32_t)idx.size();
    sub.materialIndex = materialIndex;
    m->subMeshes.push_back(sub);
    return m;
}

ModelScene::ModelScene(std::string modelPath, std::string sceneName,
                       bool addFloor, float modelScale)
    : modelPath_(std::move(modelPath))
    , sceneName_(std::move(sceneName))
    , addFloor_(addFloor)
    , modelScale_(modelScale) {}

void ModelScene::Init(ResourceManager& /*rm*/) {
    world_.Clear();

    // Load model (mesh + materials). Throws if file missing.
    LoadResult loaded = MeshLoader::Load(modelPath_);

    // Append loaded materials. SubMesh.materialIndex from MeshLoader is
    // already local-relative (0..N), and since we add the model BEFORE the
    // floor material, the indices stay valid.
    auto& materials = world_.GetMaterials();
    const uint32_t modelMaterialBase = static_cast<uint32_t>(materials.size());
    for (auto& mat : loaded.materials) {
        materials.push_back(std::move(mat));
    }

    // If the model brought materials, its sub-meshes reference them by local
    // index 0..N-1. Because we appended at the start (modelMaterialBase == 0
    // on a fresh scene), no remap is needed in the common case. Guard anyway
    // so this scene composes if extended later.
    if (modelMaterialBase != 0) {
        for (auto& sub : loaded.mesh->subMeshes) {
            sub.materialIndex += modelMaterialBase;
        }
    }

    // Add the loaded model with a uniform scale and Y-up orientation.
    auto& model = world_.CreateEntity(sceneName_);
    model.transform.scale = {modelScale_, modelScale_, modelScale_};
    model.meshRenderer = MeshRenderer { loaded.mesh };

    // Optional: add a simple plaster floor with its own material slot.
    if (addFloor_) {
        Material floorMat;
        floorMat.baseColorFactor = {0.7f, 0.7f, 0.7f, 1.0f};
        floorMat.roughnessFactor = 0.9f;
        floorMat.metallicFactor  = 0.0f;
        uint32_t floorMatIdx = static_cast<uint32_t>(materials.size());
        materials.push_back(std::move(floorMat));

        // Floor sits at y = -1 to give camera headroom; scale model-relative.
        auto& floor = world_.CreateEntity("Floor");
        floor.transform.translation = {0.0f, -1.0f, 0.0f};
        floor.meshRenderer = MeshRenderer { BuildFloor(floorMatIdx, 10.0f) };
    }

    // Sun light: angled so the model casts a visible shadow on the floor.
    auto& sun = world_.GetSunLight();
    sun.direction = glm::normalize(glm::vec3(0.4f, -1.0f, -0.3f));
    sun.color     = {1.0f, 0.95f, 0.85f};
    sun.intensity = 6.0f;

    SyncPtrs();
}

} // namespace HuanGL

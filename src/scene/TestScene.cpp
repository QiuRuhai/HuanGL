#include "TestScene.h"
#include "../resource/ResourceManager.h"
#include "../renderer/Buffer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace HuanGL {

static std::vector<Vertex> GenSphere(int stacks, int slices, float r) {
    std::vector<Vertex> v;
    for (int i = 0; i <= stacks; ++i) {
        float phi = glm::pi<float>() * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float th = 2.f * glm::pi<float>() * j / slices;
            Vertex vt;
            vt.position = {r * sin(phi) * cos(th), r * cos(phi), r * sin(phi) * sin(th)};
            vt.normal   = glm::normalize(glm::vec3(vt.position));
            vt.texCoord = {float(j) / slices, float(i) / stacks};
            vt.tangent  = {0, 0, 0};
            v.push_back(vt);
        }
    }
    return v;
}

static std::vector<uint32_t> GenSphereIdx(int stacks, int slices) {
    std::vector<uint32_t> idx;
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            uint32_t a = i * (slices + 1) + j, b = a + slices + 1;
            idx.insert(idx.end(), {a, b, a + 1, b, b + 1, a + 1});
        }
    return idx;
}

static std::shared_ptr<Mesh> BuildMesh(const std::vector<Vertex>& verts,
                                        const std::vector<uint32_t>& idx,
                                        uint32_t matIdx) {
    auto m = std::make_shared<Mesh>();
    m->vao = std::make_shared<VertexArray>();
    m->vbo = std::make_shared<Buffer>(GL_ARRAY_BUFFER);
    m->ebo = std::make_shared<Buffer>(GL_ELEMENT_ARRAY_BUFFER);
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
    sub.indexCount = (uint32_t)idx.size();
    sub.materialIndex = matIdx;
    m->subMeshes.push_back(sub);
    return m;
}

void TestScene::Init(ResourceManager& /*rm*/) {
    // Materials: 0=rusty iron, 1=copper, 2=gold, 3=plaster
    materials_.push_back({{},{},{},{}, {0.56f,0.36f,0.25f,1}, 0.8f, 0.0f});
    materials_.push_back({{},{},{},{}, {0.95f,0.64f,0.54f,1}, 0.4f, 1.0f});
    materials_.push_back({{},{},{},{}, {1.0f,0.84f,0.0f,1},  0.1f, 1.0f});
    materials_.push_back({{},{},{},{}, {0.9f,0.9f,0.9f,1},   0.95f,0.0f});

    // Floor
    float h = 10.f;
    std::vector<Vertex> pv = {
        {{-h,0,-h},{0,1,0},{0,0},{1,0,0}}, {{ h,0,-h},{0,1,0},{1,0},{1,0,0}},
        {{ h,0, h},{0,1,0},{1,1},{1,0,0}}, {{-h,0, h},{0,1,0},{0,1},{1,0,0}},
    };
    meshesOwned_.push_back(BuildMesh(pv, {0,1,2, 0,2,3}, 3));
    modelMatrices_.push_back(glm::mat4(1));

    // Spheres
    auto sv = GenSphere(32, 64, 1.0f);
    auto si = GenSphereIdx(32, 64);
    meshesOwned_.push_back(BuildMesh(sv, si, 0));
    modelMatrices_.push_back(glm::translate(glm::mat4(1), {-3, 2, 0}));
    meshesOwned_.push_back(BuildMesh(sv, si, 1));
    modelMatrices_.push_back(glm::translate(glm::mat4(1), { 0, 2, 0}));
    meshesOwned_.push_back(BuildMesh(sv, si, 2));
    modelMatrices_.push_back(glm::translate(glm::mat4(1), { 3, 2, 0}));

    // Boxes
    auto cv = GenSphere(4, 4, 0.6f);
    auto ci = GenSphereIdx(4, 4);
    meshesOwned_.push_back(BuildMesh(cv, ci, 3));
    modelMatrices_.push_back(glm::translate(glm::mat4(1), {-5, 1.5f, 3}));
    meshesOwned_.push_back(BuildMesh(cv, ci, 3));
    modelMatrices_.push_back(glm::translate(glm::mat4(1), { 5, 1.5f, 3}));

    sunLight_.direction = glm::normalize(glm::vec3(0.4f, -1.0f, -0.3f));
    sunLight_.color     = {1.0f, 0.95f, 0.85f};
    sunLight_.intensity = 8.0f;

    SyncPtrs();
}

} // namespace HuanGL

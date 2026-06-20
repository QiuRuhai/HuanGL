#include "TestScene.h"
#include "PrimitiveMesh.h"
#include "../resource/ResourceManager.h"
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

void TestScene::Init(ResourceManager& /*rm*/) {
    world_.Clear();

    // Materials: 0=rusty iron, 1=copper, 2=gold, 3=plaster
    auto& materials = world_.GetMaterials();
    materials.push_back({{},{},{},{}, {0.56f,0.36f,0.25f,1}, 0.8f, 0.0f});
    materials.push_back({{},{},{},{}, {0.95f,0.64f,0.54f,1}, 0.4f, 1.0f});
    materials.push_back({{},{},{},{}, {1.0f,0.84f,0.0f,1},  0.1f, 1.0f});
    materials.push_back({{},{},{},{}, {0.9f,0.9f,0.9f,1},   0.95f,0.0f});

    // Floor
    float h = 10.f;
    std::vector<Vertex> pv = {
        {{-h,0,-h},{0,1,0},{0,0},{1,0,0}}, {{ h,0,-h},{0,1,0},{1,0},{1,0,0}},
        {{ h,0, h},{0,1,0},{1,1},{1,0,0}}, {{-h,0, h},{0,1,0},{0,1},{1,0,0}},
    };
    auto& floor = world_.CreateEntity("Floor");
    floor.meshRenderer = MeshRenderer { BuildMesh(pv, {0,1,2, 0,2,3}, 3) };

    // Spheres
    auto sv = GenSphere(32, 64, 1.0f);
    auto si = GenSphereIdx(32, 64);
    auto& ironSphere = world_.CreateEntity("Rusty Iron Sphere");
    ironSphere.transform.translation = {-3.0f, 2.0f, 0.0f};
    ironSphere.meshRenderer = MeshRenderer { BuildMesh(sv, si, 0) };
    auto& copperSphere = world_.CreateEntity("Copper Sphere");
    copperSphere.transform.translation = {0.0f, 2.0f, 0.0f};
    copperSphere.meshRenderer = MeshRenderer { BuildMesh(sv, si, 1) };
    auto& goldSphere = world_.CreateEntity("Gold Sphere");
    goldSphere.transform.translation = {3.0f, 2.0f, 0.0f};
    goldSphere.meshRenderer = MeshRenderer { BuildMesh(sv, si, 2) };

    // Boxes
    auto cv = GenSphere(4, 4, 0.6f);
    auto ci = GenSphereIdx(4, 4);
    auto& leftBox = world_.CreateEntity("Left Low-poly Sphere");
    leftBox.transform.translation = {-5.0f, 1.5f, 3.0f};
    leftBox.meshRenderer = MeshRenderer { BuildMesh(cv, ci, 3) };
    auto& rightBox = world_.CreateEntity("Right Low-poly Sphere");
    rightBox.transform.translation = {5.0f, 1.5f, 3.0f};
    rightBox.meshRenderer = MeshRenderer { BuildMesh(cv, ci, 3) };

    auto& sun = world_.GetSunLight();
    sun.direction = glm::normalize(glm::vec3(0.4f, -1.0f, -0.3f));
    sun.color     = {1.0f, 0.95f, 0.85f};
    sun.intensity = 8.0f;

}

} // namespace HuanGL

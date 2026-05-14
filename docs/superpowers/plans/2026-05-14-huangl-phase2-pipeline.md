# HuanGL Phase 2: Render Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the deferred PBR rendering pipeline: GBuffer fill, cascaded shadow maps with PCSS soft shadows, Cook-Torrance PBR with IBL, and a resource manager for texture/mesh caching.

**Architecture:** ResourceManager loads/caches assets. Scene provides mesh/material/light data. Three passes execute in order: ShadowPass → GBufferPass → LightingPass. RenderPipeline orchestrates passes; App owns pipeline + scene + UBOs + camera.

**Tech Stack:** C++17, OpenGL 4.6 Core Profile (DSA), GLFW 3.3+, GLAD2, GLM, Assimp 5.x, stb_image

---

## File Map

```
NEW:  src/renderer/Schema.h
NEW:  src/resource/ResourceManager.h/cpp
NEW:  src/resource/MeshLoader.h/cpp
NEW:  src/core/Camera.h
NEW:  src/scene/Scene.h/cpp
NEW:  src/scene/TestScene.h/cpp
NEW:  src/pipeline/passes/GBufferPass.h/cpp
NEW:  src/pipeline/passes/ShadowPass.h/cpp
NEW:  src/pipeline/passes/LightingPass.h/cpp
NEW:  src/pipeline/RenderPipeline.h/cpp
NEW:  shader/gbuffer/gbuffer.vert
NEW:  shader/gbuffer/gbuffer.frag
NEW:  shader/shadow/csm.vert
NEW:  shader/shadow/csm.frag
NEW:  shader/lighting/fullscreen.vert
NEW:  shader/lighting/pbr_ibl.frag
NEW:  shader/lighting/equirect_to_cubemap.frag
NEW:  shader/lighting/irradiance.frag
NEW:  shader/lighting/prefilter.frag
NEW:  shader/lighting/brdf_lut.frag
MOD:  src/core/App.h
MOD:  src/core/App.cpp
```

---

### Task 1: Schema header

**Files:**
- Create: `src/renderer/Schema.h`

- [ ] **Step 1: Create `src/renderer/Schema.h`**

```cpp
#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>

namespace HuanGL {

class Texture;
class VertexArray;
class Buffer;

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
};

struct Material {
    std::shared_ptr<Texture> albedoMap;
    std::shared_ptr<Texture> normalMap;
    std::shared_ptr<Texture> roughnessMap;
    std::shared_ptr<Texture> metallicMap;
    glm::vec4 baseColorFactor = {1, 1, 1, 1};
    float     roughnessFactor = 1.0f;
    float     metallicFactor  = 0.0f;
};

struct DirectionalLight {
    glm::vec3 direction = {0.2f, -1.0f, -0.3f};
    glm::vec3 color     = {1.0f, 0.98f, 0.95f};
    float     intensity = 5.0f;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
};

} // namespace HuanGL
```

- [ ] **Step 2: Build and commit**

```powershell
cmake --build build --config Debug
```

```bash
git add src/renderer/Schema.h && git commit -m "feat: add Mesh/SubMesh/Material/DirectionalLight schema"
```

---

### Task 2: ResourceManager + MeshLoader

**Files:**
- Create: `src/resource/ResourceManager.h`
- Create: `src/resource/ResourceManager.cpp`
- Create: `src/resource/MeshLoader.h`
- Create: `src/resource/MeshLoader.cpp`

- [ ] **Step 1: Create `src/resource/ResourceManager.h`**

```cpp
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <typeindex>

namespace HuanGL {

class ResourceManager {
public:
    static void Init();
    static void Shutdown();

    template<typename T>
    static std::shared_ptr<T> Load(const std::string& path);

    static void GC();

private:
    template<typename T>
    static std::string MakeKey(const std::string& path) {
        return std::string(typeid(T).name()) + "|" + path;
    }
    struct Entry { std::weak_ptr<void> ptr; };
    static std::unordered_map<std::string, Entry> cache_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/resource/ResourceManager.cpp`**

```cpp
#include "ResourceManager.h"
#include "MeshLoader.h"
#include "../renderer/Texture.h"

namespace HuanGL {

std::unordered_map<std::string, ResourceManager::Entry> ResourceManager::cache_;

void ResourceManager::Init() {}
void ResourceManager::Shutdown() { GC(); }

void ResourceManager::GC() {
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->second.ptr.expired()) it = cache_.erase(it);
        else ++it;
    }
}

template<>
std::shared_ptr<Texture> ResourceManager::Load<Texture>(const std::string& path) {
    std::string key = MakeKey<Texture>(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Texture>(p);
    }
    auto tex = Texture::Load2D(path, true);
    cache_[key] = {tex};
    return tex;
}

template<>
std::shared_ptr<Mesh> ResourceManager::Load<Mesh>(const std::string& path) {
    std::string key = MakeKey<Mesh>(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        if (auto p = it->second.ptr.lock())
            return std::static_pointer_cast<Mesh>(p);
    }
    auto mesh = MeshLoader::Load(path);
    cache_[key] = {mesh};
    return mesh;
}

} // namespace HuanGL
```

- [ ] **Step 3: Create `src/resource/MeshLoader.h`**

```cpp
#pragma once
#include <memory>
#include <string>
#include "../renderer/Schema.h"

namespace HuanGL {
class MeshLoader {
public:
    static std::shared_ptr<Mesh> Load(const std::string& path);
};
} // namespace HuanGL
```

- [ ] **Step 4: Create `src/resource/MeshLoader.cpp`**

```cpp
#include "MeshLoader.h"
#include "../renderer/Buffer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdexcept>
#include <vector>

namespace HuanGL {

std::shared_ptr<Mesh> MeshLoader::Load(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* aiscene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_CalcTangentSpace | aiProcess_FlipUVs);

    if (!aiscene || !aiscene->HasMeshes())
        throw std::runtime_error("[MeshLoader] Failed: " + path);

    auto mesh = std::make_shared<Mesh>();
    mesh->vao = std::make_shared<VertexArray>();
    mesh->vbo = std::make_shared<Buffer>(GL_ARRAY_BUFFER);
    mesh->ebo = std::make_shared<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    uint32_t totalVerts = 0, totalIndices = 0;
    for (unsigned i = 0; i < aiscene->mNumMeshes; ++i) {
        totalVerts   += aiscene->mMeshes[i]->mNumVertices;
        totalIndices += aiscene->mMeshes[i]->mNumFaces * 3;
    }

    std::vector<Vertex> vertices(totalVerts);
    std::vector<uint32_t> indices(totalIndices);
    uint32_t vo = 0, io = 0;

    for (unsigned i = 0; i < aiscene->mNumMeshes; ++i) {
        const aiMesh* am = aiscene->mMeshes[i];
        SubMesh sub;
        sub.indexOffset   = io;
        sub.indexCount    = am->mNumFaces * 3;
        sub.materialIndex = am->mMaterialIndex;
        mesh->subMeshes.push_back(sub);

        for (unsigned v = 0; v < am->mNumVertices; ++v) {
            Vertex& vt = vertices[vo + v];
            vt.position = {am->mVertices[v].x, am->mVertices[v].y, am->mVertices[v].z};
            if (am->HasNormals())
                vt.normal = {am->mNormals[v].x, am->mNormals[v].y, am->mNormals[v].z};
            if (am->HasTextureCoords(0))
                vt.texCoord = {am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y};
            if (am->HasTangentsAndBitangents())
                vt.tangent = {am->mTangents[v].x, am->mTangents[v].y, am->mTangents[v].z};
        }
        for (unsigned f = 0; f < am->mNumFaces; ++f)
            for (unsigned j = 0; j < 3; ++j)
                indices[io + f * 3 + j] = vo + am->mFaces[f].mIndices[j];

        vo += am->mNumVertices;
        io += sub.indexCount;
    }

    mesh->vbo->Upload(vertices.data(), vertices.size() * sizeof(Vertex));
    mesh->ebo->Upload(indices.data(), indices.size() * sizeof(uint32_t));

    constexpr GLsizei stride = sizeof(Vertex);
    mesh->vao->Bind();
    mesh->vbo->Bind();
    mesh->ebo->Bind();
    mesh->vao->BindVertexBuffer(0, mesh->vbo->GetID(), stride, 0);
    mesh->vao->AddAttribute(0, 3, GL_FLOAT, GL_FALSE, stride, offsetof(Vertex, position));
    mesh->vao->AddAttribute(1, 3, GL_FLOAT, GL_FALSE, stride, offsetof(Vertex, normal));
    mesh->vao->AddAttribute(2, 2, GL_FLOAT, GL_FALSE, stride, offsetof(Vertex, texCoord));
    mesh->vao->AddAttribute(3, 3, GL_FLOAT, GL_FALSE, stride, offsetof(Vertex, tangent));
    mesh->vao->Unbind();

    return mesh;
}

} // namespace HuanGL
```

- [ ] **Step 5: Build and commit**

```powershell
cmake --build build --config Debug
```

Expected: compiles clean.

```bash
git add src/resource/ && git commit -m "feat: add ResourceManager with Texture/Mesh caching and Assimp MeshLoader"
```

---

### Task 3: Camera + Scene + TestScene

**Files:**
- Create: `src/core/Camera.h`
- Create: `src/scene/Scene.h`
- Create: `src/scene/Scene.cpp`
- Create: `src/scene/TestScene.h`
- Create: `src/scene/TestScene.cpp`

- [ ] **Step 1: Create `src/core/Camera.h`**

```cpp
#pragma once
#include "../renderer/UniformBuffer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

namespace HuanGL {

class Camera {
public:
    Camera(float fovDeg = 60.f, float nearP = 0.1f, float farP = 100.f)
        : fov_(glm::radians(fovDeg)), near_(nearP), far_(farP) {}

    void Update(float dt, GLFWwindow* window, bool capture = true) {
        if (capture) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            float x = (float)mx, y = (float)my;
            if (first_) { lastX_ = x; lastY_ = y; first_ = false; }
            float dx = x - lastX_, dy = lastY_ - y;
            lastX_ = x; lastY_ = y;
            yaw_ += dx * 0.1f; pitch_ += dy * 0.1f;
            pitch_ = glm::clamp(pitch_, -89.f, 89.f);
        }
        front_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_.y = sin(glm::radians(pitch_));
        front_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_ = glm::normalize(front_);
        glm::vec3 right = glm::normalize(glm::cross(front_, worldUp_));
        glm::vec3 up    = glm::cross(right, front_);
        float spd = moveSpeed_ * dt;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) pos_ += front_ * spd;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) pos_ -= front_ * spd;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) pos_ -= right * spd;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) pos_ += right * spd;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) pos_ += up * spd;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) pos_ -= up * spd;
    }

    CameraData GetData(float aspect) const {
        CameraData d;
        d.view     = glm::lookAt(pos_, pos_ + front_, worldUp_);
        d.proj     = glm::perspective(fov_, aspect, near_, far_);
        d.viewProj = d.proj * d.view;
        d.invView  = glm::inverse(d.view);
        d.invProj  = glm::inverse(d.proj);
        d.camPos   = pos_;
        d.near_    = near_;
        d.far_     = far_;
        return d;
    }

    glm::vec3 GetPos() const { return pos_; }

private:
    glm::vec3 pos_ = {0, 3, 10}, front_ = {0, 0, -1}, worldUp_ = {0, 1, 0};
    float yaw_ = -90, pitch_ = 0, fov_, near_, far_, moveSpeed_ = 5;
    float lastX_ = 0, lastY_ = 0;
    bool first_ = true;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/scene/Scene.h`**

```cpp
#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../renderer/Schema.h"

namespace HuanGL {

class ResourceManager;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void Init(ResourceManager& rm) = 0;
    virtual void Update(float dt) { (void)dt; }

    size_t GetMeshCount() const { return meshPtrs_.size(); }
    Mesh*  GetMesh(size_t i) const { return meshPtrs_[i]; }
    glm::mat4 GetModelMatrix(size_t i) const { return modelMatrices_[i]; }

    const std::vector<Material>&    GetMaterials() const { return materials_; }
    const DirectionalLight&         GetSunLight()  const { return sunLight_; }
    const glm::vec3&                GetAmbient()   const { return ambient_; }

protected:
    std::vector<std::shared_ptr<Mesh>> meshesOwned_;
    std::vector<Mesh*>                 meshPtrs_;
    std::vector<glm::mat4>             modelMatrices_;
    std::vector<Material>              materials_;
    DirectionalLight                   sunLight_;
    glm::vec3                          ambient_ = {0.03f, 0.03f, 0.05f};

    void SyncPtrs() {
        meshPtrs_.clear();
        for (auto& m : meshesOwned_) meshPtrs_.push_back(m.get());
    }
};

} // namespace HuanGL
```

- [ ] **Step 3: Create `src/scene/Scene.cpp`**

```cpp
#include "Scene.h"
namespace HuanGL { }
```

- [ ] **Step 4: Create `src/scene/TestScene.h`**

```cpp
#pragma once
#include "Scene.h"

namespace HuanGL {

class TestScene : public Scene {
public:
    void Init(ResourceManager& rm) override;
};

} // namespace HuanGL
```

- [ ] **Step 5: Create `src/scene/TestScene.cpp`**

```cpp
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
    // 0: rusty iron
    materials_.push_back({{},{},{},{}, {0.56f,0.36f,0.25f,1}, 0.8f, 0.0f});
    // 1: copper
    materials_.push_back({{},{},{},{}, {0.95f,0.64f,0.54f,1}, 0.4f, 1.0f});
    // 2: gold
    materials_.push_back({{},{},{},{}, {1.0f,0.84f,0.0f,1},  0.1f, 1.0f});
    // 3: plaster floor
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
    meshesOwned_.push_back(BuildMesh(sv, si, 0)); // iron
    modelMatrices_.push_back(glm::translate(glm::mat4(1), {-3, 2, 0}));
    meshesOwned_.push_back(BuildMesh(sv, si, 1)); // copper
    modelMatrices_.push_back(glm::translate(glm::mat4(1), { 0, 2, 0}));
    meshesOwned_.push_back(BuildMesh(sv, si, 2)); // gold
    modelMatrices_.push_back(glm::translate(glm::mat4(1), { 3, 2, 0}));

    // Box casters (for shadows)
    auto cv = GenSphere(4, 4, 0.6f); // low-poly sphere as "boxes"
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
```

- [ ] **Step 6: Build and commit**

```powershell
cmake --build build --config Debug
```

```bash
git add src/core/Camera.h src/scene/ && git commit -m "feat: add Camera, Scene base, and TestScene with PBR materials"
```

---

### Task 4: GBufferPass + shaders

**Files:**
- Create: `shader/gbuffer/gbuffer.vert`
- Create: `shader/gbuffer/gbuffer.frag`
- Create: `src/pipeline/passes/GBufferPass.h`
- Create: `src/pipeline/passes/GBufferPass.cpp`

- [ ] **Step 1: Create `shader/gbuffer/gbuffer.vert`**

```glsl
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 viewProj;

out vec3 vWorldNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position   = viewProj * worldPos;
    vWorldNormal  = mat3(model) * aNormal;
    vTexCoord     = aTexCoord;
}
```

- [ ] **Step 2: Create `shader/gbuffer/gbuffer.frag`**

```glsl
#version 460 core
in vec3 vWorldNormal;
in vec2 vTexCoord;

layout(location = 0) out vec4 OutAlbedoMetallic;
layout(location = 1) out vec4 OutNormalRoughness;

uniform sampler2D uAlbedoMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uMetallicMap;
uniform vec4  uBaseColor;
uniform float uRoughness;
uniform float uMetallic;
uniform int   uHasAlbedoTex;
uniform int   uHasRoughnessTex;
uniform int   uHasMetallicTex;

void main() {
    vec4 albedo = uHasAlbedoTex > 0 ? texture(uAlbedoMap, vTexCoord) : vec4(1.0);
    albedo *= uBaseColor;

    float roughness = uHasRoughnessTex > 0
        ? texture(uRoughnessMap, vTexCoord).r : uRoughness;
    float metallic  = uHasMetallicTex > 0
        ? texture(uMetallicMap, vTexCoord).r  : uMetallic;

    vec3 N = normalize(vWorldNormal);

    OutAlbedoMetallic   = vec4(albedo.rgb, metallic);
    OutNormalRoughness  = vec4(N, roughness);
}
```

- [ ] **Step 3: Create `src/pipeline/passes/GBufferPass.h`**

```cpp
#pragma once
#include <memory>
#include "../../renderer/Texture.h"
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

class Shader;
class Framebuffer;
class Scene;

class GBufferPass {
public:
    void Init(int width, int height);
    void Resize(int width, int height);
    void Render(const Scene& scene, const CameraData& camera);

    std::shared_ptr<Texture> GetAlbedoMetallic()  const;
    std::shared_ptr<Texture> GetNormalRoughness() const;
    std::shared_ptr<Texture> GetDepth()           const;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<Shader>      shader_;
    int width_ = 0, height_ = 0;
};

} // namespace HuanGL
```

- [ ] **Step 4: Create `src/pipeline/passes/GBufferPass.cpp`**

```cpp
#include "GBufferPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../renderer/Schema.h"
#include "../../scene/Scene.h"
#include <glm/gtc/type_ptr.hpp>

namespace HuanGL {

void GBufferPass::Init(int w, int h) {
    width_ = w; height_ = h;
    shader_ = std::make_unique<Shader>("../shader/gbuffer/gbuffer.vert",
                                       "../shader/gbuffer/gbuffer.frag");
    fbo_ = std::make_unique<Framebuffer>(w, h);

    auto rt0 = Texture::Create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    auto rt1 = Texture::Create2D(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    auto dtx = Texture::Create2D(w, h, GL_DEPTH_COMPONENT24,
                                 GL_DEPTH_COMPONENT, GL_FLOAT);
    fbo_->AttachColor(rt0, 0);
    fbo_->AttachColor(rt1, 1);
    fbo_->AttachDepth(dtx);
    fbo_->SetDrawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
    if (!fbo_->IsComplete())
        throw std::runtime_error("[GBufferPass] FBO incomplete");
}

void GBufferPass::Resize(int w, int h) { Init(w, h); }

void GBufferPass::Render(const Scene& scene, const CameraData& camera) {
    fbo_->Bind();
    Renderer::SetViewport(0, 0, width_, height_);
    Renderer::Clear(true, true, false);
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);
    Renderer::SetDepthFunc(GL_LESS);

    shader_->Use();
    shader_->SetMat4("viewProj", camera.viewProj);

    for (size_t i = 0; i < scene.GetMeshCount(); ++i) {
        Mesh* mesh = scene.GetMesh(i);
        glm::mat4 model = scene.GetModelMatrix(i);
        shader_->SetMat4("model", model);

        mesh->vao->Bind();
        for (auto& sub : mesh->subMeshes) {
            const Material& mat = scene.GetMaterials()[sub.materialIndex];

            shader_->SetInt("uHasAlbedoTex",    mat.albedoMap    ? 1 : 0);
            shader_->SetInt("uHasRoughnessTex", mat.roughnessMap ? 1 : 0);
            shader_->SetInt("uHasMetallicTex",  mat.metallicMap  ? 1 : 0);
            shader_->SetVec4("uBaseColor",  mat.baseColorFactor);
            shader_->SetFloat("uRoughness", mat.roughnessFactor);
            shader_->SetFloat("uMetallic",  mat.metallicFactor);

            if (mat.albedoMap)    mat.albedoMap->Bind(0);
            if (mat.roughnessMap) mat.roughnessMap->Bind(1);
            if (mat.metallicMap)  mat.metallicMap->Bind(2);

            glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                           (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
        }
        mesh->vao->Unbind();
    }
    Framebuffer::BindDefault();
}

std::shared_ptr<Texture> GBufferPass::GetAlbedoMetallic()  const { return fbo_->GetColor(0); }
std::shared_ptr<Texture> GBufferPass::GetNormalRoughness() const { return fbo_->GetColor(1); }
std::shared_ptr<Texture> GBufferPass::GetDepth()           const { return fbo_->GetDepth(); }

} // namespace HuanGL
```

- [ ] **Step 5: Build and commit**

```powershell
cmake --build build --config Debug
```

```bash
git add shader/gbuffer/ src/pipeline/passes/GBufferPass.h src/pipeline/passes/GBufferPass.cpp
git commit -m "feat: add GBufferPass with deferred MRT fill (2 targets + depth)"
```

---

### Task 5: ShadowPass + CSM shaders

**Files:**
- Create: `shader/shadow/csm.vert`
- Create: `shader/shadow/csm.frag`
- Create: `src/pipeline/passes/ShadowPass.h`
- Create: `src/pipeline/passes/ShadowPass.cpp`

- [ ] **Step 1: Create `shader/shadow/csm.vert`**

```glsl
#version 460 core
layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 lightViewProj;

void main() {
    gl_Position = lightViewProj * model * vec4(aPos, 1.0);
}
```

- [ ] **Step 2: Create `shader/shadow/csm.frag`**

```glsl
#version 460 core
// empty — depth writes automatically
void main() {}
```

- [ ] **Step 3: Create `src/pipeline/passes/ShadowPass.h`**

```cpp
#pragma once
#include <memory>
#include <array>
#include <glm/glm.hpp>
#include "../../renderer/Texture.h"
#include "../../renderer/Schema.h"
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

struct CascadeData {
    glm::mat4 viewProj;
    float     farPlane = 0.f;
    float     pad[3]   = {};
};

class ShadowPass {
public:
    void Init(int resolution = 2048);
    ~ShadowPass();
    void Render(const Scene& scene, const CameraData& camera,
                const DirectionalLight& light);

    GLuint GetShadowMapArray() const { return shadowArrayID_; }
    const std::array<CascadeData, 4>& GetCascades() const { return cascades_; }

private:
    std::unique_ptr<Shader>      shader_;
    std::unique_ptr<Framebuffer> fbo_;
    GLuint                       shadowArrayID_ = 0;
    std::array<CascadeData, 4>   cascades_;
    int resolution_ = 2048;
};

} // namespace HuanGL
```

- [ ] **Step 4: Create `src/pipeline/passes/ShadowPass.cpp`**

```cpp
#include "ShadowPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Renderer.h"
#include "../../scene/Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>

namespace HuanGL {

// Compute 4 cascade split depths using logarithmic/linear blend
static std::array<float, 4> ComputeCascadeSplits(float nearP, float farP, float lambda = 0.75f) {
    std::array<float, 4> splits;
    for (int i = 0; i < 4; ++i) {
        float p = (i + 1) / 4.f;
        float logSplit  = nearP * pow(farP / nearP, p);
        float linSplit  = nearP + (farP - nearP) * p;
        splits[i] = glm::mix(linSplit, logSplit, lambda);
    }
    return splits;
}

void ShadowPass::Init(int resolution) {
    resolution_ = resolution;
    shader_ = std::make_unique<Shader>("../shader/shadow/csm.vert",
                                       "../shader/shadow/csm.frag");

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadowArrayID_);
    glTextureStorage3D(shadowArrayID_, 1, GL_DEPTH_COMPONENT24,
                       resolution, resolution, 4);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(shadowArrayID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1,1,1,1};
    glTextureParameterfv(shadowArrayID_, GL_TEXTURE_BORDER_COLOR, border);

    fbo_ = std::make_unique<Framebuffer>(resolution, resolution);
    glNamedFramebufferTexture(fbo_->GetID(), GL_DEPTH_ATTACHMENT,
                              shadowArrayID_, 0);
    glNamedFramebufferDrawBuffer(fbo_->GetID(), GL_NONE);
    glNamedFramebufferReadBuffer(fbo_->GetID(), GL_NONE);
}

ShadowPass::~ShadowPass() {
    if (shadowArrayID_) glDeleteTextures(1, &shadowArrayID_);
}

static glm::mat4 LightViewProj(const DirectionalLight& light,
                                const std::vector<glm::vec3>& frustumCorners) {
    // Compute frustum center in world space
    glm::vec3 center(0);
    for (auto& c : frustumCorners) center += c;
    center /= (float)frustumCorners.size();

    // Light "camera" looks at center from the light direction
    glm::vec3 lightPos = center - light.direction * 50.f;
    glm::mat4 lightView = glm::lookAt(lightPos, center, {0, 1, 0});

    // Transform corners to light space to find AABB
    glm::vec3 mn(1e9f), mx(-1e9f);
    for (auto& c : frustumCorners) {
        glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1));
        mn = glm::min(mn, ls); mx = glm::max(mx, ls);
    }
    // Extend Z range to catch occluders behind the frustum
    mn.z -= 50.f; mx.z += 50.f;

    glm::mat4 lightProj = glm::ortho(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
    return lightProj * lightView;
}

static std::vector<glm::vec3> FrustumCorners(const glm::mat4& invViewProj,
                                               float nearP, float farP) {
    std::vector<glm::vec3> corners(8);
    int idx = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                glm::vec4 ndc(x * 2.f - 1.f, y * 2.f - 1.f,
                              z == 0 ? nearP : (nearP + farP) * 0.5f, 1.f);
                // Actually use the near/far for NDC z:
                // NDC z: -1=near, 1=far in OpenGL
                ndc.z = z == 0 ? -1.f : 1.f;
                glm::vec4 world = invViewProj * ndc;
                corners[idx++] = glm::vec3(world) / world.w;
            }
        }
    }
    return corners;
}

void ShadowPass::Render(const Scene& scene, const CameraData& camera,
                         const DirectionalLight& light) {
    auto splits = ComputeCascadeSplits(camera.near_, camera.far_);
    auto invVP  = glm::inverse(camera.viewProj);

    Renderer::EnableCullFace(true);
    Renderer::SetCullFace(GL_FRONT); // peter panning reduction
    Renderer::EnableDepthTest(true);
    Renderer::EnableDepthWrite(true);

    shader_->Use();

    for (int c = 0; c < 4; ++c) {
        float prevFar = c == 0 ? camera.near_ : splits[c - 1];
        auto corners = FrustumCorners(invVP, prevFar, splits[c]);
        glm::mat4 lightVP = LightViewProj(light, corners);

        cascades_[c].viewProj = lightVP;
        cascades_[c].farPlane = splits[c];

        // Attach layer c of the array
        glNamedFramebufferTextureLayer(fbo_->GetID(), GL_DEPTH_ATTACHMENT,
                                       shadowArrayID_, 0, c);
        fbo_->Bind();
        Renderer::SetViewport(0, 0, resolution_, resolution_);
        Renderer::Clear(false, true, false);

        shader_->SetMat4("lightViewProj", lightVP);

        for (size_t i = 0; i < scene.GetMeshCount(); ++i) {
            shader_->SetMat4("model", scene.GetModelMatrix(i));
            Mesh* mesh = scene.GetMesh(i);
            mesh->vao->Bind();
            for (auto& sub : mesh->subMeshes)
                glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                               (void*)(uintptr_t)(sub.indexOffset * sizeof(uint32_t)));
            mesh->vao->Unbind();
        }
    }

    Renderer::SetCullFace(GL_BACK);
    Framebuffer::BindDefault();
}

} // namespace HuanGL
```

- [ ] **Step 5: Build and commit**

```powershell
cmake --build build --config Debug
```

```bash
git add shader/shadow/ src/pipeline/passes/ShadowPass.h src/pipeline/passes/ShadowPass.cpp
git commit -m "feat: add ShadowPass with 4-cascade CSM depth array"
```

---

### Task 6: LightingPass + IBL shaders

**Files:**
- Create: `shader/lighting/fullscreen.vert`
- Create: `shader/lighting/pbr_ibl.frag`
- Create: `shader/lighting/equirect_to_cubemap.frag`
- Create: `shader/lighting/irradiance.frag`
- Create: `shader/lighting/prefilter.frag`
- Create: `shader/lighting/brdf_lut.frag`
- Create: `src/pipeline/passes/LightingPass.h`
- Create: `src/pipeline/passes/LightingPass.cpp`

- [ ] **Step 1: Create `shader/lighting/fullscreen.vert`**

```glsl
#version 460 core
out vec2 vUV;
void main() {
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    vUV = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
```

- [ ] **Step 2: Create `shader/lighting/equirect_to_cubemap.frag`**

```glsl
#version 460 core
in vec3 vWorldPos; // cubemap sampling direction
out vec4 FragColor;
uniform sampler2D uEquirect;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 dir) {
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv *= invAtan; uv += 0.5; return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(vWorldPos));
    FragColor = vec4(texture(uEquirect, uv).rgb, 1.0);
}
```

- [ ] **Step 3: Create `shader/lighting/irradiance.frag`**

```glsl
#version 460 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform samplerCube uEnvMap;

void main() {
    vec3 N = normalize(vWorldPos);
    vec3 up = abs(N.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 right = normalize(cross(up, N));
    vec3 up2   = cross(N, right);

    vec3 color = vec3(0);
    float samples = 0;
    float step = 0.05;
    for (float phi = 0; phi < 6.2832; phi += step) {
        for (float theta = 0; theta < 1.5708; theta += step) {
            vec3 tangent = cos(phi) * right + sin(phi) * up2;
            vec3 sampleDir = cos(theta) * N + sin(theta) * tangent;
            color += texture(uEnvMap, sampleDir).rgb * cos(theta) * sin(theta);
            samples++;
        }
    }
    FragColor = vec4(3.14159 * color / samples, 1.0);
}
```

- [ ] **Step 4: Create `shader/lighting/prefilter.frag`**

```glsl
#version 460 core
in vec3 vWorldPos;
out vec4 FragColor;
uniform samplerCube uEnvMap;
uniform float uRoughness;

const uint SAMPLE_COUNT = 1024u;
const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return tangent * H.x + bitangent * H.y + N * H.z;
}

void main() {
    vec3 N = normalize(vWorldPos);
    vec3 R = N;
    vec3 V = R;

    float totalWeight = 0;
    vec3 prefiltered = vec3(0);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefiltered += texture(uEnvMap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    FragColor = vec4(prefiltered / max(totalWeight, 0.001), 1.0);
}
```

- [ ] **Step 5: Create `shader/lighting/brdf_lut.frag`**

```glsl
#version 460 core
in vec2 vUV;
out vec2 FragColor;

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N) { return vec2(float(i)/float(N), RadicalInverse_VdC(i)); }
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 t = normalize(cross(up, N)), b = cross(N, t);
    return t * H.x + b * H.y + N * H.z;
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness, k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}
vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float A = 0, B = 0;
    vec3 N = vec3(0, 0, 1);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.z, 0.0), NdotH = max(H.z, 0.0), VdotH = max(dot(V, H), 0.0);
        if (NdotL > 0.0) {
            float G = GeometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.001);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2(A, B) / float(SAMPLE_COUNT);
}
void main() {
    FragColor = IntegrateBRDF(vUV.x, vUV.y);
}
```

- [ ] **Step 6: Create `shader/lighting/pbr_ibl.frag`**

```glsl
#version 460 core
in vec2 vUV;
out vec4 FragColor;

// GBuffer inputs
uniform sampler2D uAlbedoMetallic;
uniform sampler2D uNormalRoughness;
uniform sampler2D uDepth;

// Shadow
uniform sampler2DArrayShadow uShadowMap;
uniform mat4 uCascadeViewProj[4];
uniform float uCascadeFar[4];
uniform vec3 uLightDir;
uniform vec3 uLightColor;

// IBL
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D uBRDFLUT;

// Camera
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform float uAmbientStrength;

const float PI = 3.14159265359;

// --- PBR functions ---
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness, a2 = a * a;
    float NdotH = max(dot(N, H), 0.0), denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0, k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- PCSS ---
float PCSS(sampler2DArrayShadow shadowMap, vec3 worldPos, vec3 N, vec3 L) {
    // Select cascade
    float viewDepth = -(vec4(worldPos,1) * uCascadeViewProj[0]).z; // approximate
    // Reconstruct actual camera-space depth
    // Simplified: use world-space distance-based cascade selection
    float dist = length(worldPos - uCamPos);
    int cascade = 3;
    for (int c = 0; c < 4; ++c) {
        if (dist < uCascadeFar[c]) { cascade = c; break; }
    }
    vec4 lightSpace = uCascadeViewProj[cascade] * vec4(worldPos, 1);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    if (proj.x < -1 || proj.x > 1 || proj.y < -1 || proj.y > 1) return 1.0;

    // Simple PCF with fixed radius for now
    float shadow = 0;
    vec2 ts = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(uShadowMap, vec4(proj.xy + vec2(x,y) * ts, cascade, proj.z));
    return shadow / 9.0;
}

vec3 WorldPosFromDepth(vec2 uv, float depth, mat4 invVP) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = invVP * clip;
    return world.xyz / world.w;
}

void main() {
    // Sample GBuffer
    vec4 albedoMetallic  = texture(uAlbedoMetallic, vUV);
    vec4 normalRoughness = texture(uNormalRoughness, vUV);
    float depth          = texture(uDepth, vUV).r;

    vec3 albedo    = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N         = normalize(normalRoughness.xyz);
    float roughness = max(normalRoughness.a, 0.04);

    vec3 worldPos = WorldPosFromDepth(vUV, depth, uInvViewProj);
    vec3 V = normalize(uCamPos - worldPos);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // --- Direct light (Cook-Torrance) ---
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);

    vec3 radiance = uLightColor;
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular = numerator / denominator;
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 direct = (kD * albedo / PI + specular) * radiance * NdotL;

    // Shadow
    float shadow = PCSS(uShadowMap, worldPos, N, L);
    direct *= shadow;

    // --- IBL ---
    vec3 kS = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kDIbl = (1.0 - kS) * (1.0 - metallic);

    vec3 irradiance = texture(uIrradianceMap, N).rgb;
    vec3 diffuseIBL = kDIbl * irradiance * albedo;

    vec3 R = reflect(-V, N);
    float maxReflectionLod = 4.0;
    vec3 prefiltered = textureLod(uPrefilterMap, R, roughness * maxReflectionLod).rgb;
    vec2 envBRDF = texture(uBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefiltered * (kS * envBRDF.x + envBRDF.y);

    vec3 ambient = (diffuseIBL + specularIBL) * uAmbientStrength;

    vec3 color = direct + ambient;

    // Simple exposure + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

- [ ] **Step 7: Create `src/pipeline/passes/LightingPass.h`**

```cpp
#pragma once
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "../../renderer/Texture.h"
#include "../../renderer/UniformBuffer.h"

namespace HuanGL {

class Shader;
class GBufferPass;
class ShadowPass;
class Scene;

class LightingPass {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Render(const GBufferPass& gbuffer, const ShadowPass& shadow,
                const Scene& scene, const CameraData& camera);

private:
    void GenerateIBL(const std::string& hdrPath);

    std::unique_ptr<Shader> pbrShader_;
    std::shared_ptr<Texture> irradianceMap_;
    std::shared_ptr<Texture> prefilterMap_;
    std::shared_ptr<Texture> brdfLUT_;

    // Cubemap rendering
    std::unique_ptr<Framebuffer> captureFBO_;
    std::unique_ptr<VertexArray> cubeVAO_;
    std::unique_ptr<Buffer>      cubeVBO_;

    // Fullscreen triangle VAO (gl_VertexID shader)
    std::unique_ptr<VertexArray> dummyVAO_;
};

} // namespace HuanGL
```

- [ ] **Step 8: Create `src/pipeline/passes/LightingPass.cpp`**

```cpp
#include "LightingPass.h"
#include "GBufferPass.h"
#include "ShadowPass.h"
#include "../../renderer/Shader.h"
#include "../../renderer/Framebuffer.h"
#include "../../renderer/Buffer.h"
#include "../../renderer/Renderer.h"
#include "../../scene/Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace HuanGL {

// Cubemap face view matrices
static const glm::mat4 kCaptureViews[6] = {
    glm::lookAt({0,0,0}, { 1, 0, 0}, {0,-1, 0}),
    glm::lookAt({0,0,0}, {-1, 0, 0}, {0,-1, 0}),
    glm::lookAt({0,0,0}, { 0, 1, 0}, {0, 0, 1}),
    glm::lookAt({0,0,0}, { 0,-1, 0}, {0, 0,-1}),
    glm::lookAt({0,0,0}, { 0, 0, 1}, {0,-1, 0}),
    glm::lookAt({0,0,0}, { 0, 0,-1}, {0,-1, 0}),
};
static const glm::mat4 kCaptureProj = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 10.f);

// Vertex shader for cubemap rendering: outputs world-space direction from a unit cube
static const char* kCubeVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
out vec3 vWorldPos;
uniform mat4 uViewProj;
void main() {
    vWorldPos = aPos;
    vec4 clip = uViewProj * vec4(aPos, 1.0);
    gl_Position = clip.xyww; // force far plane
}
)";

void LightingPass::Init(int /*width*/, int /*height*/, const std::string& hdrPath) {
    pbrShader_ = std::make_unique<Shader>("../shader/lighting/fullscreen.vert",
                                          "../shader/lighting/pbr_ibl.frag");

    captureFBO_ = std::make_unique<Framebuffer>(512, 512);
    captureFBO_->AttachDepthRenderbuffer();

    // Unit cube for cubemap rendering (36 vertices)
    static const float kCubeVerts[] = {
        -1,-1,-1, -1,-1, 1, -1, 1, 1, -1,-1,-1, -1, 1, 1, -1, 1,-1, // -X
         1,-1, 1,  1,-1,-1,  1, 1,-1,  1,-1, 1,  1, 1,-1,  1, 1, 1, // +X
        -1,-1, 1,  1,-1, 1,  1, 1, 1, -1,-1, 1,  1, 1, 1, -1, 1, 1, // +Z
        -1,-1,-1, -1, 1,-1, -1, 1, 1, -1,-1,-1, -1, 1, 1, -1,-1, 1, // -X (was wrong)
        // Corrected faces:
    };
    // Full unit cube with correct winding:
    static const float kCube[108] = {
        -1,-1,-1, -1,-1, 1, -1, 1, 1, -1,-1,-1, -1, 1, 1, -1, 1,-1,
         1,-1, 1,  1,-1,-1,  1, 1,-1,  1,-1, 1,  1, 1,-1,  1, 1, 1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1, -1,-1, 1,  1, 1, 1, -1, 1, 1,
         1,-1,-1, -1,-1,-1, -1, 1,-1,  1,-1,-1, -1, 1,-1,  1, 1,-1,
        -1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1, 1,  1, 1,-1, -1, 1,-1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1,-1,  1,-1, 1, -1,-1, 1,
    };
    cubeVAO_ = std::make_unique<VertexArray>();
    cubeVBO_ = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    cubeVBO_->Upload(kCube, sizeof(kCube));
    cubeVAO_->Bind();
    cubeVBO_->Bind();
    cubeVAO_->BindVertexBuffer(0, cubeVBO_->GetID(), 3 * sizeof(float), 0);
    cubeVAO_->AddAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    cubeVAO_->Unbind();

    // Dummy VAO for fullscreen triangle (no vertex data needed — fullscreen.vert uses gl_VertexID)
    dummyVAO_ = std::make_unique<VertexArray>();

    GenerateIBL(hdrPath);
}

void LightingPass::GenerateIBL(const std::string& hdrPath) {
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    cubeVAO_->Bind();

    // Load HDR
    auto hdrTex = Texture::LoadHDR(hdrPath);

    // --- Step 1: Equirect to cubemap (512²) ---
    auto envCubemap = Texture::CreateCubemap(512, GL_RGB16F, true);
    Shader eqShader(kCubeVS, "../shader/lighting/equirect_to_cubemap.frag");
    eqShader.Use();
    hdrTex->Bind(0);

    for (int i = 0; i < 6; ++i) {
        glm::mat4 vp = kCaptureProj * kCaptureViews[i];
        eqShader.SetMat4("uViewProj", vp);
        glNamedFramebufferTextureLayer(captureFBO_->GetID(), GL_COLOR_ATTACHMENT0,
                                       envCubemap->GetID(), 0, i);
        captureFBO_->Bind();
        Renderer::SetViewport(0, 0, 512, 512);
        Renderer::Clear(true, false, false);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glGenerateTextureMipmap(envCubemap->GetID());

    // --- Step 2: Irradiance map (32² cubemap) ---
    irradianceMap_ = Texture::CreateCubemap(32, GL_RGB16F, false);
    Shader irrShader(kCubeVS, "../shader/lighting/irradiance.frag");
    irrShader.Use();
    envCubemap->Bind(0);

    for (int i = 0; i < 6; ++i) {
        irrShader.SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
        glNamedFramebufferTextureLayer(captureFBO_->GetID(), GL_COLOR_ATTACHMENT0,
                                       irradianceMap_->GetID(), 0, i);
        captureFBO_->Bind();
        Renderer::SetViewport(0, 0, 32, 32);
        Renderer::Clear(true, false, false);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // --- Step 3: Prefilter map (128² cubemap with 5 mips) ---
    prefilterMap_ = Texture::CreateCubemap(128, GL_RGB16F, true);
    glGenerateTextureMipmap(prefilterMap_->GetID());
    Shader pfShader(kCubeVS, "../shader/lighting/prefilter.frag");
    pfShader.Use();
    envCubemap->Bind(0);

    const int maxMips = 5;
    for (int mip = 0; mip < maxMips; ++mip) {
        int mipSize = 128 >> mip;
        glFramebufferTexture(captureFBO_->GetID()); // work around existing FBO
        // Recreate FBO for each mip size
        auto pfFBO = std::make_unique<Framebuffer>(mipSize, mipSize);
        pfFBO->AttachDepthRenderbuffer();

        float roughness = (float)mip / (float)(maxMips - 1);
        pfShader.SetFloat("uRoughness", roughness);

        for (int i = 0; i < 6; ++i) {
            pfShader.SetMat4("uViewProj", kCaptureProj * kCaptureViews[i]);
            glNamedFramebufferTextureLayer(pfFBO->GetID(), GL_COLOR_ATTACHMENT0,
                                           prefilterMap_->GetID(), mip, i);
            pfFBO->Bind();
            Renderer::SetViewport(0, 0, mipSize, mipSize);
            Renderer::Clear(true, false, false);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    // --- Step 4: BRDF LUT (512² 2D) ---
    brdfLUT_ = Texture::Create2D(512, 512, GL_RG16F, GL_RG, GL_FLOAT);
    Shader brdfShader("../shader/lighting/fullscreen.vert",
                      "../shader/lighting/brdf_lut.frag");
    auto brdfFBO = std::make_unique<Framebuffer>(512, 512);
    brdfFBO->AttachColor(brdfLUT_, 0);
    brdfFBO->AttachDepthRenderbuffer();
    brdfShader.Use();
    brdfFBO->Bind();
    Renderer::SetViewport(0, 0, 512, 512);
    Renderer::Clear(true, false, false);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    cubeVAO_->Unbind();
    Framebuffer::BindDefault();
    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

void LightingPass::Render(const GBufferPass& gbuffer, const ShadowPass& shadow,
                           const Scene& scene, const CameraData& camera) {
    Renderer::EnableDepthTest(false);
    Renderer::EnableCullFace(false);

    pbrShader_->Use();

    // GBuffer
    gbuffer.GetAlbedoMetallic()->Bind(0);
    gbuffer.GetNormalRoughness()->Bind(1);
    gbuffer.GetDepth()->Bind(2);

    // Shadow
    glBindTextureUnit(3, shadow.GetShadowMapArray());
    auto& cascades = shadow.GetCascades();
    for (int c = 0; c < 4; ++c) {
        pbrShader_->SetMat4("uCascadeViewProj[" + std::to_string(c) + "]", cascades[c].viewProj);
        pbrShader_->SetFloat("uCascadeFar[" + std::to_string(c) + "]", cascades[c].farPlane);
    }
    auto& light = scene.GetSunLight();
    pbrShader_->SetVec3("uLightDir", light.direction);
    pbrShader_->SetVec3("uLightColor", light.color * light.intensity);

    // IBL
    irradianceMap_->Bind(4);
    prefilterMap_->Bind(5);
    brdfLUT_->Bind(6);

    // Camera
    pbrShader_->SetMat4("uInvViewProj", glm::inverse(camera.viewProj));
    pbrShader_->SetVec3("uCamPos", camera.camPos);
    pbrShader_->SetFloat("uAmbientStrength", 1.0f);

    dummyVAO_->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    dummyVAO_->Unbind();

    Renderer::EnableCullFace(true);
    Renderer::EnableDepthTest(true);
}

} // namespace HuanGL
```

- [ ] **Step 9: Build and commit**

```powershell
cmake --build build --config Debug
```

```bash
git add shader/lighting/ src/pipeline/passes/LightingPass.h src/pipeline/passes/LightingPass.cpp
git commit -m "feat: add LightingPass with PBR direct light and IBL (irradiance/prefilter/BRDF LUT)"
```

---

### Task 7: RenderPipeline + App integration

**Files:**
- Create: `src/pipeline/RenderPipeline.h`
- Create: `src/pipeline/RenderPipeline.cpp`
- Modify: `src/core/App.h`
- Modify: `src/core/App.cpp`

- [ ] **Step 1: Create `src/pipeline/RenderPipeline.h`**

```cpp
#pragma once
#include <memory>
#include <string>
#include "passes/ShadowPass.h"
#include "passes/GBufferPass.h"
#include "passes/LightingPass.h"
#include "../renderer/UniformBuffer.h"

namespace HuanGL {

class Scene;

class RenderPipeline {
public:
    void Init(int width, int height, const std::string& hdrPath);
    void Resize(int width, int height);
    void Execute(const Scene& scene, const CameraData& camera);

private:
    ShadowPass   shadowPass_;
    GBufferPass  gbufferPass_;
    LightingPass lightingPass_;
};

} // namespace HuanGL
```

- [ ] **Step 2: Create `src/pipeline/RenderPipeline.cpp`**

```cpp
#include "RenderPipeline.h"
#include "../scene/Scene.h"

namespace HuanGL {

void RenderPipeline::Init(int w, int h, const std::string& hdrPath) {
    shadowPass_.Init(2048);
    gbufferPass_.Init(w, h);
    lightingPass_.Init(w, h, hdrPath);
}

void RenderPipeline::Resize(int w, int h) {
    gbufferPass_.Resize(w, h);
}

void RenderPipeline::Execute(const Scene& scene, const CameraData& camera) {
    shadowPass_.Render(scene, camera, scene.GetSunLight());
    gbufferPass_.Render(scene, camera);
    lightingPass_.Render(gbufferPass_, shadowPass_, scene, camera);
}

} // namespace HuanGL
```

- [ ] **Step 3: Modify `src/core/App.h`** — add new members

Read the current file, then replace:

```cpp
#pragma once
#include <memory>

namespace HuanGL {

class Window;

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void Run();

private:
    void Init();
    void Shutdown();
    void Update(float dt);
    void Render();

    std::unique_ptr<Window> window_;
    float lastTime_ = 0.0f;
    bool  running_  = true;
};

} // namespace HuanGL
```

Replace with:

```cpp
#pragma once
#include <memory>
#include <string>
#include "../renderer/UniformBuffer.h"

namespace HuanGL {

class Window;
class RenderPipeline;
class Scene;
class Camera;
class ResourceManager;

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void Run();

private:
    void Init();
    void Shutdown();
    void Update(float dt);
    void Render();

    std::unique_ptr<Window>          window_;
    std::unique_ptr<Camera>          camera_;
    std::unique_ptr<Scene>           scene_;
    std::unique_ptr<RenderPipeline>  pipeline_;
    std::unique_ptr<ResourceManager> resourceManager_;

    std::unique_ptr<CameraUBO> cameraUBO_;
    std::unique_ptr<LightsUBO> lightsUBO_;
    std::unique_ptr<TimeUBO>   timeUBO_;

    float lastTime_ = 0.0f;
    bool  running_  = true;
};

} // namespace HuanGL
```

- [ ] **Step 4: Modify `src/core/App.cpp`** — wire up the pipeline

Replace entire file:

```cpp
#include "App.h"
#include "Window.h"
#include "Input.h"
#include "Camera.h"
#include "../renderer/Renderer.h"
#include "../pipeline/RenderPipeline.h"
#include "../scene/Scene.h"
#include "../scene/TestScene.h"
#include "../resource/ResourceManager.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace HuanGL {

App::App()  { Init(); }
App::~App() { Shutdown(); }

void App::Init() {
    window_ = std::make_unique<Window>(1280, 720, "HuanGL");
    Input::Init(window_->GetHandle());
    Renderer::Init();
    Renderer::SetViewport(0, 0, window_->GetWidth(), window_->GetHeight());

    window_->SetResizeCallback([](int w, int h) {
        Renderer::SetViewport(0, 0, w, h);
    });

    // UBOs
    cameraUBO_ = std::make_unique<CameraUBO>();
    lightsUBO_ = std::make_unique<LightsUBO>();
    timeUBO_   = std::make_unique<TimeUBO>();

    // Camera
    camera_ = std::make_unique<Camera>(60.f, 0.1f, 100.f);
    Input::SetCursorCaptured(true);

    // Resource + Scene
    resourceManager_ = std::make_unique<ResourceManager>();
    scene_ = std::make_unique<TestScene>();
    scene_->Init(*resourceManager_);

    // Pipeline
    pipeline_ = std::make_unique<RenderPipeline>();
    pipeline_->Init(window_->GetWidth(), window_->GetHeight(),
                    "../resources/texture/hdr/brown_photostudio_02_2k.hdr");
}

void App::Shutdown() {
    ResourceManager::Shutdown();
}

void App::Run() {
    while (!window_->ShouldClose() && running_) {
        float now = static_cast<float>(glfwGetTime());
        float dt  = now - lastTime_;
        lastTime_ = now;

        Input::Update();
        window_->PollEvents();

        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
            running_ = false;

        Update(dt);
        Render();
        window_->SwapBuffers();
    }
}

void App::Update(float dt) {
    camera_->Update(dt, window_->GetHandle(), true);
    scene_->Update(dt);

    TimeData timeData;
    timeData.time      = static_cast<float>(glfwGetTime());
    timeData.deltaTime = dt;
    timeUBO_->Update(timeData);
}

void App::Render() {
    int w = window_->GetWidth();
    int h = window_->GetHeight();
    float aspect = w > 0 && h > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.f;

    CameraData camData = camera_->GetData(aspect);
    cameraUBO_->Update(camData);

    LightsData lightData;
    auto& sun = scene_->GetSunLight();
    lightData.dirLightDir       = sun.direction;
    lightData.dirLightColor     = sun.color;
    lightData.dirLightIntensity = sun.intensity;
    lightsUBO_->Update(lightData);

    Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    Renderer::Clear();

    pipeline_->Execute(*scene_, camData);
}

} // namespace HuanGL
```

- [ ] **Step 5: Build and run**

```powershell
cmake --build build --config Debug
```

Expected: compiles clean.

```bash
git add src/pipeline/RenderPipeline.h src/pipeline/RenderPipeline.cpp src/core/App.h src/core/App.cpp
git commit -m "feat: add RenderPipeline orchestrator and integrate into App"
```

---

## Verification Checklist

After all tasks complete:

- [ ] `cmake --build build --config Debug` succeeds with no warnings
- [ ] Window opens, dark background with three colored spheres on a gray plane
- [ ] Camera: WASD move, mouse look, ESC exit
- [ ] No GL debug callback errors in terminal
- [ ] Three spheres show distinct PBR materials (rusty iron, copper, polished gold)
- [ ] Specular highlights visible on metallic spheres
- [ ] Environment reflections visible on gold sphere
- [ ] Shadow on the plane under the spheres
- [ ] Shadow has soft edges (3×3 PCF)

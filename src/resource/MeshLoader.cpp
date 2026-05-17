#include "MeshLoader.h"
#include "../renderer/Buffer.h"
#include "../renderer/Texture.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <stdexcept>
#include <vector>
#include <filesystem>

namespace HuanGL {

static std::string ResolvePath(const std::string& modelDir, const aiString& texPath) {
    std::filesystem::path p(texPath.C_Str());
    if (p.is_absolute()) return p.string();
    return (std::filesystem::path(modelDir) / p).string();
}

// Loads either:
//  1. An external texture file referenced by relative/absolute path, or
//  2. An embedded texture from the aiScene when path starts with '*' (the
//     digits after '*' index into aiScene::mTextures[]). This is how
//     Assimp surfaces textures packed inside .glb / FBX files.
static std::shared_ptr<Texture> LoadMaterialTexture(const aiScene* scene,
                                                     const aiMaterial* mat,
                                                     aiTextureType type,
                                                     const std::string& modelDir,
                                                     bool sRGB) {
    if (mat->GetTextureCount(type) == 0) return nullptr;
    aiString path;
    mat->GetTexture(type, 0, &path);
    const char* cstr = path.C_Str();

    // Embedded texture: '*N' references aiScene->mTextures[N].
    if (cstr && cstr[0] == '*') {
        unsigned idx = static_cast<unsigned>(std::atoi(cstr + 1));
        if (!scene->mTextures || idx >= scene->mNumTextures)
            throw std::runtime_error(
                std::string("[MeshLoader] Embedded texture index out of range: ") + cstr);
        const aiTexture* tex = scene->mTextures[idx];
        // mHeight == 0 means pcData is compressed (PNG/JPG bytes), mWidth is size in bytes.
        // Otherwise it's raw RGBA8888 with width*height pixels.
        if (tex->mHeight == 0) {
            return Texture::Load2DFromMemory(
                reinterpret_cast<const unsigned char*>(tex->pcData),
                tex->mWidth, sRGB);
        }
        // Raw uncompressed embedded texture — uncommon for glTF. Fall through
        // to an error rather than silently mis-decoding.
        throw std::runtime_error(
            "[MeshLoader] Uncompressed embedded texture not supported yet");
    }

    // External file on disk.
    std::string resolved = ResolvePath(modelDir, path);
    return Texture::Load2D(resolved, sRGB);
}

LoadResult MeshLoader::Load(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* aiscene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_CalcTangentSpace | aiProcess_FlipUVs);

    if (!aiscene || !aiscene->HasMeshes())
        throw std::runtime_error("[MeshLoader] Failed: " + path);

    std::string modelDir = std::filesystem::path(path).parent_path().string();

    // --- Load materials ---
    std::vector<Material> materials;
    materials.reserve(aiscene->mNumMaterials);

    for (unsigned mi = 0; mi < aiscene->mNumMaterials; ++mi) {
        const aiMaterial* aiMat = aiscene->mMaterials[mi];
        Material mat;

        // Base color / diffuse texture (sRGB)
        mat.albedoMap = LoadMaterialTexture(aiscene, aiMat, aiTextureType_BASE_COLOR, modelDir, true);
        if (!mat.albedoMap)
            mat.albedoMap = LoadMaterialTexture(aiscene, aiMat, aiTextureType_DIFFUSE, modelDir, true);

        // Normal map (linear)
        mat.normalMap = LoadMaterialTexture(aiscene, aiMat, aiTextureType_NORMALS, modelDir, false);
        if (!mat.normalMap)
            mat.normalMap = LoadMaterialTexture(aiscene, aiMat, aiTextureType_HEIGHT, modelDir, false);

        // Metallic-roughness (glTF packs into one texture as aiTextureType_UNKNOWN or separate)
        auto mrTex = LoadMaterialTexture(aiscene, aiMat, aiTextureType_UNKNOWN, modelDir, false);
        if (mrTex) {
            // glTF packed: G=roughness, B=metallic
            mat.roughnessMap = mrTex;
            mat.packedMetallicRoughness = true;
        } else {
            mat.roughnessMap = LoadMaterialTexture(aiscene, aiMat, aiTextureType_DIFFUSE_ROUGHNESS, modelDir, false);
            mat.metallicMap  = LoadMaterialTexture(aiscene, aiMat, aiTextureType_METALNESS, modelDir, false);
        }

        // Factor values
        aiColor4D baseColor;
        if (aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
            mat.baseColorFactor = {baseColor.r, baseColor.g, baseColor.b, baseColor.a};
        } else {
            aiColor4D diffuse;
            if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
                mat.baseColorFactor = {diffuse.r, diffuse.g, diffuse.b, diffuse.a};
        }

        float roughness = 1.0f;
        if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
            mat.roughnessFactor = roughness;

        float metallic = 0.0f;
        if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
            mat.metallicFactor = metallic;

        materials.push_back(std::move(mat));
    }

    // --- Load geometry ---
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

    return {mesh, std::move(materials)};
}

} // namespace HuanGL

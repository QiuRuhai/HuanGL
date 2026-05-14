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

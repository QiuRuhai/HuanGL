#pragma once
#include "../renderer/Schema.h"
#include "../renderer/Buffer.h"
#include <glad/glad.h>
#include <memory>
#include <vector>
#include <cstddef>

namespace HuanGL {

// Uploads vertex and index data to GL buffers (VAO/VBO/EBO) and records a
// host-side copy in cpuGeometry so later systems (e.g. BVH construction for
// the reference path tracer) can access triangle data without reading back
// from the GPU.
inline std::shared_ptr<Mesh> BuildMesh(const std::vector<Vertex>& verts,
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

    // Retain a host-side copy so BVH construction and other CPU systems can
    // read triangle data without a GPU readback.
    m->cpuGeometry = std::make_shared<CpuGeometry>();
    m->cpuGeometry->vertices = verts;
    m->cpuGeometry->indices  = idx;

    return m;
}

} // namespace HuanGL

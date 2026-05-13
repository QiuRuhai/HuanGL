#pragma once
#include <cstddef>
#include <glad/glad.h>

namespace HuanGL {

class VertexArray {
public:
    VertexArray();
    ~VertexArray();
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    void Bind() const;
    void Unbind() const;
    // index: attribute location; count: component count; stride/offset in bytes
    void AddAttribute(GLuint index, GLint count, GLenum type,
                      GLboolean normalized, GLsizei stride, size_t offset);
    void BindVertexBuffer(GLuint bindingIndex, GLuint vboID, GLsizei stride, GLintptr offset = 0);

    GLuint GetID() const { return id_; }

private:
    GLuint id_ = 0;
};

class Buffer {
public:
    explicit Buffer(GLenum target, GLenum usage = GL_STATIC_DRAW);
    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    void Bind() const;
    void Unbind() const;
    void Upload(const void* data, size_t size);
    void UpdateSubData(const void* data, size_t size, size_t offset = 0);
    void BindBase(GLuint bindingPoint) const; // for UBO/SSBO

    GLuint GetID() const { return id_; }
    GLenum GetTarget() const { return target_; }
    size_t GetSize() const { return size_; }

private:
    GLuint id_     = 0;
    GLenum target_ = GL_ARRAY_BUFFER;
    GLenum usage_  = GL_STATIC_DRAW;
    size_t size_   = 0;
};

} // namespace HuanGL

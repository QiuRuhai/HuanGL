#include "Buffer.h"

namespace HuanGL {

// --- VertexArray ---

VertexArray::VertexArray() {
    glCreateVertexArrays(1, &id_); // DSA
}

VertexArray::~VertexArray() {
    glDeleteVertexArrays(1, &id_);
}

void VertexArray::Bind() const   { glBindVertexArray(id_); }
void VertexArray::Unbind() const { glBindVertexArray(0); }

void VertexArray::AddAttribute(GLuint index, GLint count, GLenum type,
                                GLboolean normalized, GLsizei stride, size_t offset)
{
    glEnableVertexArrayAttrib(id_, index);
    glVertexArrayAttribFormat(id_, index, count, type, normalized,
                              static_cast<GLuint>(offset));
    glVertexArrayAttribBinding(id_, index, 0);
    (void)stride; // stride is set on the buffer binding, not the attribute
}

void VertexArray::BindVertexBuffer(GLuint bindingIndex, GLuint vboID,
                                    GLsizei stride, GLintptr offset)
{
    glVertexArrayVertexBuffer(id_, bindingIndex, vboID, offset, stride);
}

// --- Buffer ---

Buffer::Buffer(GLenum target, GLenum usage)
    : target_(target), usage_(usage)
{
    glCreateBuffers(1, &id_); // DSA
}

Buffer::~Buffer() {
    glDeleteBuffers(1, &id_);
}

void Buffer::Bind() const   { glBindBuffer(target_, id_); }
void Buffer::Unbind() const { glBindBuffer(target_, 0); }

void Buffer::Upload(const void* data, size_t size) {
    size_ = size;
    glNamedBufferData(id_, static_cast<GLsizeiptr>(size), data, usage_);
}

void Buffer::UpdateSubData(const void* data, size_t size, size_t offset) {
    glNamedBufferSubData(id_,
                         static_cast<GLintptr>(offset),
                         static_cast<GLsizeiptr>(size),
                         data);
}

void Buffer::BindBase(GLuint bindingPoint) const {
    glBindBufferBase(target_, bindingPoint, id_);
}

} // namespace HuanGL

#include "Framebuffer.h"
#include <iostream>

namespace HuanGL {

Framebuffer::Framebuffer(int w, int h) : width_(w), height_(h) {
    glCreateFramebuffers(1, &id_); // DSA
}

Framebuffer::~Framebuffer() {
    if (rbo_) glDeleteRenderbuffers(1, &rbo_);
    if (id_)  glDeleteFramebuffers(1, &id_);
}

void Framebuffer::AttachColor(std::shared_ptr<Texture> tex, GLuint attachment, int mip) {
    glNamedFramebufferTexture(id_, GL_COLOR_ATTACHMENT0 + attachment, tex->GetID(), mip);
    if (colors_.size() <= static_cast<size_t>(attachment))
        colors_.resize(static_cast<size_t>(attachment) + 1);
    colors_[attachment] = std::move(tex);
}

void Framebuffer::AttachDepth(std::shared_ptr<Texture> tex) {
    glNamedFramebufferTexture(id_, GL_DEPTH_ATTACHMENT, tex->GetID(), 0);
    depth_ = std::move(tex);
}

void Framebuffer::AttachDepthRenderbuffer() {
    glCreateRenderbuffers(1, &rbo_);
    glNamedRenderbufferStorage(rbo_, GL_DEPTH_COMPONENT24, width_, height_);
    glNamedFramebufferRenderbuffer(id_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_);
}

void Framebuffer::SetDrawBuffers(const std::vector<GLenum>& buffers) {
    glNamedFramebufferDrawBuffers(id_,
        static_cast<GLsizei>(buffers.size()), buffers.data());
}

bool Framebuffer::IsComplete() const {
    GLenum status = glCheckNamedFramebufferStatus(id_, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Framebuffer] Incomplete: 0x" << std::hex << status << "\n";
        return false;
    }
    return true;
}

void Framebuffer::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, id_); }
void Framebuffer::BindDefault() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

std::shared_ptr<Texture> Framebuffer::GetColor(GLuint index) const {
    return (static_cast<size_t>(index) < colors_.size()) ? colors_[index] : nullptr;
}

std::shared_ptr<Texture> Framebuffer::GetDepth() const { return depth_; }

} // namespace HuanGL

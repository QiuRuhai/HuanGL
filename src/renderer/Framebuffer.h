#pragma once
#include <vector>
#include <memory>
#include <glad/glad.h>
#include "Texture.h"

namespace HuanGL {

class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void AttachColor(std::shared_ptr<Texture> tex, GLuint attachment = 0, int mipLevel = 0);
    void AttachDepth(std::shared_ptr<Texture> tex);
    void AttachDepthRenderbuffer(); // depth-only, not sampled
    void SetDrawBuffers(const std::vector<GLenum>& buffers); // for MRT
    bool IsComplete() const;

    void Bind() const;
    static void BindDefault();

    std::shared_ptr<Texture> GetColor(GLuint index = 0) const;
    std::shared_ptr<Texture> GetDepth() const;

    int GetWidth() const  { return width_; }
    int GetHeight() const { return height_; }
    GLuint GetID() const  { return id_; }

private:
    GLuint id_  = 0;
    GLuint rbo_ = 0;
    int width_, height_;
    std::vector<std::shared_ptr<Texture>> colors_;
    std::shared_ptr<Texture> depth_;
};

} // namespace HuanGL

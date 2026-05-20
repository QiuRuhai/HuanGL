#pragma once
#include <string>
#include <memory>
#include <cmath>
#include <glad/glad.h>

namespace HuanGL {

class Texture {
public:
    // Load 2D texture from file (sRGB=true uses GL_SRGB8_ALPHA8)
    static std::shared_ptr<Texture> Load2D(const std::string& path, bool sRGB = true);
    // Load 2D texture from in-memory encoded image bytes (PNG/JPG/etc).
    // Used for textures embedded in .glb / FBX files via Assimp.
    static std::shared_ptr<Texture> Load2DFromMemory(const unsigned char* data,
                                                      size_t size,
                                                      bool sRGB = true);
    // Load HDR float texture from file
    static std::shared_ptr<Texture> LoadHDR(const std::string& path);
    // Create empty 2D texture (for FBO attachments)
    static std::shared_ptr<Texture> Create2D(int w, int h,
        GLenum internalFmt, GLenum fmt, GLenum type);
    // Create empty cubemap
    static std::shared_ptr<Texture> CreateCubemap(int size,
        GLenum internalFmt, bool mipmap = false);
    // Create empty 3D texture (for VXGI voxel grid)
    static std::shared_ptr<Texture> Create3D(int w, int h, int d, GLenum internalFmt);

    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void Bind(GLuint slot = 0) const;
    void BindImage(GLuint unit, GLenum access, GLenum fmt, int level = 0) const;
    void GenerateMipmaps() const;
    void SetFilter(GLenum minFilter, GLenum magFilter) const;
    void SetWrap(GLenum wrapS, GLenum wrapT) const;

    GLuint GetID() const     { return id_; }
    int GetWidth() const     { return width_; }
    int GetHeight() const    { return height_; }
    int GetDepth() const     { return depth_; }
    GLenum GetTarget() const { return target_; }

private:
    Texture() = default;
    GLuint id_     = 0;
    int    width_  = 0;
    int    height_ = 0;
    int    depth_  = 0;
    GLenum target_ = GL_TEXTURE_2D;
};

} // namespace HuanGL

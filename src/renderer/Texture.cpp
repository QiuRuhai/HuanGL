#include "Texture.h"
#include <stb_image.h>
#include <stdexcept>
#include <algorithm>

namespace HuanGL {

std::shared_ptr<Texture> Texture::Load2D(const std::string& path, bool sRGB) {
    auto tex = std::shared_ptr<Texture>(new Texture());
    tex->target_ = GL_TEXTURE_2D;

    stbi_set_flip_vertically_on_load(true);
    int nrChannels = 0;
    unsigned char* data = stbi_load(path.c_str(), &tex->width_, &tex->height_, &nrChannels, 0);
    if (!data)
        throw std::runtime_error("[Texture] Failed to load: " + path);

    GLenum intFmt, fmt;
    if (nrChannels == 4) {
        intFmt = sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        fmt    = GL_RGBA;
    } else if (nrChannels == 3) {
        intFmt = sRGB ? GL_SRGB8 : GL_RGB8;
        fmt    = GL_RGB;
    } else {
        intFmt = GL_R8;
        fmt    = GL_RED;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &tex->id_);
    glTextureStorage2D(tex->id_, 1, intFmt, tex->width_, tex->height_);
    glTextureSubImage2D(tex->id_, 0, 0, 0, tex->width_, tex->height_,
                        fmt, GL_UNSIGNED_BYTE, data);
    glGenerateTextureMipmap(tex->id_);
    glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

std::shared_ptr<Texture> Texture::Load2DFromMemory(const unsigned char* bytes,
                                                    size_t size, bool sRGB) {
    auto tex = std::shared_ptr<Texture>(new Texture());
    tex->target_ = GL_TEXTURE_2D;

    stbi_set_flip_vertically_on_load(true);
    int nrChannels = 0;
    unsigned char* data = stbi_load_from_memory(bytes, static_cast<int>(size),
                                                 &tex->width_, &tex->height_,
                                                 &nrChannels, 0);
    if (!data)
        throw std::runtime_error("[Texture] Failed to decode embedded texture");

    GLenum intFmt, fmt;
    if (nrChannels == 4) {
        intFmt = sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        fmt    = GL_RGBA;
    } else if (nrChannels == 3) {
        intFmt = sRGB ? GL_SRGB8 : GL_RGB8;
        fmt    = GL_RGB;
    } else {
        intFmt = GL_R8;
        fmt    = GL_RED;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &tex->id_);
    glTextureStorage2D(tex->id_, 1, intFmt, tex->width_, tex->height_);
    glTextureSubImage2D(tex->id_, 0, 0, 0, tex->width_, tex->height_,
                        fmt, GL_UNSIGNED_BYTE, data);
    glGenerateTextureMipmap(tex->id_);
    glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

std::shared_ptr<Texture> Texture::LoadHDR(const std::string& path) {
    auto tex = std::shared_ptr<Texture>(new Texture());
    tex->target_ = GL_TEXTURE_2D;

    stbi_set_flip_vertically_on_load(true);
    int nrChannels = 0;
    float* data = stbi_loadf(path.c_str(), &tex->width_, &tex->height_, &nrChannels, 0);
    if (!data)
        throw std::runtime_error("[Texture] Failed to load HDR: " + path);

    glCreateTextures(GL_TEXTURE_2D, 1, &tex->id_);
    glTextureStorage2D(tex->id_, 1, GL_RGB16F, tex->width_, tex->height_);
    glTextureSubImage2D(tex->id_, 0, 0, 0, tex->width_, tex->height_,
                        GL_RGB, GL_FLOAT, data);
    glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    return tex;
}

std::shared_ptr<Texture> Texture::Create2D(int w, int h,
    GLenum internalFmt, GLenum /*fmt*/, GLenum /*type*/)
{
    auto tex = std::shared_ptr<Texture>(new Texture());
    tex->target_ = GL_TEXTURE_2D;
    tex->width_  = w;
    tex->height_ = h;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex->id_);
    glTextureStorage2D(tex->id_, 1, internalFmt, w, h);
    glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

std::shared_ptr<Texture> Texture::CreateCubemap(int size, GLenum internalFmt, bool mipmap) {
    auto tex = std::shared_ptr<Texture>(new Texture());
    tex->target_ = GL_TEXTURE_CUBE_MAP;
    tex->width_  = size;
    tex->height_ = size;

    int levels = mipmap ? (static_cast<int>(std::floor(std::log2(size))) + 1) : 1;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex->id_);
    glTextureStorage2D(tex->id_, levels, internalFmt, size, size);
    glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER,
                        mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return tex;
}

std::shared_ptr<Texture> Texture::Create3D(int w, int h, int d, GLenum internalFmt) {
    auto tex = std::shared_ptr<Texture>(new Texture());
    tex->target_ = GL_TEXTURE_3D;
    tex->width_  = w;
    tex->height_ = h;
    tex->depth_  = d;

    glCreateTextures(GL_TEXTURE_3D, 1, &tex->id_);
    glTextureStorage3D(tex->id_, 1, internalFmt, w, h, d);
    glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return tex;
}

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

void Texture::Bind(GLuint slot) const {
    glBindTextureUnit(slot, id_); // DSA
}

void Texture::BindImage(GLuint unit, GLenum access, GLenum fmt, int level) const {
    glBindImageTexture(unit, id_, level, GL_TRUE, 0, access, fmt);
}

void Texture::GenerateMipmaps() const {
    glGenerateTextureMipmap(id_); // DSA
}

void Texture::SetFilter(GLenum minFilter, GLenum magFilter) const {
    glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, minFilter);
    glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, magFilter);
}

void Texture::SetWrap(GLenum wrapS, GLenum wrapT) const {
    glTextureParameteri(id_, GL_TEXTURE_WRAP_S, wrapS);
    glTextureParameteri(id_, GL_TEXTURE_WRAP_T, wrapT);
}

} // namespace HuanGL

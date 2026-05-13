#include "Renderer.h"
#include <iostream>

static void GLAPIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei, const GLchar* message, const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    const char* srcStr = "?", *typeStr = "?", *sevStr = "?";
    switch (source) {
        case GL_DEBUG_SOURCE_API:             srcStr = "API"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: srcStr = "Shader"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     srcStr = "App"; break;
        default: break;
    }
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               typeStr = "ERROR"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "DEPRECATED"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "PERFORMANCE"; break;
        default: typeStr = "OTHER"; break;
    }
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:   sevStr = "HIGH"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: sevStr = "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_LOW:    sevStr = "LOW"; break;
        default: break;
    }
    std::cerr << "[GL " << sevStr << "] (" << srcStr << "/" << typeStr
              << " #" << id << ") " << message << "\n";
}

namespace HuanGL {

void Renderer::Init() {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLDebugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void Renderer::SetViewport(int x, int y, int w, int h) {
    glViewport(x, y, w, h);
}

void Renderer::Clear(bool color, bool depth, bool stencil) {
    GLbitfield mask = 0;
    if (color)   mask |= GL_COLOR_BUFFER_BIT;
    if (depth)   mask |= GL_DEPTH_BUFFER_BIT;
    if (stencil) mask |= GL_STENCIL_BUFFER_BIT;
    glClear(mask);
}

void Renderer::SetClearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void Renderer::EnableDepthTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

void Renderer::EnableDepthWrite(bool enable) {
    glDepthMask(enable ? GL_TRUE : GL_FALSE);
}

void Renderer::SetDepthFunc(GLenum func) {
    glDepthFunc(func);
}

void Renderer::EnableBlend(bool enable) {
    if (enable) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}

void Renderer::SetBlendFunc(GLenum src, GLenum dst) {
    glBlendFunc(src, dst);
}

void Renderer::EnableCullFace(bool enable) {
    if (enable) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

void Renderer::SetCullFace(GLenum face) {
    glCullFace(face);
}

void Renderer::EnableSeamlessCubemap(bool enable) {
    if (enable) glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    else        glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void Renderer::PushDebugGroup(const std::string& name) {
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0,
                     static_cast<GLsizei>(name.size()), name.c_str());
}

void Renderer::PopDebugGroup() {
    glPopDebugGroup();
}

} // namespace HuanGL

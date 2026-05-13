#pragma once
#include <string>
#include <glad/glad.h>

namespace HuanGL {

class Renderer {
public:
    static void Init();

    static void SetViewport(int x, int y, int width, int height);
    static void Clear(bool color = true, bool depth = true, bool stencil = false);
    static void SetClearColor(float r, float g, float b, float a = 1.0f);

    static void EnableDepthTest(bool enable);
    static void EnableDepthWrite(bool enable);
    static void SetDepthFunc(GLenum func);
    static void EnableBlend(bool enable);
    static void SetBlendFunc(GLenum src, GLenum dst);
    static void EnableCullFace(bool enable);
    static void SetCullFace(GLenum face);
    static void EnableSeamlessCubemap(bool enable);

    static void PushDebugGroup(const std::string& name);
    static void PopDebugGroup();

};

} // namespace HuanGL

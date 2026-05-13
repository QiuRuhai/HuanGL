#include "core/Window.h"
#include "core/Input.h"
#include "renderer/Renderer.h"
#include "renderer/Shader.h"
#include "renderer/Buffer.h"
#include <iostream>

int main() {
    try {
        HuanGL::Window window(1280, 720, "HuanGL");
        HuanGL::Input::Init(window.GetHandle());
        HuanGL::Renderer::Init();
        HuanGL::Renderer::SetViewport(0, 0, window.GetWidth(), window.GetHeight());

        HuanGL::Shader testShader("../shader/test/test.vert", "../shader/test/test.frag");

        float verts[] = { -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f, 0.5f, 0.0f };
        HuanGL::VertexArray vao;
        HuanGL::Buffer vbo(GL_ARRAY_BUFFER);
        vbo.Upload(verts, sizeof(verts));
        vao.BindVertexBuffer(0, vbo.GetID(), 3 * sizeof(float));
        vao.AddAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
        std::cout << "[Test] VAO=" << vao.GetID() << " VBO=" << vbo.GetID() << "\n";

        while (!window.ShouldClose()) {
            HuanGL::Input::Update();
            window.PollEvents();
            if (HuanGL::Input::IsKeyPressed(GLFW_KEY_ESCAPE)) break;

            HuanGL::Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            HuanGL::Renderer::Clear();
            testShader.Use();
            vao.Bind();
            glDrawArrays(GL_TRIANGLES, 0, 3);
            window.SwapBuffers();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return -1;
    }
    return 0;
}

#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace HuanGL {

class Shader {
public:
    // Vertex + fragment
    Shader(const std::string& vertPath, const std::string& fragPath);
    // Vertex + geometry + fragment
    Shader(const std::string& vertPath, const std::string& geomPath,
           const std::string& fragPath);
    // Compute shader (OpenGL 4.3+)
    explicit Shader(const std::string& computePath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Use() const;
    // Dispatch compute shader; also inserts memory barrier
    void Dispatch(GLuint x, GLuint y = 1, GLuint z = 1) const;

    // DSA-style uniforms (glProgramUniform — no bind needed)
    void SetBool(const std::string& name, bool v) const;
    void SetInt(const std::string& name, int v) const;
    void SetFloat(const std::string& name, float v) const;
    void SetVec2(const std::string& name, const glm::vec2& v) const;
    void SetVec3(const std::string& name, const glm::vec3& v) const;
    void SetVec4(const std::string& name, const glm::vec4& v) const;
    void SetMat3(const std::string& name, const glm::mat3& m) const;
    void SetMat4(const std::string& name, const glm::mat4& m) const;

    GLuint GetID() const { return id_; }

    static void SetBasePath(const std::string& basePath);

private:
    GLuint id_ = 0;
    static std::string basePath_;

    GLuint Compile(const std::string& path, GLenum type) const;
    std::string ReadFile(const std::string& path) const;
    void Link() const;
};

} // namespace HuanGL

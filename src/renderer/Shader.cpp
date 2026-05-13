#include "Shader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

namespace HuanGL {

Shader::Shader(const std::string& vert, const std::string& frag) {
    id_ = glCreateProgram();
    GLuint vs = Compile(vert, GL_VERTEX_SHADER);
    GLuint fs = Compile(frag, GL_FRAGMENT_SHADER);
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    Link();
    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::Shader(const std::string& vert, const std::string& geom, const std::string& frag) {
    id_ = glCreateProgram();
    GLuint vs = Compile(vert, GL_VERTEX_SHADER);
    GLuint gs = Compile(geom, GL_GEOMETRY_SHADER);
    GLuint fs = Compile(frag, GL_FRAGMENT_SHADER);
    glAttachShader(id_, vs);
    glAttachShader(id_, gs);
    glAttachShader(id_, fs);
    Link();
    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);
}

Shader::Shader(const std::string& compute) {
    id_ = glCreateProgram();
    GLuint cs = Compile(compute, GL_COMPUTE_SHADER);
    glAttachShader(id_, cs);
    Link();
    glDeleteShader(cs);
}

Shader::~Shader() {
    if (id_) glDeleteProgram(id_);
}

void Shader::Use() const {
    glUseProgram(id_);
}

void Shader::Dispatch(GLuint x, GLuint y, GLuint z) const {
    glDispatchCompute(x, y, z);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Shader::SetBool(const std::string& n, bool v) const {
    glProgramUniform1i(id_, glGetUniformLocation(id_, n.c_str()), static_cast<int>(v));
}
void Shader::SetInt(const std::string& n, int v) const {
    glProgramUniform1i(id_, glGetUniformLocation(id_, n.c_str()), v);
}
void Shader::SetFloat(const std::string& n, float v) const {
    glProgramUniform1f(id_, glGetUniformLocation(id_, n.c_str()), v);
}
void Shader::SetVec2(const std::string& n, const glm::vec2& v) const {
    glProgramUniform2fv(id_, glGetUniformLocation(id_, n.c_str()), 1, glm::value_ptr(v));
}
void Shader::SetVec3(const std::string& n, const glm::vec3& v) const {
    glProgramUniform3fv(id_, glGetUniformLocation(id_, n.c_str()), 1, glm::value_ptr(v));
}
void Shader::SetVec4(const std::string& n, const glm::vec4& v) const {
    glProgramUniform4fv(id_, glGetUniformLocation(id_, n.c_str()), 1, glm::value_ptr(v));
}
void Shader::SetMat3(const std::string& n, const glm::mat3& m) const {
    glProgramUniformMatrix3fv(id_, glGetUniformLocation(id_, n.c_str()), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::SetMat4(const std::string& n, const glm::mat4& m) const {
    glProgramUniformMatrix4fv(id_, glGetUniformLocation(id_, n.c_str()), 1, GL_FALSE, glm::value_ptr(m));
}

GLuint Shader::Compile(const std::string& path, GLenum type) const {
    std::string src = ReadFile(path);
    const char* cstr = src.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error("[Shader] Compile error in " + path + ":\n" + log);
    }
    return shader;
}

std::string Shader::ReadFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("[Shader] Cannot open: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void Shader::Link() const {
    glLinkProgram(id_);
    GLint success = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
        throw std::runtime_error(std::string("[Shader] Link error:\n") + log);
    }
}

} // namespace HuanGL

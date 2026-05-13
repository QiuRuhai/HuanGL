# HuanGL Phase 1: Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将单文件 main.cpp 重构为模块化的 HuanGL 项目基础架构，升级至 OpenGL 4.6，建立 Window/Input/Renderer/Shader/Buffer/Texture/Framebuffer/App 八个核心类，最终产出：窗口正常打开、GL 4.6 context 可用、debug callback 运行、清屏渲染正常。

**Architecture:** 轻量分层架构（方案 C）。`App` 持有 `Window` 和主循环，`Renderer` 封装 OpenGL 状态和 debug callback，`Shader/Buffer/Texture/Framebuffer` 是 RAII 封装的 OpenGL 对象。所有 GL 错误通过 `GL_DEBUG_OUTPUT` debug callback 捕获，不在每个调用后手动 `glGetError`。

**Tech Stack:** C++17, OpenGL 4.6 Core Profile, GLFW 3.3+, GLAD (需重新生成支持 4.6), GLM, Assimp（暂保留 model 加载），Dear ImGui（Phase 3 接入，本阶段只链接）

---

## 前置检查

- [ ] 确认 GPU 驱动支持 OpenGL 4.6（NVIDIA 397+，AMD 18.x+，Intel Iris Xe+）
- [ ] 确认 GLAD 已生成 OpenGL 4.6 Core Profile 版本（当前 include/glad/glad.h 可能只有 3.3）

---

## Task 1: 重新生成 GLAD 并升级 CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `include/glad/glad.h`（替换为 4.6 版本）
- Modify: `src/glad.c`（替换为 4.6 版本）

- [ ] **Step 1: 生成 OpenGL 4.6 GLAD**

  访问 https://glad.dav1d.de/，参数：
  - Language: C/C++
  - Specification: OpenGL
  - API gl: Version 4.6
  - Profile: Core
  - Options: Generate a loader ✓

  下载后替换：
  - `include/glad/glad.h` ← 新文件
  - `src/glad.c` ← 新文件（重命名原 glad.c 为 glad.c.bak 备份）

- [ ] **Step 2: 更新 CMakeLists.txt**

  将以下内容完整替换 `CMakeLists.txt`：

  ```cmake
  cmake_minimum_required(VERSION 3.20)

  project(HuanGL LANGUAGES C CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  if(APPLE)
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
      add_compile_definitions(GL_SILENCE_DEPRECATION=1)
  endif()

  # 收集源文件
  file(GLOB_RECURSE SRC
      src/*.cpp
      src/*.c
  )

  add_executable(${PROJECT_NAME} ${SRC})

  target_include_directories(${PROJECT_NAME} PRIVATE
      ${CMAKE_SOURCE_DIR}/include
  )

  # OpenGL
  find_package(OpenGL REQUIRED)
  if(TARGET OpenGL::GL)
      target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
  else()
      if(APPLE)
          find_library(OPENGL_FRAMEWORK OpenGL)
          target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_FRAMEWORK})
      endif()
  endif()

  # GLFW
  find_package(glfw3 3.3 REQUIRED CONFIG)
  if(TARGET glfw)
      target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
  elseif(TARGET glfw::glfw)
      target_link_libraries(${PROJECT_NAME} PRIVATE glfw::glfw)
  endif()

  # Assimp
  find_package(assimp REQUIRED CONFIG)
  if(TARGET assimp::assimp)
      target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
  else()
      target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
  endif()

  if(APPLE)
      set_target_properties(${PROJECT_NAME} PROPERTIES
          BUILD_RPATH "/opt/homebrew/lib"
          INSTALL_RPATH "/opt/homebrew/lib"
      )
  endif()

  if(MSVC)
      target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
  else()
      target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SRC})
  ```

- [ ] **Step 3: 创建目录结构**

  ```
  src/core/
  src/renderer/
  src/pipeline/
  src/pipeline/passes/
  src/pipeline/gi/
  src/scene/
  src/resource/
  src/ui/
  shader/gbuffer/
  shader/shadow/
  shader/gi/rsm/
  shader/gi/ssgi/
  shader/gi/vxgi/
  shader/gi/ddgi/
  shader/lighting/
  shader/postprocess/bloom/
  shader/postprocess/taa/
  shader/postprocess/tonemapping/
  ```

  Windows PowerShell:
  ```powershell
  $dirs = @(
    "src/core","src/renderer","src/pipeline","src/pipeline/passes",
    "src/pipeline/gi","src/scene","src/resource","src/ui",
    "shader/gbuffer","shader/shadow",
    "shader/gi/rsm","shader/gi/ssgi","shader/gi/vxgi","shader/gi/ddgi",
    "shader/lighting",
    "shader/postprocess/bloom","shader/postprocess/taa","shader/postprocess/tonemapping"
  )
  foreach ($d in $dirs) { New-Item -ItemType Directory -Force $d }
  ```

- [ ] **Step 4: 验证构建（仅确认 CMake 不报错，暂时不跑程序）**

  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build
  ```

  预期：编译成功，无链接错误。若 GLAD 4.6 函数找不到，检查 glad.h 版本号是否为 4.6。

- [ ] **Step 5: Commit**

  ```bash
  git add CMakeLists.txt include/glad/ src/glad.c
  git commit -m "build: upgrade to OpenGL 4.6, rename project to HuanGL"
  ```

---

## Task 2: Window 类

**Files:**
- Create: `src/core/Window.h`
- Create: `src/core/Window.cpp`

- [ ] **Step 1: 创建 `src/core/Window.h`**

  ```cpp
  #pragma once
  #include <functional>
  #include <string>
  #include <glad/glad.h>
  #include <GLFW/glfw3.h>

  namespace HuanGL {

  class Window {
  public:
      Window(int width, int height, const std::string& title);
      ~Window();

      Window(const Window&) = delete;
      Window& operator=(const Window&) = delete;

      bool ShouldClose() const;
      void SwapBuffers() const;
      void PollEvents() const;

      int GetWidth() const  { return width_; }
      int GetHeight() const { return height_; }
      GLFWwindow* GetHandle() const { return handle_; }

      void SetResizeCallback(std::function<void(int, int)> cb);

  private:
      GLFWwindow* handle_ = nullptr;
      int width_, height_;
      std::function<void(int, int)> resizeCb_;

      static void FramebufferSizeCallback(GLFWwindow* w, int width, int height);
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/core/Window.cpp`**

  ```cpp
  #include "Window.h"
  #include <stdexcept>
  #include <iostream>

  namespace HuanGL {

  Window::Window(int width, int height, const std::string& title)
      : width_(width), height_(height)
  {
      if (!glfwInit())
          throw std::runtime_error("Failed to initialize GLFW");

      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
      glfwWindowHint(GLFW_SAMPLES, 1); // TAA 替代 MSAA
  #ifdef __APPLE__
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif
      // 开启 GL debug context
      glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

      handle_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
      if (!handle_) {
          glfwTerminate();
          throw std::runtime_error("Failed to create GLFW window");
      }

      glfwMakeContextCurrent(handle_);
      glfwSetWindowUserPointer(handle_, this);
      glfwSetFramebufferSizeCallback(handle_, FramebufferSizeCallback);

      if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
          throw std::runtime_error("Failed to initialize GLAD");

      // 打印实际 GL 版本，确认 4.6
      std::cout << "[HuanGL] OpenGL " << glGetString(GL_VERSION)
                << " | " << glGetString(GL_RENDERER) << "\n";
  }

  Window::~Window() {
      if (handle_) glfwDestroyWindow(handle_);
      glfwTerminate();
  }

  bool Window::ShouldClose() const { return glfwWindowShouldClose(handle_); }
  void Window::SwapBuffers() const { glfwSwapBuffers(handle_); }
  void Window::PollEvents() const  { glfwPollEvents(); }

  void Window::SetResizeCallback(std::function<void(int, int)> cb) {
      resizeCb_ = std::move(cb);
  }

  void Window::FramebufferSizeCallback(GLFWwindow* w, int width, int height) {
      auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
      self->width_  = width;
      self->height_ = height;
      if (self->resizeCb_) self->resizeCb_(width, height);
  }

  } // namespace HuanGL
  ```

- [ ] **Step 3: 在 main.cpp 临时测试 Window（替换旧 main.cpp 内容）**

  ```cpp
  #include "core/Window.h"
  #include <iostream>

  int main() {
      try {
          HuanGL::Window window(1280, 720, "HuanGL");
          while (!window.ShouldClose()) {
              glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
              glClear(GL_COLOR_BUFFER_BIT);
              window.SwapBuffers();
              window.PollEvents();
          }
      } catch (const std::exception& e) {
          std::cerr << "Fatal: " << e.what() << "\n";
          return -1;
      }
  }
  ```

- [ ] **Step 4: 构建并运行，验证窗口正常打开**

  ```bash
  cmake --build build && ./build/HuanGL
  ```

  预期：终端输出 `[HuanGL] OpenGL 4.6.x | <你的显卡名>`，窗口为深灰色背景。

- [ ] **Step 5: Commit**

  ```bash
  git add src/core/Window.h src/core/Window.cpp src/main.cpp
  git commit -m "feat: add Window class with OpenGL 4.6 context"
  ```

---

## Task 3: Input 类

**Files:**
- Create: `src/core/Input.h`
- Create: `src/core/Input.cpp`

- [ ] **Step 1: 创建 `src/core/Input.h`**

  ```cpp
  #pragma once
  #include <glm/glm.hpp>
  #include <GLFW/glfw3.h>

  namespace HuanGL {

  class Input {
  public:
      static void Init(GLFWwindow* window);
      static void Update(); // 每帧开始调用，重置 delta

      static bool IsKeyPressed(int glfwKey);
      static bool IsMouseButtonPressed(int glfwButton);
      static glm::vec2 GetMousePosition();
      static glm::vec2 GetMouseDelta();  // 当帧鼠标位移
      static float GetScrollDelta();     // 当帧滚轮量
      static void SetCursorCaptured(bool captured);

  private:
      static void MouseCallback(GLFWwindow*, double x, double y);
      static void ScrollCallback(GLFWwindow*, double, double y);

      static GLFWwindow* window_;
      static glm::vec2 mousePos_;
      static glm::vec2 mouseDelta_;
      static float scrollDelta_;
      static bool firstMouse_;
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/core/Input.cpp`**

  ```cpp
  #include "Input.h"
  #include <algorithm>

  namespace HuanGL {

  GLFWwindow* Input::window_      = nullptr;
  glm::vec2   Input::mousePos_    = {0, 0};
  glm::vec2   Input::mouseDelta_  = {0, 0};
  float       Input::scrollDelta_ = 0.0f;
  bool        Input::firstMouse_  = true;

  void Input::Init(GLFWwindow* window) {
      window_ = window;
      glfwSetCursorPosCallback(window, MouseCallback);
      glfwSetScrollCallback(window, ScrollCallback);
  }

  void Input::Update() {
      mouseDelta_ = {0, 0};
      scrollDelta_ = 0.0f;
  }

  bool Input::IsKeyPressed(int key) {
      return glfwGetKey(window_, key) == GLFW_PRESS;
  }

  bool Input::IsMouseButtonPressed(int button) {
      return glfwGetMouseButton(window_, button) == GLFW_PRESS;
  }

  glm::vec2 Input::GetMousePosition() { return mousePos_; }
  glm::vec2 Input::GetMouseDelta()    { return mouseDelta_; }
  float     Input::GetScrollDelta()   { return scrollDelta_; }

  void Input::SetCursorCaptured(bool captured) {
      glfwSetInputMode(window_, GLFW_CURSOR,
          captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
      firstMouse_ = true; // 防止捕获瞬间大跳
  }

  void Input::MouseCallback(GLFWwindow*, double x, double y) {
      glm::vec2 pos = {static_cast<float>(x), static_cast<float>(y)};
      if (firstMouse_) { mousePos_ = pos; firstMouse_ = false; }
      mouseDelta_ = pos - mousePos_;
      mouseDelta_.y = -mouseDelta_.y; // OpenGL y 轴朝上
      mousePos_   = pos;
  }

  void Input::ScrollCallback(GLFWwindow*, double, double y) {
      scrollDelta_ = static_cast<float>(y);
  }

  } // namespace HuanGL
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/core/Input.h src/core/Input.cpp
  git commit -m "feat: add Input class with mouse/keyboard/scroll"
  ```

---

## Task 4: Renderer（GL 状态封装 + Debug Callback）

**Files:**
- Create: `src/renderer/Renderer.h`
- Create: `src/renderer/Renderer.cpp`

- [ ] **Step 1: 创建 `src/renderer/Renderer.h`**

  ```cpp
  #pragma once
  #include <string>
  #include <glad/glad.h>

  namespace HuanGL {

  class Renderer {
  public:
      static void Init();   // 必须在 GLAD 初始化后、第一帧之前调用

      static void SetViewport(int x, int y, int width, int height);
      static void Clear(bool color = true, bool depth = true, bool stencil = false);
      static void SetClearColor(float r, float g, float b, float a = 1.0f);

      static void EnableDepthTest(bool enable);
      static void EnableDepthWrite(bool enable);
      static void SetDepthFunc(GLenum func);          // 默认 GL_LESS
      static void EnableBlend(bool enable);
      static void SetBlendFunc(GLenum src, GLenum dst);
      static void EnableCullFace(bool enable);
      static void SetCullFace(GLenum face);           // GL_BACK / GL_FRONT
      static void EnableSeamlessCubemap(bool enable);

      // GPU 调试标记（RenderDoc / NSight 可见）
      static void PushDebugGroup(const std::string& name);
      static void PopDebugGroup();

  private:
      static void APIENTRY DebugCallback(
          GLenum source, GLenum type, GLuint id, GLenum severity,
          GLsizei length, const GLchar* message, const void* userParam);
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/renderer/Renderer.cpp`**

  ```cpp
  #include "Renderer.h"
  #include <iostream>

  namespace HuanGL {

  void Renderer::Init() {
      // 开启 debug output（需要 GLFW_OPENGL_DEBUG_CONTEXT = true）
      glEnable(GL_DEBUG_OUTPUT);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback(DebugCallback, nullptr);
      // 过滤掉仅提示性消息（severity = NOTIFICATION）
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                            GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

      // 合理默认状态
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
  }

  void Renderer::SetViewport(int x, int y, int w, int h) { glViewport(x, y, w, h); }

  void Renderer::Clear(bool color, bool depth, bool stencil) {
      GLbitfield mask = 0;
      if (color)   mask |= GL_COLOR_BUFFER_BIT;
      if (depth)   mask |= GL_DEPTH_BUFFER_BIT;
      if (stencil) mask |= GL_STENCIL_BUFFER_BIT;
      glClear(mask);
  }

  void Renderer::SetClearColor(float r, float g, float b, float a) { glClearColor(r,g,b,a); }
  void Renderer::EnableDepthTest(bool e)   { e ? glEnable(GL_DEPTH_TEST)  : glDisable(GL_DEPTH_TEST); }
  void Renderer::EnableDepthWrite(bool e)  { glDepthMask(e ? GL_TRUE : GL_FALSE); }
  void Renderer::SetDepthFunc(GLenum f)    { glDepthFunc(f); }
  void Renderer::EnableBlend(bool e)       { e ? glEnable(GL_BLEND)       : glDisable(GL_BLEND); }
  void Renderer::SetBlendFunc(GLenum s, GLenum d) { glBlendFunc(s, d); }
  void Renderer::EnableCullFace(bool e)    { e ? glEnable(GL_CULL_FACE)   : glDisable(GL_CULL_FACE); }
  void Renderer::SetCullFace(GLenum f)     { glCullFace(f); }
  void Renderer::EnableSeamlessCubemap(bool e) {
      e ? glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS) : glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  }

  void Renderer::PushDebugGroup(const std::string& name) {
      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0,
                       static_cast<GLsizei>(name.size()), name.c_str());
  }
  void Renderer::PopDebugGroup() { glPopDebugGroup(); }

  void APIENTRY Renderer::DebugCallback(GLenum source, GLenum type, GLuint id,
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

  } // namespace HuanGL
  ```

- [ ] **Step 3: 在 main.cpp 中初始化 Renderer，验证 debug callback 触发**

  修改 main.cpp：
  ```cpp
  #include "core/Window.h"
  #include "renderer/Renderer.h"

  int main() {
      HuanGL::Window window(1280, 720, "HuanGL");
      HuanGL::Renderer::Init();

      // 故意触发一个 GL 错误来测试 debug callback
      glBindBuffer(GL_ARRAY_BUFFER, 99999); // 绑定不存在的 buffer

      while (!window.ShouldClose()) {
          HuanGL::Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
          HuanGL::Renderer::Clear();
          window.SwapBuffers();
          window.PollEvents();
      }
  }
  ```

  ```bash
  cmake --build build && ./build/HuanGL
  ```

  预期：终端输出 `[GL HIGH] (API/ERROR #...)` 的 debug 信息，证明 callback 正常工作。

- [ ] **Step 4: 移除故意错误，恢复正常 main.cpp**

  ```cpp
  // 删除 glBindBuffer(GL_ARRAY_BUFFER, 99999); 那行
  ```

- [ ] **Step 5: Commit**

  ```bash
  git add src/renderer/Renderer.h src/renderer/Renderer.cpp src/main.cpp
  git commit -m "feat: add Renderer with GL debug callback and state management"
  ```

---

## Task 5: Shader 类（升级支持 Compute Shader）

**Files:**
- Create: `src/renderer/Shader.h`（替换 `include/learnopengl/shader.h`）
- Create: `src/renderer/Shader.cpp`

- [ ] **Step 1: 创建 `src/renderer/Shader.h`**

  ```cpp
  #pragma once
  #include <string>
  #include <glad/glad.h>
  #include <glm/glm.hpp>

  namespace HuanGL {

  class Shader {
  public:
      // 顶点 + 片段 shader
      Shader(const std::string& vertPath, const std::string& fragPath);
      // 顶点 + 几何 + 片段 shader
      Shader(const std::string& vertPath, const std::string& geomPath,
             const std::string& fragPath);
      // Compute shader（OpenGL 4.3+）
      explicit Shader(const std::string& computePath);
      ~Shader();

      Shader(const Shader&) = delete;
      Shader& operator=(const Shader&) = delete;

      void Use() const;
      // Compute shader dispatch（需先 Use()）
      void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) const;

      // DSA 风格 uniform（glProgramUniform，无需先 bind）
      void SetBool(const std::string& name, bool v) const;
      void SetInt(const std::string& name, int v) const;
      void SetFloat(const std::string& name, float v) const;
      void SetVec2(const std::string& name, const glm::vec2& v) const;
      void SetVec3(const std::string& name, const glm::vec3& v) const;
      void SetVec4(const std::string& name, const glm::vec4& v) const;
      void SetMat3(const std::string& name, const glm::mat3& m) const;
      void SetMat4(const std::string& name, const glm::mat4& m) const;

      GLuint GetID() const { return id_; }

  private:
      GLuint id_ = 0;
      GLuint Compile(const std::string& path, GLenum type) const;
      std::string ReadFile(const std::string& path) const;
      void CheckLinkErrors() const;
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/renderer/Shader.cpp`**

  ```cpp
  #include "Shader.h"
  #include <fstream>
  #include <sstream>
  #include <iostream>
  #include <stdexcept>
  #include <glm/gtc/type_ptr.hpp>

  namespace HuanGL {

  Shader::Shader(const std::string& vert, const std::string& frag) {
      GLuint vs = Compile(vert, GL_VERTEX_SHADER);
      GLuint fs = Compile(frag, GL_FRAGMENT_SHADER);
      id_ = glCreateProgram();
      glAttachShader(id_, vs); glAttachShader(id_, fs);
      glLinkProgram(id_);
      CheckLinkErrors();
      glDeleteShader(vs); glDeleteShader(fs);
  }

  Shader::Shader(const std::string& vert, const std::string& geom, const std::string& frag) {
      GLuint vs = Compile(vert, GL_VERTEX_SHADER);
      GLuint gs = Compile(geom, GL_GEOMETRY_SHADER);
      GLuint fs = Compile(frag, GL_FRAGMENT_SHADER);
      id_ = glCreateProgram();
      glAttachShader(id_, vs); glAttachShader(id_, gs); glAttachShader(id_, fs);
      glLinkProgram(id_);
      CheckLinkErrors();
      glDeleteShader(vs); glDeleteShader(gs); glDeleteShader(fs);
  }

  Shader::Shader(const std::string& compute) {
      GLuint cs = Compile(compute, GL_COMPUTE_SHADER);
      id_ = glCreateProgram();
      glAttachShader(id_, cs);
      glLinkProgram(id_);
      CheckLinkErrors();
      glDeleteShader(cs);
  }

  Shader::~Shader() { if (id_) glDeleteProgram(id_); }

  void Shader::Use() const { glUseProgram(id_); }

  void Shader::Dispatch(uint32_t x, uint32_t y, uint32_t z) const {
      glDispatchCompute(x, y, z);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
  }

  // DSA 风格：glProgramUniform 不需要先 bind
  void Shader::SetBool(const std::string& n, bool v) const {
      glProgramUniform1i(id_, glGetUniformLocation(id_, n.c_str()), v);
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

      GLint success;
      glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
      if (!success) {
          char log[1024];
          glGetShaderInfoLog(shader, 1024, nullptr, log);
          throw std::runtime_error("[Shader] Compile error in " + path + ":\n" + log);
      }
      return shader;
  }

  std::string Shader::ReadFile(const std::string& path) const {
      std::ifstream file(path);
      if (!file.is_open())
          throw std::runtime_error("[Shader] Cannot open: " + path);
      std::stringstream ss;
      ss << file.rdbuf();
      return ss.str();
  }

  void Shader::CheckLinkErrors() const {
      GLint success;
      glGetProgramiv(id_, GL_LINK_STATUS, &success);
      if (!success) {
          char log[1024];
          glGetProgramInfoLog(id_, 1024, nullptr, log);
          throw std::runtime_error(std::string("[Shader] Link error:\n") + log);
      }
  }

  } // namespace HuanGL
  ```

- [ ] **Step 3: 添加测试 shader，验证 Shader 类可以编译并运行**

  创建 `shader/test/test.vert`：
  ```glsl
  #version 460 core
  layout(location = 0) in vec3 aPos;
  void main() { gl_Position = vec4(aPos, 1.0); }
  ```

  创建 `shader/test/test.frag`：
  ```glsl
  #version 460 core
  out vec4 FragColor;
  void main() { FragColor = vec4(1.0, 0.5, 0.2, 1.0); }
  ```

  修改 main.cpp 加载测试 shader：
  ```cpp
  #include "renderer/Shader.h"
  // ...
  HuanGL::Shader testShader("../shader/test/test.vert", "../shader/test/test.frag");
  std::cout << "[Test] Shader compiled OK, id=" << testShader.GetID() << "\n";
  ```

  ```bash
  cmake --build build && ./build/HuanGL
  ```

  预期：输出 `[Test] Shader compiled OK, id=1`，无 GL error。

- [ ] **Step 4: Commit**

  ```bash
  git add src/renderer/Shader.h src/renderer/Shader.cpp shader/test/
  git commit -m "feat: add Shader class with compute shader and DSA uniform support"
  ```

---

## Task 6: Buffer 类（VAO/VBO/UBO/SSBO）

**Files:**
- Create: `src/renderer/Buffer.h`
- Create: `src/renderer/Buffer.cpp`

- [ ] **Step 1: 创建 `src/renderer/Buffer.h`**

  ```cpp
  #pragma once
  #include <cstddef>
  #include <glad/glad.h>

  namespace HuanGL {

  // VAO
  class VertexArray {
  public:
      VertexArray();
      ~VertexArray();
      VertexArray(const VertexArray&) = delete;
      VertexArray& operator=(const VertexArray&) = delete;

      void Bind() const;
      void Unbind() const;
      // index: 属性 location；count: float 数量；stride/offset: 字节单位
      void AddAttribute(GLuint index, GLint count, GLenum type,
                        GLboolean normalized, GLsizei stride, size_t offset);
      GLuint GetID() const { return id_; }
  private:
      GLuint id_ = 0;
  };

  // 通用 Buffer（VBO, EBO, UBO, SSBO）
  class Buffer {
  public:
      explicit Buffer(GLenum target, GLenum usage = GL_STATIC_DRAW);
      ~Buffer();
      Buffer(const Buffer&) = delete;
      Buffer& operator=(const Buffer&) = delete;

      void Bind() const;
      void Unbind() const;
      void Upload(const void* data, size_t size);
      void UpdateSubData(const void* data, size_t size, size_t offset = 0);
      // UBO/SSBO 用：绑定到 binding point
      void BindBase(GLuint bindingPoint) const;

      GLuint GetID() const { return id_; }
      GLenum GetTarget() const { return target_; }

  private:
      GLuint id_     = 0;
      GLenum target_ = GL_ARRAY_BUFFER;
      GLenum usage_  = GL_STATIC_DRAW;
      size_t size_   = 0;
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/renderer/Buffer.cpp`**

  ```cpp
  #include "Buffer.h"
  #include <stdexcept>

  namespace HuanGL {

  // --- VertexArray ---
  VertexArray::VertexArray()  { glCreateVertexArrays(1, &id_); }  // DSA
  VertexArray::~VertexArray() { glDeleteVertexArrays(1, &id_); }
  void VertexArray::Bind()   const { glBindVertexArray(id_); }
  void VertexArray::Unbind() const { glBindVertexArray(0); }

  void VertexArray::AddAttribute(GLuint index, GLint count, GLenum type,
                                  GLboolean normalized, GLsizei stride, size_t offset) {
      glEnableVertexArrayAttrib(id_, index);                 // DSA
      glVertexArrayAttribFormat(id_, index, count, type, normalized, static_cast<GLuint>(offset));
      glVertexArrayAttribBinding(id_, index, 0);
  }

  // --- Buffer ---
  Buffer::Buffer(GLenum target, GLenum usage) : target_(target), usage_(usage) {
      glCreateBuffers(1, &id_); // DSA
  }
  Buffer::~Buffer() { glDeleteBuffers(1, &id_); }

  void Buffer::Bind()   const { glBindBuffer(target_, id_); }
  void Buffer::Unbind() const { glBindBuffer(target_, 0); }

  void Buffer::Upload(const void* data, size_t size) {
      size_ = size;
      glNamedBufferData(id_, static_cast<GLsizeiptr>(size), data, usage_); // DSA
  }

  void Buffer::UpdateSubData(const void* data, size_t size, size_t offset) {
      glNamedBufferSubData(id_, static_cast<GLintptr>(offset),
                           static_cast<GLsizeiptr>(size), data); // DSA
  }

  void Buffer::BindBase(GLuint bindingPoint) const {
      glBindBufferBase(target_, bindingPoint, id_);
  }

  } // namespace HuanGL
  ```

- [ ] **Step 3: 验证 Buffer + VertexArray（渲染一个三角形）**

  修改 main.cpp，加在 Shader 初始化后：
  ```cpp
  // 三角形顶点
  float verts[] = {
      -0.5f, -0.5f, 0.0f,
       0.5f, -0.5f, 0.0f,
       0.0f,  0.5f, 0.0f
  };
  HuanGL::VertexArray vao;
  HuanGL::Buffer vbo(GL_ARRAY_BUFFER);
  vbo.Upload(verts, sizeof(verts));

  // 用 DSA 绑定 VBO 到 VAO
  glVertexArrayVertexBuffer(vao.GetID(), 0, vbo.GetID(), 0, 3 * sizeof(float));
  vao.AddAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

  // render loop 中：
  testShader.Use();
  vao.Bind();
  glDrawArrays(GL_TRIANGLES, 0, 3);
  ```

  ```bash
  cmake --build build && ./build/HuanGL
  ```

  预期：窗口中央出现橙色三角形，无 GL error。

- [ ] **Step 4: Commit**

  ```bash
  git add src/renderer/Buffer.h src/renderer/Buffer.cpp
  git commit -m "feat: add VertexArray and Buffer classes with DSA API"
  ```

---

## Task 7: Texture 类

**Files:**
- Create: `src/renderer/Texture.h`
- Create: `src/renderer/Texture.cpp`

- [ ] **Step 1: 创建 `src/renderer/Texture.h`**

  ```cpp
  #pragma once
  #include <string>
  #include <memory>
  #include <glad/glad.h>

  namespace HuanGL {

  class Texture {
  public:
      // 从文件加载 2D 纹理（sRGB = true 时使用 GL_SRGB8_ALPHA8）
      static std::shared_ptr<Texture> Load2D(const std::string& path, bool sRGB = true);
      // 从文件加载 HDR float 纹理
      static std::shared_ptr<Texture> LoadHDR(const std::string& path);
      // 创建空 2D 纹理（用于 FBO attachment）
      static std::shared_ptr<Texture> Create2D(int w, int h,
          GLenum internalFmt, GLenum fmt, GLenum type);
      // 创建空 Cubemap
      static std::shared_ptr<Texture> CreateCubemap(int size,
          GLenum internalFmt, bool mipmap = false);
      // 创建空 3D 纹理（VXGI 用）
      static std::shared_ptr<Texture> Create3D(int w, int h, int d, GLenum internalFmt);

      ~Texture();
      Texture(const Texture&) = delete;
      Texture& operator=(const Texture&) = delete;

      // 绑定到 texture unit slot
      void Bind(GLuint slot = 0) const;
      // 绑定为 image unit（compute shader 读写）
      void BindImage(GLuint unit, GLenum access, GLenum fmt, int level = 0) const;
      void GenerateMipmaps() const;

      GLuint GetID() const       { return id_; }
      int GetWidth() const       { return width_; }
      int GetHeight() const      { return height_; }
      GLenum GetTarget() const   { return target_; }

  private:
      Texture() = default;
      GLuint id_      = 0;
      int    width_   = 0;
      int    height_  = 0;
      int    depth_   = 0;
      GLenum target_  = GL_TEXTURE_2D;
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/renderer/Texture.cpp`**

  ```cpp
  #include "Texture.h"
  #include <stb_image.h>
  #include <stdexcept>

  namespace HuanGL {

  std::shared_ptr<Texture> Texture::Load2D(const std::string& path, bool sRGB) {
      auto tex = std::shared_ptr<Texture>(new Texture());
      tex->target_ = GL_TEXTURE_2D;

      stbi_set_flip_vertically_on_load(true);
      int nrChannels;
      unsigned char* data = stbi_load(path.c_str(), &tex->width_, &tex->height_, &nrChannels, 0);
      if (!data) throw std::runtime_error("[Texture] Failed to load: " + path);

      GLenum intFmt = (nrChannels == 4) ? (sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8)
                                        : (sRGB ? GL_SRGB8        : GL_RGB8);
      GLenum fmt    = (nrChannels == 4) ? GL_RGBA : GL_RGB;

      glCreateTextures(GL_TEXTURE_2D, 1, &tex->id_); // DSA
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
      int nrChannels;
      float* data = stbi_loadf(path.c_str(), &tex->width_, &tex->height_, &nrChannels, 0);
      if (!data) throw std::runtime_error("[Texture] Failed to load HDR: " + path);

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

  std::shared_ptr<Texture> Texture::Create2D(int w, int h, GLenum intFmt, GLenum fmt, GLenum type) {
      auto tex = std::shared_ptr<Texture>(new Texture());
      tex->target_ = GL_TEXTURE_2D;
      tex->width_ = w; tex->height_ = h;

      glCreateTextures(GL_TEXTURE_2D, 1, &tex->id_);
      glTextureStorage2D(tex->id_, 1, intFmt, w, h);
      glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      (void)fmt; (void)type; // storage 版本不需要，保留参数备 FBO 用
      return tex;
  }

  std::shared_ptr<Texture> Texture::CreateCubemap(int size, GLenum intFmt, bool mipmap) {
      auto tex = std::shared_ptr<Texture>(new Texture());
      tex->target_ = GL_TEXTURE_CUBE_MAP;
      tex->width_ = tex->height_ = size;

      glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex->id_);
      int levels = mipmap ? (int)std::floor(std::log2(size)) + 1 : 1;
      glTextureStorage2D(tex->id_, levels, intFmt, size, size);
      glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER,
                          mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
      glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      return tex;
  }

  std::shared_ptr<Texture> Texture::Create3D(int w, int h, int d, GLenum intFmt) {
      auto tex = std::shared_ptr<Texture>(new Texture());
      tex->target_ = GL_TEXTURE_3D;
      tex->width_ = w; tex->height_ = h; tex->depth_ = d;

      glCreateTextures(GL_TEXTURE_3D, 1, &tex->id_);
      glTextureStorage3D(tex->id_, 1, intFmt, w, h, d);
      glTextureParameteri(tex->id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTextureParameteri(tex->id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTextureParameteri(tex->id_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      return tex;
  }

  Texture::~Texture() { if (id_) glDeleteTextures(1, &id_); }

  void Texture::Bind(GLuint slot) const {
      glBindTextureUnit(slot, id_); // DSA
  }

  void Texture::BindImage(GLuint unit, GLenum access, GLenum fmt, int level) const {
      glBindImageTexture(unit, id_, level, GL_TRUE, 0, access, fmt);
  }

  void Texture::GenerateMipmaps() const {
      glGenerateTextureMipmap(id_); // DSA
  }

  } // namespace HuanGL
  ```

- [ ] **Step 3: 验证 Texture::Load2D（加载现有资源中的一张纹理）**

  main.cpp 中临时添加：
  ```cpp
  auto tex = HuanGL::Texture::Load2D("../resources/objects/Cerberus/textures/08_-_Default_baseColor.png", true);
  std::cout << "[Test] Texture loaded: " << tex->GetWidth() << "x" << tex->GetHeight() << "\n";
  ```

  ```bash
  cmake --build build && ./build/HuanGL
  ```

  预期：打印纹理尺寸，无 GL error。

- [ ] **Step 4: Commit**

  ```bash
  git add src/renderer/Texture.h src/renderer/Texture.cpp
  git commit -m "feat: add Texture class with DSA support (2D/HDR/Cubemap/3D)"
  ```

---

## Task 8: Framebuffer 类

**Files:**
- Create: `src/renderer/Framebuffer.h`
- Create: `src/renderer/Framebuffer.cpp`

- [ ] **Step 1: 创建 `src/renderer/Framebuffer.h`**

  ```cpp
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

      // 附加颜色纹理到 attachment index（0-7）
      void AttachColor(std::shared_ptr<Texture> tex, GLuint attachment = 0, int mipLevel = 0);
      // 附加深度纹理（可采样）
      void AttachDepth(std::shared_ptr<Texture> tex);
      // 附加深度 Renderbuffer（仅深度测试，不采样）
      void AttachDepthRenderbuffer();
      // 设置 MRT draw buffers（GBuffer 用）
      void SetDrawBuffers(const std::vector<GLenum>& buffers);
      // 返回 false 并打印错误原因
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
  ```

- [ ] **Step 2: 创建 `src/renderer/Framebuffer.cpp`**

  ```cpp
  #include "Framebuffer.h"
  #include <stdexcept>
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
      // 扩展 colors_ 数组
      if (colors_.size() <= attachment) colors_.resize(attachment + 1);
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
  void Framebuffer::BindDefault()  { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

  std::shared_ptr<Texture> Framebuffer::GetColor(GLuint i) const {
      return (i < colors_.size()) ? colors_[i] : nullptr;
  }
  std::shared_ptr<Texture> Framebuffer::GetDepth() const { return depth_; }

  } // namespace HuanGL
  ```

- [ ] **Step 3: 验证 Framebuffer（渲染到离屏 FBO，再 blit 到屏幕）**

  main.cpp 临时测试：
  ```cpp
  // 创建离屏 FBO
  auto offscreen = std::make_unique<HuanGL::Framebuffer>(1280, 720);
  auto colorTex = HuanGL::Texture::Create2D(1280, 720, GL_RGBA16F, GL_RGBA, GL_FLOAT);
  offscreen->AttachColor(colorTex);
  offscreen->AttachDepthRenderbuffer();
  if (!offscreen->IsComplete()) return -1;

  // render loop 中：
  offscreen->Bind();
  HuanGL::Renderer::Clear();
  // ... 画三角形 ...
  HuanGL::Framebuffer::BindDefault();
  // blit 离屏到屏幕
  glBlitNamedFramebuffer(offscreen->GetID(), 0,
      0,0,1280,720, 0,0,1280,720, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  ```

  预期：三角形仍然显示，无 GL error，`IsComplete()` 返回 true。

- [ ] **Step 4: Commit**

  ```bash
  git add src/renderer/Framebuffer.h src/renderer/Framebuffer.cpp
  git commit -m "feat: add Framebuffer class with DSA API and MRT support"
  ```

---

## Task 9: UBO 封装 + 全局 UBO 定义

**Files:**
- Create: `src/renderer/UniformBuffer.h`（薄封装，便于 UBO 管理）
- Create: `shader/common/uniforms.glsl`（所有 shader 共享的 UBO 定义）

- [ ] **Step 1: 创建 `shader/common/uniforms.glsl`**

  ```glsl
  // 所有 shader 通过 #include 或复制此块引用
  // binding 点固定，不得修改

  layout(std140, binding = 0) uniform CameraUBO {
      mat4 view;
      mat4 proj;
      mat4 viewProj;
      mat4 invView;
      mat4 invProj;
      vec3 camPos;
      float near_;
      float far_;
      float pad[3];
  };

  layout(std140, binding = 1) uniform LightsUBO {
      // 方向光
      vec3  dirLightDir;
      float pad0;
      vec3  dirLightColor;
      float dirLightIntensity;
  };

  layout(std140, binding = 2) uniform TimeUBO {
      float time;
      float deltaTime;
      float pad[2];
  };
  ```

- [ ] **Step 2: 创建 `src/renderer/UniformBuffer.h`**

  ```cpp
  #pragma once
  #include "Buffer.h"
  #include <cstring>

  namespace HuanGL {

  // CameraUBO layout（与 uniforms.glsl 保持一致）
  struct CameraData {
      glm::mat4 view;
      glm::mat4 proj;
      glm::mat4 viewProj;
      glm::mat4 invView;
      glm::mat4 invProj;
      glm::vec3 camPos;
      float near_;
      float far_;
      float pad[3] = {};
  };

  struct LightsData {
      glm::vec3 dirLightDir;
      float pad0 = 0.f;
      glm::vec3 dirLightColor;
      float dirLightIntensity;
  };

  struct TimeData {
      float time;
      float deltaTime;
      float pad[2] = {};
  };

  // 通用 UBO 帮助类：自动绑定到指定 binding point
  template<typename T, GLuint BindingPoint>
  class UniformBuffer {
  public:
      UniformBuffer() : buffer_(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW) {
          buffer_.Upload(nullptr, sizeof(T));
          buffer_.BindBase(BindingPoint);
      }
      void Update(const T& data) {
          buffer_.UpdateSubData(&data, sizeof(T));
      }
  private:
      Buffer buffer_;
  };

  using CameraUBO = UniformBuffer<CameraData, 0>;
  using LightsUBO = UniformBuffer<LightsData, 1>;
  using TimeUBO   = UniformBuffer<TimeData,   2>;

  } // namespace HuanGL
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/renderer/UniformBuffer.h shader/common/uniforms.glsl
  git commit -m "feat: add UBO structs and shared GLSL uniform definitions"
  ```

---

## Task 10: App 类 + 最终 main.cpp

**Files:**
- Create: `src/core/App.h`
- Create: `src/core/App.cpp`
- Modify: `src/main.cpp`（最终版本，精简到 10 行以内）

- [ ] **Step 1: 创建 `src/core/App.h`**

  ```cpp
  #pragma once
  #include <memory>

  namespace HuanGL {

  class Window;

  class App {
  public:
      App();
      ~App();
      App(const App&) = delete;
      App& operator=(const App&) = delete;

      void Run();

  private:
      void Init();
      void Shutdown();
      void Update(float dt);
      void Render();

      std::unique_ptr<Window> window_;
      float lastTime_ = 0.0f;
      bool running_   = true;
  };

  } // namespace HuanGL
  ```

- [ ] **Step 2: 创建 `src/core/App.cpp`**

  ```cpp
  #include "App.h"
  #include "Window.h"
  #include "Input.h"
  #include "../renderer/Renderer.h"
  #include <glad/glad.h>
  #include <GLFW/glfw3.h>

  namespace HuanGL {

  App::App() { Init(); }
  App::~App() { Shutdown(); }

  void App::Init() {
      window_ = std::make_unique<Window>(1280, 720, "HuanGL");
      Input::Init(window_->GetHandle());
      Renderer::Init();
      Renderer::SetViewport(0, 0, window_->GetWidth(), window_->GetHeight());

      window_->SetResizeCallback([](int w, int h) {
          Renderer::SetViewport(0, 0, w, h);
      });
  }

  void App::Shutdown() {}

  void App::Run() {
      while (!window_->ShouldClose() && running_) {
          float now = static_cast<float>(glfwGetTime());
          float dt  = now - lastTime_;
          lastTime_ = now;

          Input::Update();
          window_->PollEvents();

          if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) running_ = false;

          Update(dt);
          Render();

          window_->SwapBuffers();
      }
  }

  void App::Update(float dt) {
      (void)dt;
      // Phase 3：SceneManager::ActiveScene→OnUpdate(dt)
  }

  void App::Render() {
      Renderer::SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
      Renderer::Clear();
      // Phase 3：SceneManager::ActiveScene→OnRender()
  }

  } // namespace HuanGL
  ```

- [ ] **Step 3: 最终 main.cpp（10 行）**

  ```cpp
  #include "core/App.h"
  #include <iostream>

  int main() {
      try {
          HuanGL::App app;
          app.Run();
      } catch (const std::exception& e) {
          std::cerr << "[Fatal] " << e.what() << "\n";
          return -1;
      }
      return 0;
  }
  ```

- [ ] **Step 4: 清理测试代码**

  删除 `shader/test/` 目录和 main.cpp 中的所有临时测试代码（三角形、纹理加载测试等）。

- [ ] **Step 5: 全量构建并验证**

  ```bash
  cmake --build build --clean-first && ./build/HuanGL
  ```

  预期：
  - 终端输出 `[HuanGL] OpenGL 4.6.x | <显卡名>`
  - 窗口打开，深色背景，无 GL debug 错误
  - ESC 键可退出
  - 整个 `src/` 目录下无任何全局变量（camera、deltaTime 等）残留

- [ ] **Step 6: 最终 Commit**

  ```bash
  git add src/ shader/
  git rm include/learnopengl/shader.h  # 已被 src/renderer/Shader.h 替代
  git commit -m "feat: complete Phase 1 foundation - Window/Input/Renderer/Shader/Buffer/Texture/Framebuffer/App"
  ```

---

## 验收标准

Phase 1 完成后，以下全部成立：

- [ ] 项目名为 HuanGL，CMakeLists.txt 使用 `project(HuanGL)`
- [ ] OpenGL 版本为 4.6 Core Profile（终端打印确认）
- [ ] GL debug callback 正常工作（任何 GL 错误会在终端打印 `[GL HIGH/MEDIUM/LOW]`）
- [ ] `src/main.cpp` ≤ 15 行
- [ ] 八个核心类均已实现：`Window`, `Input`, `Renderer`, `Shader`, `VertexArray`, `Buffer`, `Texture`, `Framebuffer`
- [ ] 所有 OpenGL 对象创建使用 DSA API（`glCreateTextures`, `glCreateBuffers`, `glCreateFramebuffers` 等）
- [ ] `src/` 目录下无全局变量
- [ ] 构建无警告（MSVC /W4 或 GCC -Wall -Wextra）

---

## 后续计划

- **Plan 2**：Render Pipeline（GBuffer + CSM/PCSS Shadow + PBR Lighting + ResourceManager）
- **Plan 3**：Scene System（SceneManager + ImGui + Sponza/Helmet 场景加载）
- **Plan 4**：Post-Processing（Bloom → TAA → ACES）
- **Plan 5–8**：GI 算法（RSM / SSGI / VXGI / DDGI）

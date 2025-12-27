cmake_minimum_required(VERSION 3.20)

# 工程
project(LearnOpenGL LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Apple Silicon: 强制 arm64（如你需要通用二进制可改为 "arm64;x86_64"）
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    # 静默 OpenGL 弃用告警
    add_compile_definitions(GL_SILENCE_DEPRECATION=1)
endif()

# 源码/头文件
set(SRC
        src/main.cpp
        src/glad.c          # GLAD 的 C 源文件
        src/stb_image.cpp   # 你自己的 stb_image 实现文件
)

add_executable(${PROJECT_NAME} ${SRC})

# 你的头文件路径（含 learnopengl/、glad/、stb/ 等）
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# -------- OpenGL --------
# 统一用 CMake 的 OpenGL imported target，跨平台自动映射
find_package(OpenGL REQUIRED)
if(TARGET OpenGL::GL)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
else()
    # 极少数旧环境兜底（macOS）
    if(APPLE)
        find_library(OPENGL_FRAMEWORK OpenGL)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_FRAMEWORK})
    endif()
endif()

# -------- GLFW --------
# Homebrew/vcpkg 都会安装 CMake config；名字通常是 glfw3
# 有时导出的 target 叫 glfw，有时叫 glfw::glfw，下面两者都兼容
find_package(glfw3 3.3 REQUIRED CONFIG)
if(TARGET glfw)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
elseif(TARGET glfw::glfw)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw::glfw)
else()
    message(FATAL_ERROR "Found glfw3 config but no target named glfw or glfw::glfw")
endif()

# -------- Assimp --------
# Homebrew/vcpkg 会提供 assimp::assimp
# 兼容 CONFIG/Module 两种查找模式
find_package(assimp REQUIRED CONFIG)
if(TARGET assimp::assimp)
    target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
else()
    # 兜底（很少用到）
    target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
endif()

# -------- 运行时 RPATH（macOS 常用）--------
# 这样可直接运行，不用手动设置 DYLD_LIBRARY_PATH
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "/opt/homebrew/lib"
            INSTALL_RPATH "/opt/homebrew/lib"
    )
endif()

# 更友好的警告等级（可选）
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

#（可选）在 Ninja/多目标时更清晰的分组
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SRC})

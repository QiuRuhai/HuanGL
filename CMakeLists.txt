cmake_minimum_required(VERSION 3.20)

project(HuanGL LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    add_compile_definitions(GL_SILENCE_DEPRECATION=1)
endif()

# Collect source files
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
else()
    message(FATAL_ERROR "Found glfw3 config but no target named glfw or glfw::glfw")
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

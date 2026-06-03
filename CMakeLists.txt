cmake_minimum_required(VERSION 3.28)

project(SkeletalAnimationRuntime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SkeletalAnimationRuntime
    src/main.cpp

    src/math/Transform.h

    src/animation/Skeleton.h
    src/animation/Skeleton.cpp

    src/render/Shader.h
    src/render/Shader.cpp
    src/render/Mesh.h
    src/render/Mesh.cpp
    src/render/DebugDraw.h
    src/render/DebugDraw.cpp

    src/assets/GltfLoader.h
    src/assets/GltfLoader.cpp
)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h" REQUIRED)

target_include_directories(SkeletalAnimationRuntime PRIVATE
    src
    ${TINYGLTF_INCLUDE_DIRS}
)

target_link_libraries(SkeletalAnimationRuntime PRIVATE
    glfw
    glad::glad
    glm::glm
    imgui::imgui
    nlohmann_json::nlohmann_json
)

if (MSVC)
    target_compile_options(SkeletalAnimationRuntime PRIVATE /W4 /permissive-)
else()
    target_compile_options(SkeletalAnimationRuntime PRIVATE -Wall -Wextra -Wpedantic)
endif()
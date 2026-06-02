#pragma once

#include <cstdint>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

struct StaticVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec2 texCoord{0.0f};
};

class Mesh
{
public:
    Mesh() = default;

    bool Create(
        const std::vector<StaticVertex> &vertices,
        const std::vector<std::uint32_t> &indices);

    void Draw() const;
    void Destroy();

    std::uint32_t GetIndexCount() const { return m_indexCount; }

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    std::uint32_t m_indexCount = 0;
};
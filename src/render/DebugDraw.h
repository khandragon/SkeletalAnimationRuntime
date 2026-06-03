#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "animation/Skeleton.h"
#include "render/Shader.h"

struct DebugVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
};

class DebugDraw
{
public:
    bool Create();
    void Destroy();

    void Begin();

    void AddLine(
        const glm::vec3 &start,
        const glm::vec3 &end,
        const glm::vec3 &color);

    void AddSkeleton(
        const Skeleton &skeleton,
        const std::vector<glm::mat4> &globalJointMatrices,
        const glm::vec3 &color);

    void Draw(const glm::mat4 &viewProjection);

private:
    Shader m_shader;

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    std::vector<DebugVertex> m_vertices;
};
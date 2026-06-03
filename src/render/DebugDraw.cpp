#include "render/DebugDraw.h"

#include <cstddef>
#include <iostream>

#include <glad/glad.h>

bool DebugDraw::Create()
{
    const char *vertexShaderSource = R"(
        #version 450 core

        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aColor;

        uniform mat4 uViewProjection;

        out vec3 vColor;

        void main()
        {
            vColor = aColor;
            gl_Position = uViewProjection * vec4(aPosition, 1.0);
        }
    )";

    const char *fragmentShaderSource = R"(
        #version 450 core

        in vec3 vColor;

        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(vColor, 1.0);
        }
    )";

    if (!m_shader.CreateFromSource(vertexShaderSource, fragmentShaderSource))
    {
        std::cerr << "Failed to create debug draw shader\n";
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        0,
        nullptr,
        GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugVertex),
        reinterpret_cast<void *>(offsetof(DebugVertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugVertex),
        reinterpret_cast<void *>(offsetof(DebugVertex, color)));

    glBindVertexArray(0);

    return true;
}

void DebugDraw::Destroy()
{
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_shader.Destroy();
    m_vertices.clear();
}

void DebugDraw::Begin()
{
    m_vertices.clear();
}

void DebugDraw::AddLine(
    const glm::vec3 &start,
    const glm::vec3 &end,
    const glm::vec3 &color)
{
    m_vertices.push_back(DebugVertex{start, color});
    m_vertices.push_back(DebugVertex{end, color});
}

void DebugDraw::AddSkeleton(
    const Skeleton &skeleton,
    const std::vector<glm::mat4> &globalJointMatrices,
    const glm::vec3 &color)
{
    if (skeleton.joints.size() != globalJointMatrices.size())
    {
        return;
    }

    for (std::size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
    {
        const Joint &joint = skeleton.joints[jointIndex];

        if (joint.parent < 0)
        {
            continue;
        }

        const glm::mat4 &childMatrix =
            globalJointMatrices[jointIndex];

        const glm::mat4 &parentMatrix =
            globalJointMatrices[static_cast<std::size_t>(joint.parent)];

        glm::vec3 childPosition =
            glm::vec3(childMatrix[3]);

        glm::vec3 parentPosition =
            glm::vec3(parentMatrix[3]);

        AddLine(parentPosition, childPosition, color);
    }
}

void DebugDraw::Draw(const glm::mat4 &viewProjection)
{
    if (m_vertices.empty())
    {
        return;
    }

    m_shader.Use();
    m_shader.SetMat4("uViewProjection", viewProjection);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(DebugVertex)),
        m_vertices.data(),
        GL_DYNAMIC_DRAW);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    glDrawArrays(
        GL_LINES,
        0,
        static_cast<GLsizei>(m_vertices.size()));

    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
}
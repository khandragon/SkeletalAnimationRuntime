#include "render/Mesh.h"

#include <cstddef>
#include <iostream>

bool Mesh::Create(
    const std::vector<StaticVertex> &vertices,
    const std::vector<std::uint32_t> &indices)
{
    if (vertices.empty())
    {
        std::cerr << "Mesh creation failed: no vertices\n";
        return false;
    }

    if (indices.empty())
    {
        std::cerr << "Mesh creation failed: no indices\n";
        return false;
    }

    m_indexCount = static_cast<std::uint32_t>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(StaticVertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

    // Attribute 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StaticVertex),
        reinterpret_cast<void *>(offsetof(StaticVertex, position)));

    // Attribute 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StaticVertex),
        reinterpret_cast<void *>(offsetof(StaticVertex, normal)));

    // Attribute 2: texCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StaticVertex),
        reinterpret_cast<void *>(offsetof(StaticVertex, texCoord)));

    glBindVertexArray(0);

    return true;
}

bool Mesh::CreateSkinned(
    const std::vector<SkinnedVertex> &vertices,
    const std::vector<std::uint32_t> &indices)
{
    if (vertices.empty())
    {
        std::cerr << "Skinned mesh creation failed: no vertices\n";
        return false;
    }

    if (indices.empty())
    {
        std::cerr << "Skinned mesh creation failed: no indices\n";
        return false;
    }

    m_indexCount = static_cast<std::uint32_t>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(SkinnedVertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

    // location 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkinnedVertex),
        reinterpret_cast<void *>(offsetof(SkinnedVertex, position)));

    // location 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkinnedVertex),
        reinterpret_cast<void *>(offsetof(SkinnedVertex, normal)));

    // location 2: uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkinnedVertex),
        reinterpret_cast<void *>(offsetof(SkinnedVertex, uv)));

    // location 3: joint indices
    //
    // IMPORTANT:
    // Because this is an integer attribute in the shader, use
    // glVertexAttribIPointer, not glVertexAttribPointer.
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(
        3,
        4,
        GL_UNSIGNED_INT,
        sizeof(SkinnedVertex),
        reinterpret_cast<void *>(offsetof(SkinnedVertex, joints)));

    // location 4: joint weights
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkinnedVertex),
        reinterpret_cast<void *>(offsetof(SkinnedVertex, weights)));

    glBindVertexArray(0);

    return true;
}

void Mesh::Draw() const
{
    glBindVertexArray(m_vao);

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(m_indexCount),
        GL_UNSIGNED_INT,
        nullptr);

    glBindVertexArray(0);
}

void Mesh::Destroy()
{
    if (m_ebo != 0)
    {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }

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

    m_indexCount = 0;
}
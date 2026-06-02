#pragma once

#include <string>

#include <glad/glad.h>
#include <glm/mat4x4.hpp>

class Shader
{
public:
    Shader() = default;

    bool CreateFromSource(const char *vertexSource, const char *fragmentSource);
    void Use() const;
    void Destroy();

    void SetMat4(const std::string &name, const glm::mat4 &value) const;

    GLuint GetProgram() const { return m_program; }

private:
    GLuint m_program = 0;

    static bool CheckShaderCompile(GLuint shader, const std::string &name);
    static bool CheckProgramLink(GLuint program);
};
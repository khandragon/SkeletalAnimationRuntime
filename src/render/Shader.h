#pragma once

#include <string>
#include <vector>

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

    void SetMat4Array(
        const std::string &name,
        const std::vector<glm::mat4> &values) const;

    GLint GetUniformLocation(const std::string &name) const;

    void SetMat4(
        GLint location,
        const glm::mat4 &value) const;

    void SetMat4Array(
        GLint location,
        const std::vector<glm::mat4> &values) const;
        
    GLuint GetProgram() const { return m_program; }

private:
    GLuint m_program = 0;

    static bool CheckShaderCompile(GLuint shader, const std::string &name);
    static bool CheckProgramLink(GLuint program);
};
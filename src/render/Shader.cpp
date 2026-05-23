#include "Shader.h"

#include <iostream>
#include <vector>

bool Shader::CreateFromSource(const char *vertexSource, const char *fragmentSource)
{
    // Creating and compiling vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    if (!CheckShaderCompile(vertexShader, "Vertex Shader"))
    {
        glDeleteShader(vertexShader);
        return false;
    }

    // Creating and compiling fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    if (!CheckShaderCompile(fragmentShader, "Fragment Shader"))
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Creating shader program and linking shaders
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);

    // Deleting shader objects after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!CheckProgramLink(m_program))
    {
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    return true;
}

// Activating the shader program for rendering
void Shader::Use() const
{
    glUseProgram(m_program);
}

// Deleting the shader program when it's no longer needed
void Shader::Destroy()
{
    if (m_program != 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

bool Shader::CheckShaderCompile(GLuint shader, const std::string &name)
{
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success == GL_TRUE)
    {
        return true;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    std::vector<char> log(static_cast<size_t>(logLength));
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());

    std::cerr << name << " compile error:\n"
              << log.data() << '\n';
    return false;
}

bool Shader::CheckProgramLink(GLuint program)
{
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success == GL_TRUE)
    {
        return true;
    }

    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    std::vector<char> log(static_cast<size_t>(logLength));
    glGetProgramInfoLog(program, logLength, nullptr, log.data());

    std::cerr << "Shader program link error:\n"
              << log.data() << '\n';
    return false;
}
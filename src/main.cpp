#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "render/Shader.h"

static void GlfwErrorCallback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error << ": " << description << '\n';
}

int main()
{
    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    // Ask OpenGL for a modern core-profile context.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "Data-Driven Skeletal Animation Runtime",
        nullptr,
        nullptr
    );

    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
   if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    const char* vertexShaderSource = R"(
        #version 450 core

        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aColor;

        out vec3 vColor;

        void main()
        {
            vColor = aColor;
            gl_Position = vec4(aPosition, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 450 core

        in vec3 vColor;
        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(vColor, 1.0);
        }
    )";

    Shader triangleShader;

    if (!triangleShader.CreateFromSource(vertexShaderSource, fragmentShaderSource))
    {
        std::cerr << "Failed to create triangle shader\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    float vertices[] =
    {
        // position            // color
         0.0f,  0.5f, 0.0f,    1.0f, 0.2f, 0.2f,
        -0.5f, -0.5f, 0.0f,    0.2f, 1.0f, 0.2f,
         0.5f, -0.5f, 0.0f,    0.2f, 0.4f, 1.0f
    };

    // These indices say:
    // draw a triangle using vertex 0, then vertex 1, then vertex 2.
    unsigned int indices[] =
    {
        0, 1, 2
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    // Bind the VAO first.
    // The VAO remembers the vertex layout setup below.
    glBindVertexArray(vao);

    // Upload vertex data into the VBO.
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    // Upload index data into the EBO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(indices),
        indices,
        GL_STATIC_DRAW
    );

    // Vertex memory layout:
    //
    // float 0: position.x
    // float 1: position.y
    // float 2: position.z
    // float 3: color.r
    // float 4: color.g
    // float 5: color.b
    //
    // One full vertex = 6 floats.

    // Attribute 0: position
    // Matches this in the vertex shader:
    // layout(location = 0) in vec3 aPosition;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,                            // attribute location
        3,                            // vec3 = 3 floats
        GL_FLOAT,                     // data type
        GL_FALSE,                     // do not normalize
        6 * sizeof(float),            // stride: size of one full vertex
        reinterpret_cast<void*>(0)    // offset: position starts at float 0
    );

    // Attribute 1: color
    // Matches this in the vertex shader:
    // layout(location = 1) in vec3 aColor;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,                                         // attribute location
        3,                                         // vec3 = 3 floats
        GL_FLOAT,                                  // data type
        GL_FALSE,                                  // do not normalize
        6 * sizeof(float),                         // stride: size of one full vertex
        reinterpret_cast<void*>(3 * sizeof(float)) // offset: color starts after position
    );

    // Unbind the VAO so accidental state changes are less likely.
    glBindVertexArray(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    while (!glfwWindowShouldClose(window))
    {
        // Process keyboard/mouse/window events.
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Start a new ImGui frame.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Animation Runtime Debug");
        ImGui::Text("Milestone 2");
        ImGui::Separator();
        ImGui::Text("Goal: Draw one triangle");
        ImGui::Text("Shader: working");
        ImGui::Text("VAO: working");
        ImGui::Text("VBO: working");
        ImGui::Text("EBO: working");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
        ImGui::End();

        ImGui::Render();

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        // Clear the screen.
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw the triangle
        triangleShader.Use();

        glBindVertexArray(vao);

        glDrawElements(
            GL_TRIANGLES,      // draw triangles
            3,                 // use 3 indices
            GL_UNSIGNED_INT,   // index type
            nullptr            // start at beginning of EBO
        );

        glBindVertexArray(0);

        // Draw ImGui last so it appears on top.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    triangleShader.Destroy();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
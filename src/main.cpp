#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "assets/GltfLoader.h"

#include "animation/AnimationClip.h"
#include "animation/AnimationGraph.h"
#include "animation/Animator.h"
#include "animation/Pose.h"
#include "animation/Skeleton.h"

#include "render/DebugDraw.h"
#include "render/Mesh.h"
#include "render/Shader.h"

namespace
{
    struct MeshDisplaySettings
    {
        glm::vec3 center{0.0f};
        float scale = 1.0f;
    };

    static void GlfwErrorCallback(int error, const char *description)
    {
        std::cerr << "GLFW Error " << error << ": " << description << '\n';
    }

    MeshDisplaySettings ComputeMeshDisplaySettings(
        const std::vector<SkinnedVertex> &vertices)
    {
        MeshDisplaySettings settings{};

        if (vertices.empty())
        {
            return settings;
        }

        glm::vec3 meshMin(std::numeric_limits<float>::max());
        glm::vec3 meshMax(-std::numeric_limits<float>::max());

        for (const SkinnedVertex &vertex : vertices)
        {
            meshMin = glm::min(meshMin, vertex.position);
            meshMax = glm::max(meshMax, vertex.position);
        }

        const glm::vec3 meshSize = meshMax - meshMin;
        const float largestAxis =
            glm::max(meshSize.x, glm::max(meshSize.y, meshSize.z));

        settings.center = (meshMin + meshMax) * 0.5f;

        if (largestAxis > 0.0f)
        {
            settings.scale = 2.0f / largestAxis;
        }

        return settings;
    }

    void UpdateJointMatrices(
        const Skeleton &skeleton,
        const Pose &pose,
        std::vector<glm::mat4> &jointMatrices)
    {
        jointMatrices.resize(skeleton.joints.size());

        for (std::size_t jointIndex = 0;
             jointIndex < skeleton.joints.size();
             ++jointIndex)
        {
            jointMatrices[jointIndex] =
                pose.global[jointIndex] *
                skeleton.joints[jointIndex].inverseBindMatrix;
        }
    }

    const char *GetSkinnedVertexShaderSource()
    {
        return R"(
            #version 450 core

            const int MAX_JOINTS = 128;

            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTexCoord;
            layout(location = 3) in uvec4 aJoints;
            layout(location = 4) in vec4 aWeights;

            uniform mat4 uMVP;
            uniform mat4 uJointMatrices[MAX_JOINTS];

            out vec3 vNormal;
            out vec2 vTexCoord;

            void main()
            {
                mat4 skin =
                    aWeights.x * uJointMatrices[aJoints.x] +
                    aWeights.y * uJointMatrices[aJoints.y] +
                    aWeights.z * uJointMatrices[aJoints.z] +
                    aWeights.w * uJointMatrices[aJoints.w];

                vec4 skinnedPosition =
                    skin * vec4(aPosition, 1.0);

                vec3 skinnedNormal =
                    mat3(skin) * aNormal;

                vNormal = skinnedNormal;
                vTexCoord = aTexCoord;

                gl_Position = uMVP * skinnedPosition;
            }
        )";
    }

    const char *GetFragmentShaderSource()
    {
        return R"(
            #version 450 core

            in vec3 vNormal;
            in vec2 vTexCoord;

            out vec4 FragColor;

            void main()
            {
                vec3 normalColor = normalize(vNormal) * 0.5 + 0.5;
                FragColor = vec4(normalColor, 1.0);
            }
        )";
    }
}

int main()
{
    bool showSkeleton = true;

    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(
        1280,
        720,
        "Data-Driven Skeletal Animation Runtime",
        nullptr,
        nullptr);

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

    Shader meshShader;

    if (!meshShader.CreateFromSource(
            GetSkinnedVertexShaderSource(),
            GetFragmentShaderSource()))
    {
        std::cerr << "Failed to create mesh shader\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const std::string graphPath = "assets/graphs/fox_anim_graph.json";

    AnimationGraph animationGraph = LoadAnimationGraph(graphPath);

    if (animationGraph.model.empty())
    {
        std::cerr << "Animation graph has no model path.\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const std::string modelPath = animationGraph.model;

    SkinnedMeshData meshData;

    if (!GltfLoader::LoadFirstSkinnedMesh(modelPath, meshData))
    {
        std::cerr << "Failed to load skinned model: " << modelPath << '\n';
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const MeshDisplaySettings meshDisplay =
        ComputeMeshDisplaySettings(meshData.vertices);

    Skeleton skeleton;

    if (!GltfLoader::LoadFirstSkeleton(modelPath, skeleton))
    {
        std::cerr << "Failed to load skeleton from model.\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    PrintSkeletonHierarchy(skeleton);

    std::vector<AnimationClip> animationClips;

    if (!GltfLoader::LoadAnimationClips(modelPath, animationClips))
    {
        std::cerr << "Failed to load animation clips from model.\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    PrintAnimationClips(animationClips);

    if (!ResolveAnimationGraphClipIndices(animationGraph, animationClips))
    {
        std::cerr << "Failed to resolve animation graph clip names.\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    Animator animator;
    animator.Initialize(&skeleton, &animationClips);

    const AnimationGraphState *initialState =
        FindAnimationGraphState(animationGraph, animationGraph.initialState);

    if (initialState == nullptr)
    {
        std::cerr << "Initial animation graph state not found.\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    animator.Play(initialState->clipIndex);
    animationGraph.currentState = initialState->name;

    std::vector<glm::mat4> jointMatrices(
        skeleton.joints.size(),
        glm::mat4(1.0f));

    Mesh mesh;

    if (!mesh.CreateSkinned(meshData.vertices, meshData.indices))
    {
        std::cerr << "Failed to create OpenGL skinned mesh\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    DebugDraw debugDraw;

    if (!debugDraw.Create())
    {
        std::cerr << "Failed to create DebugDraw\n";
        mesh.Destroy();
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    bool key1WasDown = false;
    bool key2WasDown = false;
    bool key3WasDown = false;

    double previousTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        const double currentTime = glfwGetTime();
        const float deltaTime =
            static_cast<float>(currentTime - previousTime);

        previousTime = currentTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const bool key1IsDown =
            glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;

        const bool key2IsDown =
            glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;

        const bool key3IsDown =
            glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;

        if (key1IsDown && !key1WasDown)
        {
            SetAnimationGraphParameter(animationGraph, "speed", 0.0f);
        }

        if (key2IsDown && !key2WasDown)
        {
            SetAnimationGraphParameter(animationGraph, "speed", 1.0f);
        }

        if (key3IsDown && !key3WasDown)
        {
            SetAnimationGraphParameter(animationGraph, "speed", 4.0f);
        }

        key1WasDown = key1IsDown;
        key2WasDown = key2IsDown;
        key3WasDown = key3IsDown;

        if (!animator.IsBlending())
        {
            const bool stateFinished = false;

            const AnimationGraphTransition *transition =
                FindTriggeredTransition(animationGraph, stateFinished);

            if (transition != nullptr)
            {
                const AnimationGraphState *targetState =
                    FindAnimationGraphState(animationGraph, transition->to);

                if (targetState != nullptr)
                {
                    animator.CrossFadeTo(
                        targetState->clipIndex,
                        transition->blendTime);

                    animationGraph.currentState = targetState->name;
                }
            }
        }

        animator.Update(deltaTime);

        UpdateJointMatrices(
            skeleton,
            animator.GetPose(),
            jointMatrices);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Animation Runtime Debug");

        ImGui::Text("Milestone 11");
        ImGui::Separator();
        ImGui::Text("Goal: JSON animation graph");
        ImGui::Text("Graph: %s", graphPath.c_str());
        ImGui::Text("Model: %s", modelPath.c_str());
        ImGui::Text("State: %s", animationGraph.currentState.c_str());
        ImGui::Text(
            "Speed: %.2f",
            GetAnimationGraphParameter(animationGraph, "speed"));

        ImGui::Separator();
        ImGui::Text("Vertices: %zu", meshData.vertices.size());
        ImGui::Text("Indices: %zu", meshData.indices.size());
        ImGui::Text("Joints: %zu", skeleton.joints.size());
        ImGui::Text("Animation clips: %zu", animationClips.size());

        const AnimationClip *currentClip = animator.GetCurrentClip();

        if (currentClip != nullptr)
        {
            ImGui::Separator();
            ImGui::Text("Current clip: %s", currentClip->name.c_str());
            ImGui::Text(
                "Animation time: %.3f / %.3f",
                animator.GetCurrentTime(),
                currentClip->duration);
        }

        if (animator.IsBlending())
        {
            const AnimationClip *previousClip =
                animator.GetPreviousClip();

            if (previousClip != nullptr)
            {
                ImGui::Text("Blending from: %s", previousClip->name.c_str());
            }

            ImGui::Text("Blend weight: %.2f", animator.GetBlendWeight());
            ImGui::Text(
                "Blend time: %.3f / %.3f",
                animator.GetBlendElapsed(),
                animator.GetBlendDuration());
        }
        else
        {
            ImGui::Text("Blending: no");
        }

        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("1 = speed 0.0");
        ImGui::Text("2 = speed 1.0");
        ImGui::Text("3 = speed 4.0");
        ImGui::Checkbox("Show skeleton", &showSkeleton);

        const float frameTimeMs =
            io.Framerate > 0.0f
                ? 1000.0f / io.Framerate
                : 0.0f;

        ImGui::Separator();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame time: %.3f ms", frameTimeMs);

        ImGui::End();

        ImGui::Render();

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(
            window,
            &framebufferWidth,
            &framebufferHeight);

        const float aspect =
            framebufferHeight > 0
                ? static_cast<float>(framebufferWidth) /
                      static_cast<float>(framebufferHeight)
                : 1.0f;

        glm::mat4 model = glm::mat4(1.0f);

        model = glm::rotate(
            model,
            static_cast<float>(glfwGetTime()) * 0.5f,
            glm::vec3(0.0f, 1.0f, 0.0f));

        model = glm::scale(
            model,
            glm::vec3(meshDisplay.scale));

        model = glm::translate(
            model,
            -meshDisplay.center);

        const glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 1.5f, 5.0f),
            glm::vec3(0.0f, 0.5f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat4 projection = glm::perspective(
            glm::radians(60.0f),
            aspect,
            0.1f,
            100.0f);

        const glm::mat4 mvp = projection * view * model;

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        meshShader.Use();
        meshShader.SetMat4("uMVP", mvp);
        meshShader.SetMat4Array("uJointMatrices[0]", jointMatrices);
        mesh.Draw();

        if (showSkeleton)
        {
            debugDraw.Begin();

            debugDraw.AddSkeleton(
                skeleton,
                animator.GetPose().global,
                glm::vec3(1.0f, 1.0f, 0.0f));

            debugDraw.Draw(mvp);
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    debugDraw.Destroy();
    mesh.Destroy();
    meshShader.Destroy();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
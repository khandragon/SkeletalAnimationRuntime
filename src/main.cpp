#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <thread>

#include "core/JobSystem.h"
#include "core/Timer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "assets/GltfLoader.h"

#include "animation/AnimatedCharacter.h"
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
    struct LoadingProfile
    {
        double graphLoadMs = 0.0;
        double skinnedMeshLoadMs = 0.0;
        double skeletonLoadMs = 0.0;
        double animationClipLoadMs = 0.0;
        double gltfTotalLoadMs = 0.0;
    };
    struct FrameProfile
    {
        double jointMatrixGenerationMs = 0.0;
        double jointMatrixUploadMs = 0.0;
        double renderMs = 0.0;
        double fullFrameMs = 0.0;
    };
    enum class CharacterAnimationMode
    {
        SameClip,
        RandomClip,
        RandomPhase
    };
    enum class SkinningMode
    {
        GPU,
        CPU
    };
    struct CrowdProfile
    {
        double animationUpdateMs = 0.0;
        double jointMatrixGenerationMs = 0.0;
        double renderMs = 0.0;
        double fullFrameMs = 0.0;

        double averageFrameMs = 0.0;
        double charactersUpdatedPerSecond = 0.0;

        std::size_t estimatedMemoryBytes = 0;
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
        const std::size_t jointCount = skeleton.joints.size();

        if (jointMatrices.size() != jointCount ||
            pose.global.size() != jointCount)
        {
            std::cerr
                << "UpdateJointMatrices failed: incorrect pose or joint matrix size.\n";

            return;
        }

        for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
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

    glm::mat4 ComputeCharacterWorldTransform(
        int index,
        int characterCount)
    {
        const int columns =
            static_cast<int>(std::ceil(std::sqrt(static_cast<float>(characterCount))));

        const int row = index / columns;
        const int column = index % columns;

        const float spacing = 2.5f;

        const float centerOffset =
            static_cast<float>(columns - 1) * spacing * 0.5f;

        const float x =
            static_cast<float>(column) * spacing - centerOffset;

        const float z =
            static_cast<float>(row) * spacing - centerOffset;

        return glm::translate(
            glm::mat4(1.0f),
            glm::vec3(x, 0.0f, z));
    }

    std::size_t EstimateCrowdMemoryBytes(
        std::size_t characterCount,
        std::size_t jointCount)
    {
        // Rough estimate:
        // Each character owns:
        // - Animator pose data internally
        // - jointMatrices vector externally
        //
        // Animator contains 3 poses:
        // m_pose, m_fromPose, m_toPose
        //
        // Each Pose has:
        // translations + rotations + scales + global matrices
        //
        // This is an estimate, not exact allocator-level memory.

        const std::size_t vec3Bytes =
            sizeof(glm::vec3);

        const std::size_t quatBytes =
            sizeof(glm::quat);

        const std::size_t mat4Bytes =
            sizeof(glm::mat4);

        const std::size_t poseBytes =
            jointCount * (vec3Bytes + // translations
                          quatBytes + // rotations
                          vec3Bytes + // scales
                          mat4Bytes   // global matrices
                         );

        const std::size_t animatorPoseBytes =
            3 * poseBytes;

        const std::size_t jointMatrixBytes =
            jointCount * mat4Bytes;

        const std::size_t perCharacterBytes =
            sizeof(AnimatedCharacter) +
            animatorPoseBytes +
            jointMatrixBytes;

        return characterCount * perCharacterBytes;
    }

    void RebuildCharacters(
        std::vector<AnimatedCharacter> &characters,
        int characterCount,
        CharacterAnimationMode mode,
        const Skeleton &skeleton,
        const std::vector<AnimationClip> &animationClips,
        std::size_t defaultClipIndex)
    {
        characters.clear();
        characters.resize(static_cast<std::size_t>(characterCount));

        std::mt19937 randomEngine(1337);
        std::uniform_real_distribution<float> phaseDistribution(0.0f, 1.0f);

        const int clipCount =
            static_cast<int>(animationClips.size());

        std::uniform_int_distribution<int> clipDistribution(
            0,
            std::max(clipCount - 1, 0));

        for (int i = 0; i < characterCount; ++i)
        {
            AnimatedCharacter &character =
                characters[static_cast<std::size_t>(i)];

            character.animator.Initialize(&skeleton, &animationClips);
            character.worldTransform =
                ComputeCharacterWorldTransform(i, characterCount);

            character.jointMatrices.resize(
                skeleton.joints.size(),
                glm::mat4(1.0f));

            std::size_t clipIndex = defaultClipIndex;

            if (mode == CharacterAnimationMode::RandomClip && !animationClips.empty())
            {
                clipIndex = static_cast<std::size_t>(clipDistribution(randomEngine));
            }

            character.clipIndex = static_cast<int>(clipIndex);
            character.animator.Play(clipIndex);

            if (mode == CharacterAnimationMode::RandomPhase)
            {
                const float phase =
                    phaseDistribution(randomEngine);

                character.animator.SetNormalizedTime(phase);
            }
        }
    }

    void UpdateCharacterAnimation(
        AnimatedCharacter &character,
        float deltaTime)
    {
        character.animator.Update(deltaTime);
    }

    void GenerateCharacterJointMatrices(
        AnimatedCharacter &character,
        const Skeleton &skeleton)
    {
        UpdateJointMatrices(
            skeleton,
            character.animator.GetPose(),
            character.jointMatrices);
    }
}

int main()
{
    bool showMesh = true;
    bool showSkeleton = true;
    bool showJointMarkers = true;
    bool showRootMotionPath = true;
    bool showForwardVector = true;
    bool showBindPose = false;

    int selectedJoint = 0;

    int characterCount = 1;
    int requestedCharacterCount = 1;

    bool useMultithreadedAnimation = true;

    LoadingProfile loadingProfile;
    FrameProfile frameProfile;

    CharacterAnimationMode characterAnimationMode =
        CharacterAnimationMode::SameClip;

    SkinningMode skinningMode =
        SkinningMode::GPU;

    CrowdProfile crowdProfile;
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

    const GLint uMvpLocation =
        meshShader.GetUniformLocation("uMVP");

    const GLint uJointMatricesLocation =
        meshShader.GetUniformLocation("uJointMatrices[0]");

    if (uMvpLocation == -1)
    {
        std::cerr << "Warning: uMVP uniform not found.\n";
    }

    if (uJointMatricesLocation == -1)
    {
        std::cerr << "Warning: uJointMatrices uniform not found.\n";
    }

    const std::string graphPath = "assets/graphs/fox_anim_graph.json";

    AnimationGraph animationGraph;

    {
        ScopedTimer timer("Animation graph loading", &loadingProfile.graphLoadMs);
        animationGraph = LoadAnimationGraph(graphPath);
    }

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

    {
        ScopedTimer timer("Skinned mesh loading", &loadingProfile.skinnedMeshLoadMs);

        if (!GltfLoader::LoadFirstSkinnedMesh(modelPath, meshData))
        {
            std::cerr << "Failed to load skinned model: " << modelPath << '\n';
            meshShader.Destroy();
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
    }

    const MeshDisplaySettings meshDisplay =
        ComputeMeshDisplaySettings(meshData.vertices);

    Skeleton skeleton;

    {
        ScopedTimer timer("Skeleton loading", &loadingProfile.skeletonLoadMs);

        if (!GltfLoader::LoadFirstSkeleton(modelPath, skeleton))
        {
            std::cerr << "Failed to load skeleton from model.\n";
            meshShader.Destroy();
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
    }

    PrintSkeletonHierarchy(skeleton);

    std::vector<AnimationClip> animationClips;

    {
        ScopedTimer timer("Animation clip loading", &loadingProfile.animationClipLoadMs);

        if (!GltfLoader::LoadAnimationClips(modelPath, animationClips))
        {
            std::cerr << "Failed to load animation clips from model.\n";
            meshShader.Destroy();
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
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

    loadingProfile.gltfTotalLoadMs =
        loadingProfile.skinnedMeshLoadMs +
        loadingProfile.skeletonLoadMs +
        loadingProfile.animationClipLoadMs;

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

    animationGraph.currentState = initialState->name;

    std::vector<AnimatedCharacter> characters;

    RebuildCharacters(
        characters,
        characterCount,
        characterAnimationMode,
        skeleton,
        animationClips,
        initialState->clipIndex);

    Mesh mesh;

    if (!mesh.CreateSkinned(meshData.vertices, meshData.indices))
    {
        std::cerr << "Failed to create OpenGL skinned mesh\n";
        meshShader.Destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::vector<glm::mat4> bindPoseGlobalMatrices;
    ComputeBindPoseFromInverseBindMatrices(skeleton, bindPoseGlobalMatrices);

    std::vector<glm::vec3> rootMotionPath;
    rootMotionPath.reserve(512);

    int rootMotionJoint = 0;

    for (std::size_t i = 0; i < skeleton.joints.size(); ++i)
    {
        if (skeleton.joints[i].name == "b_Hip_01")
        {
            rootMotionJoint = static_cast<int>(i);
            break;
        }
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

    JobSystem jobSystem;
    jobSystem.Start();

    bool key1WasDown = false;
    bool key2WasDown = false;
    bool key3WasDown = false;

    double previousTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ScopedTimer fullFrameTimer(
            "Full frame",
            &frameProfile.fullFrameMs);

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

        Animator *debugAnimator = nullptr;

        if (!characters.empty())
        {
            debugAnimator = &characters[0].animator;
        }

        if (debugAnimator != nullptr && !debugAnimator->IsBlending())
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
                    for (AnimatedCharacter &character : characters)
                    {
                        character.animator.CrossFadeTo(
                            targetState->clipIndex,
                            transition->blendTime);
                    }

                    animationGraph.currentState = targetState->name;
                }
            }
        }
        if (useMultithreadedAnimation)
        {
            {
                ScopedTimer timer(
                    "Multithreaded animation update",
                    &crowdProfile.animationUpdateMs);

                for (AnimatedCharacter &character : characters)
                {
                    AnimatedCharacter *characterPtr = &character;

                    jobSystem.Submit(
                        [characterPtr, deltaTime]()
                        {
                            UpdateCharacterAnimation(
                                *characterPtr,
                                deltaTime);
                        });
                }

                jobSystem.Wait();
            }

            {
                ScopedTimer timer(
                    "Multithreaded joint matrix generation",
                    &crowdProfile.jointMatrixGenerationMs);

                for (AnimatedCharacter &character : characters)
                {
                    AnimatedCharacter *characterPtr = &character;

                    jobSystem.Submit(
                        [characterPtr, &skeleton]()
                        {
                            GenerateCharacterJointMatrices(
                                *characterPtr,
                                skeleton);
                        });
                }

                jobSystem.Wait();
            }
        }
        else
        {
            {
                ScopedTimer timer(
                    "Single-threaded animation update",
                    &crowdProfile.animationUpdateMs);

                for (AnimatedCharacter &character : characters)
                {
                    UpdateCharacterAnimation(
                        character,
                        deltaTime);
                }
            }

            {
                ScopedTimer timer(
                    "Single-threaded joint matrix generation",
                    &crowdProfile.jointMatrixGenerationMs);

                for (AnimatedCharacter &character : characters)
                {
                    GenerateCharacterJointMatrices(
                        character,
                        skeleton);
                }
            }
        }

        frameProfile.jointMatrixGenerationMs =
            crowdProfile.jointMatrixGenerationMs;

        if (crowdProfile.animationUpdateMs > 0.0)
        {
            crowdProfile.charactersUpdatedPerSecond =
                static_cast<double>(characters.size()) /
                (crowdProfile.animationUpdateMs / 1000.0);
        }
        else
        {
            crowdProfile.charactersUpdatedPerSecond = 0.0;
        }

        crowdProfile.estimatedMemoryBytes =
            EstimateCrowdMemoryBytes(
                characters.size(),
                skeleton.joints.size());

        crowdProfile.fullFrameMs = frameProfile.fullFrameMs;

        const double smoothing = 0.05;

        if (crowdProfile.averageFrameMs <= 0.0)
        {
            crowdProfile.averageFrameMs = crowdProfile.fullFrameMs;
        }
        else
        {
            crowdProfile.averageFrameMs =
                crowdProfile.averageFrameMs * (1.0 - smoothing) +
                crowdProfile.fullFrameMs * smoothing;
        }

        if (debugAnimator != nullptr &&
            showRootMotionPath &&
            rootMotionJoint >= 0 &&
            rootMotionJoint < static_cast<int>(debugAnimator->GetPose().global.size()))
        {
            const glm::vec3 rootPosition =
                glm::vec3(debugAnimator->GetPose().global[rootMotionJoint][3]);

            if (rootMotionPath.empty() ||
                glm::distance(rootMotionPath.back(), rootPosition) > 0.01f)
            {
                rootMotionPath.push_back(rootPosition);
            }

            if (rootMotionPath.size() > 512)
            {
                rootMotionPath.erase(rootMotionPath.begin());
            }
        }

        bool rebuildCharacters = false;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Animation Runtime Debug");

        ImGui::Text("Debug Views");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const AnimationClip *currentClip =
                debugAnimator != nullptr
                    ? debugAnimator->GetCurrentClip()
                    : nullptr;

            ImGui::Text("Current state: %s", animationGraph.currentState.c_str());

            if (currentClip != nullptr && debugAnimator != nullptr)
            {
                const float normalizedTime =
                    currentClip->duration > 0.0f
                        ? debugAnimator->GetCurrentTime() / currentClip->duration
                        : 0.0f;

                ImGui::Text("Current clip: %s", currentClip->name.c_str());
                ImGui::Text(
                    "Animation time: %.3f / %.3f",
                    debugAnimator->GetCurrentTime(),
                    currentClip->duration);
                ImGui::Text("Normalized time: %.3f", normalizedTime);
            }

            float playbackSpeed =
                debugAnimator != nullptr
                    ? debugAnimator->GetPlaybackSpeed()
                    : 1.0f;

            if (ImGui::SliderFloat("Playback speed", &playbackSpeed, 0.0f, 3.0f))
            {
                for (AnimatedCharacter &character : characters)
                {
                    character.animator.SetPlaybackSpeed(playbackSpeed);
                }
            }

            bool looping =
                debugAnimator != nullptr
                    ? debugAnimator->IsLooping()
                    : true;

            if (ImGui::Checkbox("Loop", &looping))
            {
                for (AnimatedCharacter &character : characters)
                {
                    character.animator.SetLooping(looping);
                }
            }

            ImGui::Text(
                "Speed parameter: %.2f",
                GetAnimationGraphParameter(animationGraph, "speed"));

            ImGui::Text("Controls:");
            ImGui::Text("1 = speed 0.0");
            ImGui::Text("2 = speed 1.0");
            ImGui::Text("3 = speed 4.0");
        }
        if (ImGui::CollapsingHeader("Blending", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (debugAnimator != nullptr && debugAnimator->IsBlending())
            {
                const AnimationClip *previousClip = debugAnimator->GetPreviousClip();
                const AnimationClip *currentClip = debugAnimator->GetCurrentClip();

                if (previousClip != nullptr)
                {
                    ImGui::Text("Source clip: %s", previousClip->name.c_str());
                }

                if (currentClip != nullptr)
                {
                    ImGui::Text("Target clip: %s", currentClip->name.c_str());
                }

                ImGui::Text("Blend weight: %.2f", debugAnimator->GetBlendWeight());
                ImGui::Text(
                    "Blend time: %.3f / %.3f",
                    debugAnimator->GetBlendElapsed(),
                    debugAnimator->GetBlendDuration());
            }
            else
            {
                ImGui::Text("Blending: no");
            }
        }
        if (ImGui::CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Joint count: %zu", skeleton.joints.size());

            if (!skeleton.joints.empty() && debugAnimator != nullptr)
            {
                selectedJoint = std::clamp(
                    selectedJoint,
                    0,
                    static_cast<int>(skeleton.joints.size()) - 1);

                ImGui::SliderInt(
                    "Selected joint",
                    &selectedJoint,
                    0,
                    static_cast<int>(skeleton.joints.size()) - 1);

                const Joint &joint =
                    skeleton.joints[static_cast<std::size_t>(selectedJoint)];

                ImGui::Text("Joint name: %s", joint.name.c_str());
                ImGui::Text("Parent index: %d", joint.parent);

                if (joint.parent >= 0)
                {
                    const Joint &parent =
                        skeleton.joints[static_cast<std::size_t>(joint.parent)];

                    ImGui::Text("Parent name: %s", parent.name.c_str());
                }
                else
                {
                    ImGui::Text("Parent name: none");
                }

                const Pose &pose = debugAnimator->GetPose();

                if (selectedJoint < static_cast<int>(pose.GetJointCount()))
                {
                    const Transform local =
                        pose.GetLocal(static_cast<std::size_t>(selectedJoint));

                    ImGui::Separator();
                    ImGui::Text("Local transform");
                    ImGui::Text(
                        "Translation: %.3f, %.3f, %.3f",
                        local.translation.x,
                        local.translation.y,
                        local.translation.z);
                    ImGui::Text(
                        "Rotation quat: %.3f, %.3f, %.3f, %.3f",
                        local.rotation.w,
                        local.rotation.x,
                        local.rotation.y,
                        local.rotation.z);
                    ImGui::Text(
                        "Scale: %.3f, %.3f, %.3f",
                        local.scale.x,
                        local.scale.y,
                        local.scale.z);
                }

                if (selectedJoint < static_cast<int>(pose.global.size()))
                {
                    const glm::mat4 &global =
                        pose.global[static_cast<std::size_t>(selectedJoint)];

                    ImGui::Separator();
                    ImGui::Text("Global matrix");

                    for (int row = 0; row < 4; ++row)
                    {
                        ImGui::Text(
                            "%.3f %.3f %.3f %.3f",
                            global[0][row],
                            global[1][row],
                            global[2][row],
                            global[3][row]);
                    }
                }
            }
        }
        if (ImGui::CollapsingHeader("Debug Rendering", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Show mesh", &showMesh);
            ImGui::Checkbox("Show skeleton", &showSkeleton);
            ImGui::Checkbox("Show joint markers", &showJointMarkers);
            ImGui::Checkbox("Show root motion path", &showRootMotionPath);
            ImGui::Checkbox("Show forward vector", &showForwardVector);
            ImGui::Checkbox("Show bind pose skeleton", &showBindPose);

            if (ImGui::Button("Clear root path"))
            {
                rootMotionPath.clear();
            }
        }

        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
        {
            AnimationTimingStats timing{};

            if (debugAnimator != nullptr)
            {
                timing = debugAnimator->GetTimingStats();
            }

            if (ImGui::BeginTable(
                    "PerformanceTable",
                    2,
                    ImGuiTableFlags_Borders |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Stage");
                ImGui::TableSetupColumn("Time");
                ImGui::TableHeadersRow();

                auto AddRow = [](const char *stage, double ms)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", stage);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.4f ms", ms);
                };

                AddRow("Animation sampling", timing.samplingMs);
                AddRow("Local-to-global pose", timing.localToGlobalMs);
                AddRow(
                    useMultithreadedAnimation
                        ? "MT animation update"
                        : "ST animation update",
                    crowdProfile.animationUpdateMs);

                AddRow(
                    useMultithreadedAnimation
                        ? "MT joint matrix generation"
                        : "ST joint matrix generation",
                    crowdProfile.jointMatrixGenerationMs);
                AddRow("Joint matrix upload", frameProfile.jointMatrixUploadMs);
                AddRow("Rendering", frameProfile.renderMs);
                AddRow("Full frame", frameProfile.fullFrameMs);

                ImGui::EndTable();
            }

            ImGui::Separator();

            ImGui::Text("Characters: %zu", characters.size());
            ImGui::Text("Joint count per character: %zu", skeleton.joints.size());
            ImGui::Text(
                "Total animated joints: %zu",
                characters.size() * skeleton.joints.size());
            ImGui::Text(
                "Characters updated/sec: %.0f",
                crowdProfile.charactersUpdatedPerSecond);

            const double memoryMb =
                static_cast<double>(crowdProfile.estimatedMemoryBytes) /
                (1024.0 * 1024.0);

            ImGui::Text("Estimated animation memory: %.2f MB", memoryMb);
            ImGui::Text("Average frame time: %.3f ms", crowdProfile.averageFrameMs);

            ImGui::Separator();

            if (ImGui::BeginTable(
                    "LoadingTable",
                    2,
                    ImGuiTableFlags_Borders |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Loading stage");
                ImGui::TableSetupColumn("Time");
                ImGui::TableHeadersRow();

                auto AddRow = [](const char *stage, double ms)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", stage);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.4f ms", ms);
                };

                AddRow("Animation graph load", loadingProfile.graphLoadMs);
                AddRow("Skinned mesh glTF load", loadingProfile.skinnedMeshLoadMs);
                AddRow("Skeleton glTF load", loadingProfile.skeletonLoadMs);
                AddRow("Animation clips glTF load", loadingProfile.animationClipLoadMs);
                AddRow("Total glTF load", loadingProfile.gltfTotalLoadMs);

                ImGui::EndTable();
            }

            ImGui::Separator();
            ImGui::Text("FPS: %.1f", io.Framerate);
        }

        if (ImGui::CollapsingHeader("Crowd Test", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Character count");

            if (ImGui::Button("1"))
            {
                requestedCharacterCount = 1;
                rebuildCharacters = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("10"))
            {
                requestedCharacterCount = 10;
                rebuildCharacters = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("100"))
            {
                requestedCharacterCount = 100;
                rebuildCharacters = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("500"))
            {
                requestedCharacterCount = 500;
                rebuildCharacters = true;
            }

            ImGui::Text("Current characters: %zu", characters.size());

            ImGui::Checkbox(
                "Multithreaded animation update",
                &useMultithreadedAnimation);

            ImGui::Text(
                "Worker threads: %u",
                jobSystem.GetWorkerCount());

            ImGui::Separator();

            if (ImGui::RadioButton(
                    "Same clip",
                    characterAnimationMode == CharacterAnimationMode::SameClip))
            {
                if (characterAnimationMode != CharacterAnimationMode::SameClip)
                {
                    characterAnimationMode = CharacterAnimationMode::SameClip;
                    rebuildCharacters = true;
                }
            }

            if (ImGui::RadioButton(
                    "Random clip",
                    characterAnimationMode == CharacterAnimationMode::RandomClip))
            {
                if (characterAnimationMode != CharacterAnimationMode::RandomClip)
                {
                    characterAnimationMode = CharacterAnimationMode::RandomClip;
                    rebuildCharacters = true;
                }
            }

            if (ImGui::RadioButton(
                    "Random phase",
                    characterAnimationMode == CharacterAnimationMode::RandomPhase))
            {
                if (characterAnimationMode != CharacterAnimationMode::RandomPhase)
                {
                    characterAnimationMode = CharacterAnimationMode::RandomPhase;
                    rebuildCharacters = true;
                }
            }

            ImGui::Separator();

            ImGui::Text("Skinning");

            if (ImGui::RadioButton("GPU", skinningMode == SkinningMode::GPU))
            {
                skinningMode = SkinningMode::GPU;
            }

            if (ImGui::RadioButton("CPU", skinningMode == SkinningMode::CPU))
            {
                skinningMode = SkinningMode::CPU;
            }

            if (skinningMode == SkinningMode::CPU)
            {
                ImGui::TextWrapped(
                    "CPU skinning path is not implemented yet. "
                    "This mode is reserved for a later CPU skinning benchmark.");
            }

            ImGui::Checkbox("Debug skeletons", &showSkeleton);
        }
        ImGui::End();

        ImGui::Render();

        if (rebuildCharacters || requestedCharacterCount != characterCount)
        {
            characterCount = requestedCharacterCount;

            RebuildCharacters(
                characters,
                characterCount,
                characterAnimationMode,
                skeleton,
                animationClips,
                initialState->clipIndex);

            rootMotionPath.clear();
        }

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

        frameProfile.jointMatrixUploadMs = 0.0;

        {
            ScopedTimer renderTimer(
                "Rendering",
                &frameProfile.renderMs);

            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (showMesh && skinningMode == SkinningMode::GPU)
            {
                meshShader.Use();

                for (const AnimatedCharacter &character : characters)
                {
                    const glm::mat4 characterModel =
                        character.worldTransform * model;

                    const glm::mat4 characterMvp =
                        projection * view * characterModel;

                    meshShader.SetMat4(uMvpLocation, characterMvp);

                    double uploadMs = 0.0;

                    {
                        ScopedTimer uploadTimer(
                            "Joint matrix upload",
                            &uploadMs);

                        meshShader.SetMat4Array(
                            uJointMatricesLocation,
                            character.jointMatrices);
                    }

                    frameProfile.jointMatrixUploadMs += uploadMs;

                    mesh.Draw();
                }
            }

            if (!characters.empty())
            {
                const int maxDebugSkeletons =
                    characters.size() > 100
                        ? 10
                        : static_cast<int>(characters.size());

                if (showSkeleton)
                {
                    for (int i = 0; i < maxDebugSkeletons; ++i)
                    {
                        const AnimatedCharacter &character =
                            characters[static_cast<std::size_t>(i)];

                        const std::vector<glm::mat4> &poseToDraw =
                            showBindPose
                                ? bindPoseGlobalMatrices
                                : character.animator.GetPose().global;

                        const glm::mat4 characterModel =
                            character.worldTransform * model;

                        const glm::mat4 characterMvp =
                            projection * view * characterModel;

                        debugDraw.Begin();

                        debugDraw.AddSkeleton(
                            skeleton,
                            poseToDraw,
                            glm::vec3(1.0f, 1.0f, 0.0f));

                        debugDraw.Draw(characterMvp);
                    }
                }

                if (showJointMarkers || showRootMotionPath || showForwardVector)
                {
                    const AnimatedCharacter &debugCharacter = characters[0];

                    const std::vector<glm::mat4> &poseToDraw =
                        showBindPose
                            ? bindPoseGlobalMatrices
                            : debugCharacter.animator.GetPose().global;

                    const glm::mat4 debugModel =
                        debugCharacter.worldTransform * model;

                    const glm::mat4 debugMvp =
                        projection * view * debugModel;

                    debugDraw.Begin();

                    if (showJointMarkers)
                    {
                        debugDraw.AddJointMarkers(
                            poseToDraw,
                            selectedJoint,
                            0.025f);
                    }

                    if (showRootMotionPath)
                    {
                        debugDraw.AddPath(
                            rootMotionPath,
                            glm::vec3(0.0f, 1.0f, 1.0f));
                    }

                    if (showForwardVector && !poseToDraw.empty())
                    {
                        const glm::vec3 origin =
                            glm::vec3(poseToDraw[0][3]);

                        debugDraw.AddForwardVector(
                            origin,
                            glm::vec3(0.0f, 0.0f, 1.0f),
                            0.5f,
                            glm::vec3(1.0f, 0.0f, 1.0f));
                    }

                    debugDraw.Draw(debugMvp);
                }
            }

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        crowdProfile.renderMs = frameProfile.renderMs;

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
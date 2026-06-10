#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "animation/Animator.h"

struct RootMotionState
{
    int jointIndex = -1;

    bool hasPreviousRootPosition = false;
    bool hasReferenceLocalTranslation = false;

    glm::vec3 previousRootPosition{0.0f};
    glm::vec3 referenceLocalTranslation{0.0f};

    glm::vec3 deltaThisFrame{0.0f};
    glm::vec3 accumulatedMotion{0.0f};

    std::vector<glm::vec3> path;
};

struct AnimatedCharacter
{
    Animator animator;

    glm::vec3 worldPosition{0.0f};
    glm::mat4 worldTransform{1.0f};

    std::vector<glm::mat4> jointMatrices;

    int clipIndex = 0;

    RootMotionState rootMotion;
};
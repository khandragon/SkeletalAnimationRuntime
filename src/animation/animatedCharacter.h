#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "animation/Animator.h"

struct AnimatedCharacter
{
    Animator animator;
    glm::mat4 worldTransform{1.0f};

    std::vector<glm::mat4> jointMatrices;

    int clipIndex = 0;
};
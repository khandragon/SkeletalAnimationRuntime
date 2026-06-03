#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "math/Transform.h"

struct Pose
{
    std::vector<Transform> local;
    std::vector<glm::mat4> global;

    void Resize(std::size_t jointCount)
    {
        local.resize(jointCount);
        global.resize(jointCount);
    }
};
#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "math/Transform.h"

struct Pose
{
    std::vector<Transform> local;
    std::vector<glm::mat4> global;

    void Resize(std::size_t jointCount)
    {
        if (local.size() != jointCount)
        {
            local.resize(jointCount);
        }

        if (global.size() != jointCount)
        {
            global.resize(jointCount);
        }
    }

    bool HasSize(std::size_t jointCount) const
    {
        return local.size() == jointCount &&
               global.size() == jointCount;
    }
};
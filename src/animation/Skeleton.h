#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "math/Transform.h"

struct Joint
{
    std::string name;
    int parent = -1;
    glm::mat4 inverseBindMatrix{1.0f};
    Transform bindLocalTransform;
};

struct Skeleton
{
    std::vector<Joint> joints;
};

void PrintSkeletonHierarchy(const Skeleton &skeleton);
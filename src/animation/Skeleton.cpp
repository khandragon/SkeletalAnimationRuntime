#include "animation/Skeleton.h"

#include <iostream>

void PrintSkeletonHierarchy(const Skeleton &skeleton)
{
    std::cout << "Skeleton joint count: " << skeleton.joints.size() << '\n';

    for (std::size_t i = 0; i < skeleton.joints.size(); ++i)
    {
        const Joint &joint = skeleton.joints[i];

        std::cout
            << i
            << " "
            << joint.name
            << " parent="
            << joint.parent
            << '\n';
    }
}

void ComputeBindPoseGlobalMatrices(
    const Skeleton &skeleton,
    std::vector<glm::mat4> &outGlobalMatrices)
{
    outGlobalMatrices.resize(skeleton.joints.size());

    for (std::size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
    {
        const Joint &joint = skeleton.joints[jointIndex];

        glm::mat4 localMatrix =
            TransformToMat4(joint.bindLocalTransform);

        if (joint.parent < 0)
        {
            outGlobalMatrices[jointIndex] = localMatrix;
        }
        else
        {
            outGlobalMatrices[jointIndex] =
                outGlobalMatrices[static_cast<std::size_t>(joint.parent)] *
                localMatrix;
        }
    }
}

void ComputeBindPoseFromInverseBindMatrices(
    const Skeleton &skeleton,
    std::vector<glm::mat4> &outGlobalMatrices)
{
    outGlobalMatrices.resize(skeleton.joints.size());

    for (std::size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
    {
        outGlobalMatrices[jointIndex] =
            glm::inverse(skeleton.joints[jointIndex].inverseBindMatrix);
    }
}
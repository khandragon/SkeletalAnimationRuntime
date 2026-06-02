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
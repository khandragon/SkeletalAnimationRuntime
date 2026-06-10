#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "math/Transform.h"

struct Pose
{
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;

    std::vector<glm::mat4> global;

    void Resize(std::size_t jointCount)
    {
        if (translations.size() != jointCount)
        {
            translations.resize(jointCount, glm::vec3(0.0f));
        }

        if (rotations.size() != jointCount)
        {
            rotations.resize(jointCount, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }

        if (scales.size() != jointCount)
        {
            scales.resize(jointCount, glm::vec3(1.0f));
        }

        if (global.size() != jointCount)
        {
            global.resize(jointCount, glm::mat4(1.0f));
        }
    }

    bool HasSize(std::size_t jointCount) const
    {
        return translations.size() == jointCount &&
               rotations.size() == jointCount &&
               scales.size() == jointCount &&
               global.size() == jointCount;
    }

    std::size_t GetJointCount() const
    {
        return translations.size();
    }

    Transform GetLocal(std::size_t jointIndex) const
    {
        Transform transform{};

        transform.translation = translations[jointIndex];
        transform.rotation = rotations[jointIndex];
        transform.scale = scales[jointIndex];

        return transform;
    }

    void SetLocal(
        std::size_t jointIndex,
        const Transform &transform)
    {
        translations[jointIndex] = transform.translation;
        rotations[jointIndex] = transform.rotation;
        scales[jointIndex] = transform.scale;
    }
};
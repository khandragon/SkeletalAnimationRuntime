#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "math/Transform.h"

// Pose stores the local translation, rotation and scale for each joint, as well as the global transform for each joint.
struct Pose
{
    // Local transforms
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;

    // Global transforms
    std::vector<glm::mat4> global;

    // Making sure this pose has space for exactly jointCount joints.
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

    // Checks if this pose has space for exactly jointCount joints.
    bool HasSize(std::size_t jointCount) const
    {
        return translations.size() == jointCount &&
               rotations.size() == jointCount &&
               scales.size() == jointCount &&
               global.size() == jointCount;
    }

    // Returns the number of joints in this pose, we use translations.size() but we assume all vectors have the same size.
    std::size_t GetJointCount() const
    {
        return translations.size();
    }

    // Gets the local transform for the joint at the given index.
    Transform GetLocal(std::size_t jointIndex) const
    {
        Transform transform{};

        transform.translation = translations[jointIndex];
        transform.rotation = rotations[jointIndex];
        transform.scale = scales[jointIndex];

        return transform;
    }

    // Sets the local transform for the joint at the given index.
    void SetLocal(
        std::size_t jointIndex,
        const Transform &transform)
    {
        translations[jointIndex] = transform.translation;
        rotations[jointIndex] = transform.rotation;
        scales[jointIndex] = transform.scale;
    }
};
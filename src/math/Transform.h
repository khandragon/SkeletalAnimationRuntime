#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform
{
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

inline glm::mat4 TransformToMat4(const Transform &transform)
{
    glm::mat4 translationMatrix =
        glm::translate(glm::mat4(1.0f), transform.translation);

    glm::mat4 rotationMatrix =
        glm::mat4_cast(transform.rotation);

    glm::mat4 scaleMatrix =
        glm::scale(glm::mat4(1.0f), transform.scale);

    return translationMatrix * rotationMatrix * scaleMatrix;
}
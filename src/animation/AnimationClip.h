#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

enum class ChannelPath
{
    Translation,
    Rotation,
    Scale
};

struct BlendTree1DMotion
{
    std::string clipName;
    float position = 0.0f;

    std::size_t clipIndex =
        static_cast<std::size_t>(-1);
};

struct AnimationChannel
{
    int jointIndex = -1;
    ChannelPath path = ChannelPath::Translation;

    std::vector<float> times;
    std::vector<glm::vec4> values;
};

struct AnimationClip
{
    std::string name;
    float duration = 0.0f;

    std::vector<AnimationChannel> channels;
};

const char *ChannelPathToString(ChannelPath path);
void PrintAnimationClips(const std::vector<AnimationClip> &clips);
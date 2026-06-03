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
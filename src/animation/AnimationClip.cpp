#include "animation/AnimationClip.h"

#include <iostream>

const char *ChannelPathToString(ChannelPath path)
{
    switch (path)
    {
    case ChannelPath::Translation:
        return "Translation";

    case ChannelPath::Rotation:
        return "Rotation";

    case ChannelPath::Scale:
        return "Scale";

    default:
        return "Unknown";
    }
}

void PrintAnimationClips(const std::vector<AnimationClip> &clips)
{
    std::cout << "Animation clips loaded: " << clips.size() << '\n';

    for (const AnimationClip &clip : clips)
    {
        std::cout
            << "Clip: "
            << clip.name
            << " duration="
            << clip.duration
            << " channels="
            << clip.channels.size()
            << '\n';

        for (std::size_t i = 0; i < clip.channels.size(); ++i)
        {
            const AnimationChannel &channel = clip.channels[i];

            std::cout
                << "  Channel "
                << i
                << " joint="
                << channel.jointIndex
                << " path="
                << ChannelPathToString(channel.path)
                << " keys="
                << channel.times.size()
                << '\n';
        }
    }
}
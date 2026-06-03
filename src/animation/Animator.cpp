#include "animation/Animator.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/quaternion.hpp>

void Animator::Initialize(
    const Skeleton *skeleton,
    const std::vector<AnimationClip> *clips)
{
    m_skeleton = skeleton;
    m_clips = clips;
    m_time = 0.0f;
    m_currentClipIndex = 0;

    if (m_skeleton != nullptr)
    {
        m_pose.Resize(m_skeleton->joints.size());
        ResetPoseToBindPose();
        ComputeGlobalPose();
    }
}

void Animator::Play(std::size_t clipIndex, bool resetTime)
{
    if (m_clips == nullptr)
    {
        return;
    }

    if (clipIndex >= m_clips->size())
    {
        return;
    }

    m_currentClipIndex = clipIndex;

    if (resetTime)
    {
        m_time = 0.0f;
    }
}

void Animator::Update(float deltaTime)
{
    if (m_skeleton == nullptr || m_clips == nullptr || m_clips->empty())
    {
        return;
    }

    const AnimationClip &clip = (*m_clips)[m_currentClipIndex];

    if (clip.duration <= 0.0f)
    {
        return;
    }

    m_time += deltaTime;

    // Loop the animation.
    m_time = std::fmod(m_time, clip.duration);

    if (m_time < 0.0f)
    {
        m_time += clip.duration;
    }

    ResetPoseToBindPose();
    SampleClip(clip, m_time, m_pose);
    ComputeGlobalPose();
}

const AnimationClip *Animator::GetCurrentClip() const
{
    if (m_clips == nullptr || m_clips->empty())
    {
        return nullptr;
    }

    if (m_currentClipIndex >= m_clips->size())
    {
        return nullptr;
    }

    return &(*m_clips)[m_currentClipIndex];
}

void Animator::ResetPoseToBindPose()
{
    if (m_skeleton == nullptr)
    {
        return;
    }

    m_pose.Resize(m_skeleton->joints.size());

    for (std::size_t i = 0; i < m_skeleton->joints.size(); ++i)
    {
        m_pose.local[i] = m_skeleton->joints[i].bindLocalTransform;
        m_pose.global[i] = glm::mat4(1.0f);
    }
}

void Animator::ComputeGlobalPose()
{
    if (m_skeleton == nullptr)
    {
        return;
    }

    for (std::size_t jointIndex = 0; jointIndex < m_skeleton->joints.size(); ++jointIndex)
    {
        const Joint &joint = m_skeleton->joints[jointIndex];

        const glm::mat4 localMatrix =
            TransformToMat4(m_pose.local[jointIndex]);

        if (joint.parent < 0)
        {
            m_pose.global[jointIndex] = localMatrix;
        }
        else
        {
            m_pose.global[jointIndex] =
                m_pose.global[static_cast<std::size_t>(joint.parent)] *
                localMatrix;
        }
    }
}

void Animator::SampleClip(
    const AnimationClip &clip,
    float time,
    Pose &pose)
{
    for (const AnimationChannel &channel : clip.channels)
    {
        if (channel.jointIndex < 0 ||
            channel.jointIndex >= static_cast<int>(pose.local.size()))
        {
            continue;
        }

        Transform &jointTransform =
            pose.local[static_cast<std::size_t>(channel.jointIndex)];

        switch (channel.path)
        {
        case ChannelPath::Translation:
            jointTransform.translation =
                SampleVec3Channel(
                    channel,
                    time,
                    jointTransform.translation);
            break;

        case ChannelPath::Rotation:
            jointTransform.rotation =
                SampleRotationChannel(
                    channel,
                    time,
                    jointTransform.rotation);
            break;

        case ChannelPath::Scale:
            jointTransform.scale =
                SampleVec3Channel(
                    channel,
                    time,
                    jointTransform.scale);
            break;
        }
    }
}

std::size_t Animator::FindKeyframeIndex(
    const std::vector<float> &times,
    float time)
{
    if (times.size() < 2)
    {
        return 0;
    }

    if (time <= times.front())
    {
        return 0;
    }

    for (std::size_t i = 0; i + 1 < times.size(); ++i)
    {
        if (time >= times[i] && time <= times[i + 1])
        {
            return i;
        }
    }

    return times.size() - 2;
}

glm::vec3 Animator::SampleVec3Channel(
    const AnimationChannel &channel,
    float time,
    const glm::vec3 &fallback)
{
    if (channel.times.empty() || channel.values.empty())
    {
        return fallback;
    }

    if (channel.times.size() == 1 || channel.values.size() == 1)
    {
        return glm::vec3(channel.values.front());
    }

    if (time <= channel.times.front())
    {
        return glm::vec3(channel.values.front());
    }

    if (time >= channel.times.back())
    {
        return glm::vec3(channel.values.back());
    }

    const std::size_t keyIndex =
        FindKeyframeIndex(channel.times, time);

    const float t0 = channel.times[keyIndex];
    const float t1 = channel.times[keyIndex + 1];

    const glm::vec3 v0 = glm::vec3(channel.values[keyIndex]);
    const glm::vec3 v1 = glm::vec3(channel.values[keyIndex + 1]);

    const float alpha =
        (t1 > t0)
            ? (time - t0) / (t1 - t0)
            : 0.0f;

    return glm::mix(v0, v1, alpha);
}

glm::quat Animator::SampleRotationChannel(
    const AnimationChannel &channel,
    float time,
    const glm::quat &fallback)
{
    if (channel.times.empty() || channel.values.empty())
    {
        return fallback;
    }

    auto ToQuat = [](const glm::vec4 &value)
    {
        // glTF stores quaternion as x, y, z, w.
        // GLM expects w, x, y, z.
        return glm::normalize(glm::quat(
            value.w,
            value.x,
            value.y,
            value.z));
    };

    if (channel.times.size() == 1 || channel.values.size() == 1)
    {
        return ToQuat(channel.values.front());
    }

    if (time <= channel.times.front())
    {
        return ToQuat(channel.values.front());
    }

    if (time >= channel.times.back())
    {
        return ToQuat(channel.values.back());
    }

    const std::size_t keyIndex =
        FindKeyframeIndex(channel.times, time);

    const float t0 = channel.times[keyIndex];
    const float t1 = channel.times[keyIndex + 1];

    const glm::quat q0 = ToQuat(channel.values[keyIndex]);
    const glm::quat q1 = ToQuat(channel.values[keyIndex + 1]);

    const float alpha =
        (t1 > t0)
            ? (time - t0) / (t1 - t0)
            : 0.0f;

    return glm::normalize(glm::slerp(q0, q1, alpha));
}
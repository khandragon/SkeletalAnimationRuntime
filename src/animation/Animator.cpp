#include "animation/Animator.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/quaternion.hpp>

#include "core/Timer.h"

void Animator::Initialize(
    const Skeleton *skeleton,
    const std::vector<AnimationClip> *clips)
{
    m_skeleton = skeleton;
    m_clips = clips;
    m_time = 0.0f;
    m_previousTime = 0.0f;
    m_currentClipIndex = 0;
    m_previousClipIndex = 0;
    m_isBlending = false;
    m_blendElapsed = 0.0f;
    m_blendDuration = 0.0f;

    if (m_skeleton != nullptr)
    {
        const std::size_t jointCount = m_skeleton->joints.size();

        m_pose.Resize(jointCount);
        m_fromPose.Resize(jointCount);
        m_toPose.Resize(jointCount);

        ResetPoseToBindPose(m_pose);
        ComputeGlobalPose(m_pose);
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
    m_previousClipIndex = clipIndex;
    m_isBlending = false;
    m_blendElapsed = 0.0f;
    m_blendDuration = 0.0f;

    if (resetTime)
    {
        m_time = 0.0f;
        m_previousTime = 0.0f;
    }
}

void Animator::CrossFadeTo(std::size_t clipIndex, float blendDuration)
{
    if (m_clips == nullptr)
    {
        return;
    }

    if (clipIndex >= m_clips->size())
    {
        return;
    }

    if (clipIndex == m_currentClipIndex && !m_isBlending)
    {
        return;
    }

    if (blendDuration <= 0.0f)
    {
        Play(clipIndex, true);
        return;
    }

    // The current clip becomes the source of the blend.
    m_previousClipIndex = m_currentClipIndex;
    m_previousTime = m_time;

    // The requested clip becomes the destination.
    m_currentClipIndex = clipIndex;
    m_time = 0.0f;

    m_isBlending = true;
    m_blendElapsed = 0.0f;
    m_blendDuration = blendDuration;
}

void Animator::Update(float deltaTime)
{
    if (m_skeleton == nullptr || m_clips == nullptr || m_clips->empty())
    {
        return;
    }

    if (m_currentClipIndex >= m_clips->size())
    {
        return;
    }

    const AnimationClip &currentClip = (*m_clips)[m_currentClipIndex];

    if (currentClip.duration <= 0.0f)
    {
        return;
    }

    // Advance destination/current animation time.
    m_time += deltaTime * m_playbackSpeed;
    m_time = std::fmod(m_time, currentClip.duration);

    if (m_time < 0.0f)
    {
        m_time += currentClip.duration;
    }

    if (!m_isBlending)
    {
        {
            ScopedTimer timer(
                "Animation sampling",
                &m_timingStats.samplingMs);

            ResetPoseToBindPose(m_pose);
            SampleClip(currentClip, m_time, m_pose);
        }

        {
            ScopedTimer timer(
                "Local-to-global pose",
                &m_timingStats.localToGlobalMs);

            ComputeGlobalPose(m_pose);
        }

        return;
    }

    if (m_previousClipIndex >= m_clips->size())
    {
        m_isBlending = false;
        return;
    }

    const AnimationClip &previousClip = (*m_clips)[m_previousClipIndex];

    if (previousClip.duration <= 0.0f)
    {
        m_isBlending = false;
        return;
    }

    // Advance source/previous animation time too.
    m_previousTime += deltaTime * m_playbackSpeed;
    m_previousTime = std::fmod(m_previousTime, previousClip.duration);

    if (m_previousTime < 0.0f)
    {
        m_previousTime += previousClip.duration;
    }

    m_blendElapsed += deltaTime;

    const float weight = GetBlendWeight();

    {
        ScopedTimer timer(
            "Animation sampling",
            &m_timingStats.samplingMs
        );

        ResetPoseToBindPose(m_fromPose);
        ResetPoseToBindPose(m_toPose);

        SampleClip(previousClip, m_previousTime, m_fromPose);
        SampleClip(currentClip, m_time, m_toPose);

        BlendLocalPoses(m_fromPose, m_toPose, weight, m_pose);
    }

    {
        ScopedTimer timer(
            "Local-to-global pose",
            &m_timingStats.localToGlobalMs
        );

        ComputeGlobalPose(m_pose);
    }

    if (m_blendElapsed >= m_blendDuration)
    {
        m_isBlending = false;
        m_blendElapsed = 0.0f;
        m_blendDuration = 0.0f;
        m_previousClipIndex = m_currentClipIndex;
        m_previousTime = m_time;
    }
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

const AnimationClip *Animator::GetPreviousClip() const
{
    if (m_clips == nullptr || m_clips->empty())
    {
        return nullptr;
    }

    if (m_previousClipIndex >= m_clips->size())
    {
        return nullptr;
    }

    return &(*m_clips)[m_previousClipIndex];
}

float Animator::GetBlendWeight() const
{
    if (!m_isBlending || m_blendDuration <= 0.0f)
    {
        return 1.0f;
    }

    return std::clamp(m_blendElapsed / m_blendDuration, 0.0f, 1.0f);
}

void Animator::ResetPoseToBindPose(Pose &pose)
{
    if (m_skeleton == nullptr)
    {
        return;
    }

    pose.Resize(m_skeleton->joints.size());

    for (std::size_t i = 0; i < m_skeleton->joints.size(); ++i)
    {
        pose.local[i] = m_skeleton->joints[i].bindLocalTransform;
        pose.global[i] = glm::mat4(1.0f);
    }
}

void Animator::ComputeGlobalPose(Pose &pose)
{
    if (m_skeleton == nullptr)
    {
        return;
    }

    for (std::size_t jointIndex = 0; jointIndex < m_skeleton->joints.size(); ++jointIndex)
    {
        const Joint &joint = m_skeleton->joints[jointIndex];

        const glm::mat4 localMatrix =
            TransformToMat4(pose.local[jointIndex]);

        if (joint.parent < 0)
        {
            pose.global[jointIndex] = localMatrix;
        }
        else
        {
            pose.global[jointIndex] =
                pose.global[static_cast<std::size_t>(joint.parent)] *
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

void Animator::BlendLocalPoses(
    const Pose &fromPose,
    const Pose &toPose,
    float weight,
    Pose &outPose)
{
    if (m_skeleton == nullptr)
    {
        return;
    }

    const std::size_t jointCount = m_skeleton->joints.size();

    outPose.Resize(jointCount);

    for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
    {
        const Transform &a = fromPose.local[jointIndex];
        const Transform &b = toPose.local[jointIndex];

        Transform blended{};

        blended.translation =
            glm::mix(a.translation, b.translation, weight);

        blended.rotation =
            glm::normalize(glm::slerp(a.rotation, b.rotation, weight));

        blended.scale =
            glm::mix(a.scale, b.scale, weight);

        outPose.local[jointIndex] = blended;
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
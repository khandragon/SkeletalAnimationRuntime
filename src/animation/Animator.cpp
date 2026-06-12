#include "animation/Animator.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/quaternion.hpp>

#include "core/Timer.h"
#include "math/TransformBlend.h"

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
    m_currentKeyframeCache.clear();
    m_previousKeyframeCache.clear();

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

    const AnimationClip &clip = (*m_clips)[m_currentClipIndex];

    PrepareKeyframeCache(m_currentKeyframeCache, clip);
    PrepareKeyframeCache(m_previousKeyframeCache, clip);

    if (resetTime)
    {
        ResetKeyframeCache(m_currentKeyframeCache);
        ResetKeyframeCache(m_previousKeyframeCache);
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
    m_previousKeyframeCache = m_currentKeyframeCache;

    // The requested clip becomes the destination.
    m_currentClipIndex = clipIndex;
    m_time = 0.0f;

    const AnimationClip &currentClip = (*m_clips)[m_currentClipIndex];

    PrepareKeyframeCache(m_currentKeyframeCache, currentClip);
    ResetKeyframeCache(m_currentKeyframeCache);

    m_isBlending = true;
    m_blendElapsed = 0.0f;
    m_blendDuration = blendDuration;
}

void Animator::SetCurrentTime(float time)
{
    const AnimationClip *clip = GetCurrentClip();

    if (clip == nullptr || clip->duration <= 0.0f)
    {
        m_time = time;
        ResetKeyframeCache(m_currentKeyframeCache);
        return;
    }

    m_time = NormalizeClipTime(time, clip->duration);
    ResetKeyframeCache(m_currentKeyframeCache);
}

void Animator::SetNormalizedTime(float normalizedTime)
{
    const AnimationClip *clip = GetCurrentClip();

    if (clip == nullptr || clip->duration <= 0.0f)
    {
        return;
    }

    SetCurrentTime(normalizedTime * clip->duration);
}

float Animator::NormalizeClipTime(float time, float duration) const
{
    if (duration <= 0.0f)
    {
        return 0.0f;
    }

    if (m_loop)
    {
        float wrappedTime = std::fmod(time, duration);

        if (wrappedTime < 0.0f)
        {
            wrappedTime += duration;
        }

        return wrappedTime;
    }

    return std::clamp(time, 0.0f, duration);
}

float Animator::AdvanceClipTime(
    float time,
    float deltaTime,
    float duration,
    std::vector<std::size_t> &keyframeCache)
{
    const float previousTime = time;

    const float newTime =
        NormalizeClipTime(
            time + deltaTime * m_playbackSpeed,
            duration);

    if (m_loop)
    {
        const bool wrappedForward =
            m_playbackSpeed > 0.0f &&
            newTime < previousTime;

        const bool wrappedBackward =
            m_playbackSpeed < 0.0f &&
            newTime > previousTime;

        if (wrappedForward || wrappedBackward)
        {
            ResetKeyframeCache(keyframeCache);
        }
    }

    return newTime;
}

float Animator::InverseLerp(
    float a,
    float b,
    float value)
{
    if (std::abs(b - a) <= 0.00001f)
    {
        return 0.0f;
    }

    return std::clamp(
        (value - a) / (b - a),
        0.0f,
        1.0f);
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
    m_time = AdvanceClipTime(
        m_time,
        deltaTime,
        currentClip.duration,
        m_currentKeyframeCache);

    if (!m_isBlending)
    {
        {
            ScopedTimer timer(
                "Animation sampling",
                &m_timingStats.samplingMs);

            ResetPoseToBindPose(m_pose);
            SampleClip(
                currentClip,
                m_time,
                m_pose,
                m_currentKeyframeCache);
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
    m_previousTime = AdvanceClipTime(
        m_previousTime,
        deltaTime,
        previousClip.duration,
        m_previousKeyframeCache);

    m_blendElapsed += deltaTime;

    const float weight = GetBlendWeight();

    {
        ScopedTimer timer(
            "Animation sampling",
            &m_timingStats.samplingMs);

        ResetPoseToBindPose(m_fromPose);
        ResetPoseToBindPose(m_toPose);

        SampleClip(
            previousClip,
            m_previousTime,
            m_fromPose,
            m_previousKeyframeCache);

        SampleClip(
            currentClip,
            m_time,
            m_toPose,
            m_currentKeyframeCache);

        BlendLocalPoses(m_fromPose, m_toPose, weight, m_pose);
    }

    {
        ScopedTimer timer(
            "Local-to-global pose",
            &m_timingStats.localToGlobalMs);

        ComputeGlobalPose(m_pose);
    }

    if (m_blendElapsed >= m_blendDuration)
    {
        m_isBlending = false;
        m_blendElapsed = 0.0f;
        m_blendDuration = 0.0f;
        m_previousClipIndex = m_currentClipIndex;
        m_previousTime = m_time;
        m_previousKeyframeCache = m_currentKeyframeCache;
    }
}

void Animator::EvaluateBlendTree1D(
    const std::vector<BlendTree1DMotion> &motions,
    float parameterValue,
    float deltaTime)
{
    if (m_skeleton == nullptr ||
        m_clips == nullptr ||
        m_clips->empty() ||
        motions.empty())
    {
        return;
    }

    const BlendTree1DMotion *lowerMotion = &motions.front();
    const BlendTree1DMotion *upperMotion = &motions.front();

    if (parameterValue <= motions.front().position)
    {
        lowerMotion = &motions.front();
        upperMotion = &motions.front();
    }
    else if (parameterValue >= motions.back().position)
    {
        lowerMotion = &motions.back();
        upperMotion = &motions.back();
    }
    else
    {
        for (std::size_t i = 0; i + 1 < motions.size(); ++i)
        {
            const BlendTree1DMotion &a = motions[i];
            const BlendTree1DMotion &b = motions[i + 1];

            if (parameterValue >= a.position &&
                parameterValue <= b.position)
            {
                lowerMotion = &a;
                upperMotion = &b;
                break;
            }
        }
    }

    if (lowerMotion->clipIndex >= m_clips->size() ||
        upperMotion->clipIndex >= m_clips->size())
    {
        return;
    }

    const AnimationClip &lowerClip =
        (*m_clips)[lowerMotion->clipIndex];

    const AnimationClip &upperClip =
        (*m_clips)[upperMotion->clipIndex];

    if (lowerClip.duration <= 0.0f ||
        upperClip.duration <= 0.0f)
    {
        return;
    }

    m_time += deltaTime * m_playbackSpeed;

    const float lowerTime =
        NormalizeClipTime(m_time, lowerClip.duration);

    const float upperTime =
        NormalizeClipTime(m_time, upperClip.duration);

    m_previousClipIndex = lowerMotion->clipIndex;
    m_currentClipIndex = upperMotion->clipIndex;

    PrepareKeyframeCache(m_previousKeyframeCache, lowerClip);
    PrepareKeyframeCache(m_currentKeyframeCache, upperClip);

    const bool sameMotion =
        lowerMotion->clipIndex == upperMotion->clipIndex;

    const float weight =
        sameMotion
            ? 0.0f
            : InverseLerp(
                  lowerMotion->position,
                  upperMotion->position,
                  parameterValue);

    {
        ScopedTimer timer(
            "Animation sampling",
            &m_timingStats.samplingMs);

        if (sameMotion)
        {
            ResetPoseToBindPose(m_pose);

            SampleClip(
                lowerClip,
                lowerTime,
                m_pose,
                m_previousKeyframeCache);

            m_isBlending = false;
            m_blendElapsed = 0.0f;
            m_blendDuration = 0.0f;
        }
        else
        {
            ResetPoseToBindPose(m_fromPose);
            ResetPoseToBindPose(m_toPose);

            SampleClip(
                lowerClip,
                lowerTime,
                m_fromPose,
                m_previousKeyframeCache);

            SampleClip(
                upperClip,
                upperTime,
                m_toPose,
                m_currentKeyframeCache);

            BlendLocalPoses(
                m_fromPose,
                m_toPose,
                weight,
                m_pose);

            // For debug UI only: this is blend-tree weight,
            // not a transition timer.
            m_isBlending = true;
            m_blendElapsed = weight;
            m_blendDuration = 1.0f;
        }
    }

    {
        ScopedTimer timer(
            "Local-to-global pose",
            &m_timingStats.localToGlobalMs);

        ComputeGlobalPose(m_pose);
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

void Animator::RecomputeGlobalPose()
{
    ComputeGlobalPose(m_pose);
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

    const std::size_t jointCount = m_skeleton->joints.size();

    if (!pose.HasSize(jointCount))
    {
        std::cerr
            << "ResetPoseToBindPose failed: pose was not preallocated.\n";

        return;
    }

    for (std::size_t i = 0; i < jointCount; ++i)
    {
        pose.SetLocal(
            i,
            m_skeleton->joints[i].bindLocalTransform);
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

        const Transform localTransform =
            pose.GetLocal(jointIndex);

        const glm::mat4 localMatrix =
            TransformToMat4(localTransform);

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
    Pose &pose,
    std::vector<std::size_t> &keyframeCache)
{
    if (keyframeCache.size() != clip.channels.size())
    {
        std::cerr
            << "SampleClip failed: keyframe cache was not prepared.\n";

        return;
    }

    for (std::size_t channelIndex = 0;
         channelIndex < clip.channels.size();
         ++channelIndex)
    {
        const AnimationChannel &channel = clip.channels[channelIndex];

        const std::size_t jointCount =
            pose.GetJointCount();

        if (channel.jointIndex < 0 ||
            static_cast<std::size_t>(channel.jointIndex) >= jointCount)
        {
            continue;
        }

        const std::size_t jointIndex =
            static_cast<std::size_t>(channel.jointIndex);

        std::size_t &cachedKeyIndex =
            keyframeCache[channelIndex];

        switch (channel.path)
        {
        case ChannelPath::Translation:
            pose.translations[jointIndex] =
                SampleVec3Channel(
                    channel,
                    time,
                    pose.translations[jointIndex],
                    cachedKeyIndex);
            break;

        case ChannelPath::Rotation:
            pose.rotations[jointIndex] =
                SampleRotationChannel(
                    channel,
                    time,
                    pose.rotations[jointIndex],
                    cachedKeyIndex);
            break;

        case ChannelPath::Scale:
            pose.scales[jointIndex] =
                SampleVec3Channel(
                    channel,
                    time,
                    pose.scales[jointIndex],
                    cachedKeyIndex);
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

    if (!fromPose.HasSize(jointCount) ||
        !toPose.HasSize(jointCount) ||
        !outPose.HasSize(jointCount))
    {
        std::cerr
            << "BlendLocalPoses failed: poses were not preallocated.\n";

        return;
    }

    for (std::size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
    {
        outPose.translations[jointIndex] =
            Engine::Math::LerpVec3(
                fromPose.translations[jointIndex],
                toPose.translations[jointIndex],
                weight);

        outPose.rotations[jointIndex] =
            Engine::Math::NlerpQuat(
                fromPose.rotations[jointIndex],
                toPose.rotations[jointIndex],
                weight);

        outPose.scales[jointIndex] =
            Engine::Math::LerpVec3(
                fromPose.scales[jointIndex],
                toPose.scales[jointIndex],
                weight);
    }
}

void Animator::PrepareKeyframeCache(
    std::vector<std::size_t> &cache,
    const AnimationClip &clip)
{
    if (cache.size() != clip.channels.size())
    {
        cache.assign(clip.channels.size(), 0);
    }
}

void Animator::ResetKeyframeCache(
    std::vector<std::size_t> &cache)
{
    std::fill(cache.begin(), cache.end(), 0);
}

std::size_t Animator::FindCachedKeyframeIndex(
    const std::vector<float> &times,
    float time,
    std::size_t &cachedKeyIndex)
{
    if (times.size() < 2)
    {
        cachedKeyIndex = 0;
        return 0;
    }

    const std::size_t lastValidIndex = times.size() - 2;

    if (time <= times.front())
    {
        cachedKeyIndex = 0;
        return 0;
    }

    if (time >= times.back())
    {
        cachedKeyIndex = lastValidIndex;
        return lastValidIndex;
    }

    if (cachedKeyIndex > lastValidIndex)
    {
        cachedKeyIndex = 0;
    }

    // Animation usually moves forward, so this is usually one cheap step.
    while (cachedKeyIndex < lastValidIndex &&
           time > times[cachedKeyIndex + 1])
    {
        ++cachedKeyIndex;
    }

    // Handles looping, seeking, or random phase changes.
    while (cachedKeyIndex > 0 &&
           time < times[cachedKeyIndex])
    {
        --cachedKeyIndex;
    }

    return cachedKeyIndex;
}

glm::vec3 Animator::SampleVec3Channel(
    const AnimationChannel &channel,
    float time,
    const glm::vec3 &fallback,
    std::size_t &cachedKeyIndex)
{
    if (channel.times.empty() || channel.values.empty())
    {
        return fallback;
    }

    if (channel.times.size() == 1 || channel.values.size() == 1)
    {
        cachedKeyIndex = 0;
        return glm::vec3(channel.values.front());
    }

    if (time <= channel.times.front())
    {
        cachedKeyIndex = 0;
        return glm::vec3(channel.values.front());
    }

    if (time >= channel.times.back())
    {
        cachedKeyIndex = channel.times.size() - 2;
        return glm::vec3(channel.values.back());
    }

    const std::size_t keyIndex =
        FindCachedKeyframeIndex(
            channel.times,
            time,
            cachedKeyIndex);

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
    const glm::quat &fallback,
    std::size_t &cachedKeyIndex)
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
        cachedKeyIndex = 0;
        return ToQuat(channel.values.front());
    }

    if (time <= channel.times.front())
    {
        cachedKeyIndex = 0;
        return ToQuat(channel.values.front());
    }

    if (time >= channel.times.back())
    {
        cachedKeyIndex = channel.times.size() - 2;
        return ToQuat(channel.values.back());
    }

    const std::size_t keyIndex =
        FindCachedKeyframeIndex(
            channel.times,
            time,
            cachedKeyIndex);

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
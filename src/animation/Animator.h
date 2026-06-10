#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "animation/AnimationClip.h"
#include "animation/Pose.h"
#include "animation/Skeleton.h"

struct AnimationTimingStats
{
    double samplingMs = 0.0;
    double localToGlobalMs = 0.0;
};

class Animator
{
public:
    void Initialize(
        const Skeleton *skeleton,
        const std::vector<AnimationClip> *clips);

    // Instant switch.
    void Play(std::size_t clipIndex, bool resetTime = true);

    // Smooth switch.
    void CrossFadeTo(std::size_t clipIndex, float blendDuration);

    void Update(float deltaTime);

    void EvaluateBlendTree1D(
        const std::vector<BlendTree1DMotion> &motions,
        float parameterValue,
        float deltaTime);

    const Pose &GetPose() const { return m_pose; }

    Pose &GetMutablePose() { return m_pose; }

    void RecomputeGlobalPose();

    const AnimationClip *GetCurrentClip() const;
    const AnimationClip *GetPreviousClip() const;

    std::size_t GetCurrentClipIndex() const { return m_currentClipIndex; }

    float GetCurrentTime() const { return m_time; }

    void SetCurrentTime(float time);
    void SetNormalizedTime(float normalizedTime);

    bool IsBlending() const { return m_isBlending; }
    float GetBlendWeight() const;
    float GetBlendDuration() const { return m_blendDuration; }
    float GetBlendElapsed() const { return m_blendElapsed; }

    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float GetPlaybackSpeed() const { return m_playbackSpeed; }

    void SetLooping(bool loop) { m_loop = loop; }
    bool IsLooping() const { return m_loop; }

    const AnimationTimingStats &GetTimingStats() const
    {
        return m_timingStats;
    }

private:
    const Skeleton *m_skeleton = nullptr;
    const std::vector<AnimationClip> *m_clips = nullptr;

    Pose m_pose;

    // Temporary poses used during blending.
    Pose m_fromPose;
    Pose m_toPose;

    std::size_t m_currentClipIndex = 0;
    std::size_t m_previousClipIndex = 0;

    float m_time = 0.0f;
    float m_previousTime = 0.0f;

    bool m_isBlending = false;
    float m_blendElapsed = 0.0f;
    float m_blendDuration = 0.0f;
    float m_playbackSpeed = 1.0f;
    bool m_loop = true;

    AnimationTimingStats m_timingStats;
    std::vector<std::size_t> m_currentKeyframeCache;
    std::vector<std::size_t> m_previousKeyframeCache;

private:
    void ResetPoseToBindPose(Pose &pose);
    void ComputeGlobalPose(Pose &pose);
    float NormalizeClipTime(float time, float duration) const;

    static float InverseLerp(
        float a,
        float b,
        float value);

    float AdvanceClipTime(
        float time,
        float deltaTime,
        float duration,
        std::vector<std::size_t> &keyframeCache);

    void SampleClip(
        const AnimationClip &clip,
        float time,
        Pose &pose,
        std::vector<std::size_t> &keyframeCache);

    void BlendLocalPoses(
        const Pose &fromPose,
        const Pose &toPose,
        float weight,
        Pose &outPose);

    void PrepareKeyframeCache(
        std::vector<std::size_t> &cache,
        const AnimationClip &clip);

    static void ResetKeyframeCache(
        std::vector<std::size_t> &cache);

    static glm::vec3 SampleVec3Channel(
        const AnimationChannel &channel,
        float time,
        const glm::vec3 &fallback,
        std::size_t &cachedKeyIndex);

    static glm::quat SampleRotationChannel(
        const AnimationChannel &channel,
        float time,
        const glm::quat &fallback,
        std::size_t &cachedKeyIndex);

    static std::size_t FindCachedKeyframeIndex(
        const std::vector<float> &times,
        float time,
        std::size_t &cachedKeyIndex);
};
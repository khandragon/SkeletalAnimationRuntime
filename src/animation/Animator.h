#pragma once

#include <cstddef>
#include <vector>

#include "animation/AnimationClip.h"
#include "animation/Pose.h"
#include "animation/Skeleton.h"

class Animator
{
public:
    void Initialize(
        const Skeleton *skeleton,
        const std::vector<AnimationClip> *clips);

    void Play(std::size_t clipIndex, bool resetTime = true);
    void Update(float deltaTime);

    const Pose &GetPose() const { return m_pose; }

    const AnimationClip *GetCurrentClip() const;
    std::size_t GetCurrentClipIndex() const { return m_currentClipIndex; }
    float GetCurrentTime() const { return m_time; }

private:
    const Skeleton *m_skeleton = nullptr;
    const std::vector<AnimationClip> *m_clips = nullptr;

    Pose m_pose;

    std::size_t m_currentClipIndex = 0;
    float m_time = 0.0f;

private:
    void ResetPoseToBindPose();
    void ComputeGlobalPose();

    void SampleClip(
        const AnimationClip &clip,
        float time,
        Pose &pose);

    static glm::vec3 SampleVec3Channel(
        const AnimationChannel &channel,
        float time,
        const glm::vec3 &fallback);

    static glm::quat SampleRotationChannel(
        const AnimationChannel &channel,
        float time,
        const glm::quat &fallback);

    static std::size_t FindKeyframeIndex(
        const std::vector<float> &times,
        float time);
};
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "animation/AnimationClip.h"

struct AnimationGraphState
{
    std::string name;
    std::string clipName;
    bool loop = true;

    std::size_t clipIndex = static_cast<std::size_t>(-1);
};

struct AnimationGraphTransition
{
    std::string from;
    std::string to;
    std::string condition;

    float blendTime = 0.2f;
};

struct AnimationGraph
{
    std::string character;
    std::string model;
    std::string initialState;
    std::string currentState;

    std::unordered_map<std::string, float> parameters;
    std::unordered_map<std::string, AnimationGraphState> states;
    std::vector<AnimationGraphTransition> transitions;
};

AnimationGraph LoadAnimationGraph(const std::string &path);

bool ResolveAnimationGraphClipIndices(
    AnimationGraph &graph,
    const std::vector<AnimationClip> &clips);

void SetAnimationGraphParameter(
    AnimationGraph &graph,
    const std::string &name,
    float value);

float GetAnimationGraphParameter(
    const AnimationGraph &graph,
    const std::string &name);

const AnimationGraphState *FindAnimationGraphState(
    const AnimationGraph &graph,
    const std::string &name);

const AnimationGraphTransition *FindTriggeredTransition(
    const AnimationGraph &graph,
    bool stateFinished);
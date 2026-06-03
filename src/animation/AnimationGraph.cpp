#include "animation/AnimationGraph.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace
{
    std::string Trim(const std::string &text)
    {
        const auto first = text.find_first_not_of(" \t\n\r");

        if (first == std::string::npos)
        {
            return "";
        }

        const auto last = text.find_last_not_of(" \t\n\r");

        return text.substr(first, last - first + 1);
    }

    bool EvaluateCondition(
        const AnimationGraph &graph,
        const std::string &condition,
        bool stateFinished)
    {
        const std::string trimmed = Trim(condition);

        if (trimmed.empty())
        {
            return false;
        }

        if (trimmed == "state finished")
        {
            return stateFinished;
        }

        const std::size_t greaterPos = trimmed.find('>');
        const std::size_t lessPos = trimmed.find('<');

        if (greaterPos != std::string::npos)
        {
            const std::string parameterName =
                Trim(trimmed.substr(0, greaterPos));

            const std::string valueText =
                Trim(trimmed.substr(greaterPos + 1));

            const auto parameterIt =
                graph.parameters.find(parameterName);

            if (parameterIt == graph.parameters.end())
            {
                return false;
            }

            const float compareValue = std::stof(valueText);

            return parameterIt->second > compareValue;
        }

        if (lessPos != std::string::npos)
        {
            const std::string parameterName =
                Trim(trimmed.substr(0, lessPos));

            const std::string valueText =
                Trim(trimmed.substr(lessPos + 1));

            const auto parameterIt =
                graph.parameters.find(parameterName);

            if (parameterIt == graph.parameters.end())
            {
                return false;
            }

            const float compareValue = std::stof(valueText);

            return parameterIt->second < compareValue;
        }

        return false;
    }
}

AnimationGraph LoadAnimationGraph(const std::string &path)
{
    AnimationGraph graph{};

    std::ifstream file(path);

    if (!file.is_open())
    {
        std::cerr << "Failed to open animation graph: " << path << '\n';
        return graph;
    }

    nlohmann::json json;

    try
    {
        file >> json;
    }
    catch (const std::exception &exception)
    {
        std::cerr
            << "Failed to parse animation graph JSON: "
            << exception.what()
            << '\n';

        return graph;
    }

    graph.character = json.value("character", "");
    graph.model = json.value("model", "");
    graph.initialState = json.value("initial_state", "");
    graph.currentState = graph.initialState;

    if (json.contains("parameters"))
    {
        for (const auto &[name, value] : json["parameters"].items())
        {
            graph.parameters[name] = value.get<float>();
        }
    }

    if (json.contains("states"))
    {
        for (const auto &[stateName, stateJson] : json["states"].items())
        {
            AnimationGraphState state{};
            state.name = stateName;
            state.clipName = stateJson.value("clip", "");
            state.loop = stateJson.value("loop", true);

            graph.states[stateName] = state;
        }
    }

    if (json.contains("transitions"))
    {
        for (const auto &transitionJson : json["transitions"])
        {
            AnimationGraphTransition transition{};
            transition.from = transitionJson.value("from", "");
            transition.to = transitionJson.value("to", "");
            transition.condition = transitionJson.value("condition", "");
            transition.blendTime = transitionJson.value("blend_time", 0.2f);

            graph.transitions.push_back(transition);
        }
    }

    std::cout << "Loaded animation graph: " << path << '\n';
    std::cout << "Character: " << graph.character << '\n';
    std::cout << "Model: " << graph.model << '\n';
    std::cout << "Initial state: " << graph.initialState << '\n';
    std::cout << "States: " << graph.states.size() << '\n';
    std::cout << "Transitions: " << graph.transitions.size() << '\n';

    return graph;
}

bool ResolveAnimationGraphClipIndices(
    AnimationGraph &graph,
    const std::vector<AnimationClip> &clips)
{
    bool success = true;

    for (auto &[stateName, state] : graph.states)
    {
        bool found = false;

        for (std::size_t clipIndex = 0; clipIndex < clips.size(); ++clipIndex)
        {
            if (clips[clipIndex].name == state.clipName)
            {
                state.clipIndex = clipIndex;
                found = true;
                break;
            }
        }

        if (!found)
        {
            std::cerr
                << "Animation graph state \""
                << stateName
                << "\" references missing clip \""
                << state.clipName
                << "\"\n";

            success = false;
        }
    }

    return success;
}

void SetAnimationGraphParameter(
    AnimationGraph &graph,
    const std::string &name,
    float value)
{
    graph.parameters[name] = value;
}

float GetAnimationGraphParameter(
    const AnimationGraph &graph,
    const std::string &name)
{
    const auto it = graph.parameters.find(name);

    if (it == graph.parameters.end())
    {
        return 0.0f;
    }

    return it->second;
}

const AnimationGraphState *FindAnimationGraphState(
    const AnimationGraph &graph,
    const std::string &name)
{
    const auto it = graph.states.find(name);

    if (it == graph.states.end())
    {
        return nullptr;
    }

    return &it->second;
}

const AnimationGraphTransition *FindTriggeredTransition(
    const AnimationGraph &graph,
    bool stateFinished)
{
    for (const AnimationGraphTransition &transition : graph.transitions)
    {
        if (transition.from != graph.currentState)
        {
            continue;
        }

        if (EvaluateCondition(graph, transition.condition, stateFinished))
        {
            return &transition;
        }
    }

    return nullptr;
}
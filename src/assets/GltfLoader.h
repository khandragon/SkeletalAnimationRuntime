#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "animation/Skeleton.h"
#include "animation/AnimationClip.h"
#include "render/Mesh.h"

struct StaticMeshData
{
    std::vector<StaticVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct SkinnedMeshData
{
    std::vector<SkinnedVertex> vertices;
    std::vector<std::uint32_t> indices;
};

class GltfLoader
{
public:
    static bool LoadFirstStaticMesh(
        const std::string &path,
        StaticMeshData &outMesh);

    static bool LoadFirstSkeleton(
        const std::string &path,
        Skeleton &outSkeleton);

    static bool LoadAnimationClips(
        const std::string &path,
        std::vector<AnimationClip> &outClips);

    static bool LoadFirstSkinnedMesh(
        const std::string &path,
        SkinnedMeshData &outMesh);
};
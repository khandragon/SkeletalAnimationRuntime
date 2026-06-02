#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "animation/Skeleton.h"
#include "render/Mesh.h"

struct StaticMeshData
{
    std::vector<StaticVertex> vertices;
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
};
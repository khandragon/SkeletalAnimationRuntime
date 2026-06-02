#include "GltfLoader.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

namespace
{
    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c)
                       {
                           return static_cast<char>(std::tolower(c));
                       });

        return value;
    }

    bool EndsWith(const std::string &text, const std::string &suffix)
    {
        if (text.size() < suffix.size())
        {
            return false;
        }

        return text.compare(
                   text.size() - suffix.size(),
                   suffix.size(),
                   suffix) == 0;
    }

    std::size_t GetComponentSizeInBytes(int componentType)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return 1;

        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return 2;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return 4;

        default:
            return 0;
        }
    }

    std::size_t GetNumComponents(int accessorType)
    {
        switch (accessorType)
        {
        case TINYGLTF_TYPE_SCALAR:
            return 1;

        case TINYGLTF_TYPE_VEC2:
            return 2;

        case TINYGLTF_TYPE_VEC3:
            return 3;

        case TINYGLTF_TYPE_VEC4:
            return 4;

        default:
            return 0;
        }
    }

    std::size_t GetAccessorStride(
        const tinygltf::Model &model,
        const tinygltf::Accessor &accessor)
    {
        const tinygltf::BufferView &bufferView =
            model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];

        if (bufferView.byteStride != 0)
        {
            return static_cast<std::size_t>(bufferView.byteStride);
        }

        return GetComponentSizeInBytes(accessor.componentType) *
               GetNumComponents(accessor.type);
    }

    const unsigned char *GetAccessorData(
        const tinygltf::Model &model,
        const tinygltf::Accessor &accessor)
    {
        const tinygltf::BufferView &bufferView =
            model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];

        const tinygltf::Buffer &buffer =
            model.buffers[static_cast<std::size_t>(bufferView.buffer)];

        return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    }

    bool ReadVec3Attribute(
        const tinygltf::Model &model,
        const tinygltf::Primitive &primitive,
        const std::string &attributeName,
        std::vector<glm::vec3> &outValues)
    {
        auto it = primitive.attributes.find(attributeName);

        if (it == primitive.attributes.end())
        {
            return false;
        }

        const tinygltf::Accessor &accessor =
            model.accessors[static_cast<std::size_t>(it->second)];

        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
            accessor.type != TINYGLTF_TYPE_VEC3)
        {
            std::cerr << "Unsupported " << attributeName
                      << " format. Expected FLOAT VEC3.\n";
            return false;
        }

        const unsigned char *data = GetAccessorData(model, accessor);
        const std::size_t stride = GetAccessorStride(model, accessor);

        outValues.resize(static_cast<std::size_t>(accessor.count));

        for (std::size_t i = 0; i < accessor.count; ++i)
        {
            glm::vec3 value{};
            std::memcpy(&value, data + i * stride, sizeof(glm::vec3));
            outValues[i] = value;
        }

        return true;
    }

    bool ReadVec2Attribute(
        const tinygltf::Model &model,
        const tinygltf::Primitive &primitive,
        const std::string &attributeName,
        std::vector<glm::vec2> &outValues)
    {
        auto it = primitive.attributes.find(attributeName);

        if (it == primitive.attributes.end())
        {
            return false;
        }

        const tinygltf::Accessor &accessor =
            model.accessors[static_cast<std::size_t>(it->second)];

        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
            accessor.type != TINYGLTF_TYPE_VEC2)
        {
            std::cerr << "Unsupported " << attributeName
                      << " format. Expected FLOAT VEC2.\n";
            return false;
        }

        const unsigned char *data = GetAccessorData(model, accessor);
        const std::size_t stride = GetAccessorStride(model, accessor);

        outValues.resize(static_cast<std::size_t>(accessor.count));

        for (std::size_t i = 0; i < accessor.count; ++i)
        {
            glm::vec2 value{};
            std::memcpy(&value, data + i * stride, sizeof(glm::vec2));
            outValues[i] = value;
        }

        return true;
    }

    bool ReadIndices(
        const tinygltf::Model &model,
        const tinygltf::Primitive &primitive,
        std::size_t vertexCount,
        std::vector<std::uint32_t> &outIndices)
    {
        if (primitive.indices < 0)
        {
            outIndices.resize(vertexCount);

            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                outIndices[i] = static_cast<std::uint32_t>(i);
            }

            return true;
        }

        const tinygltf::Accessor &accessor =
            model.accessors[static_cast<std::size_t>(primitive.indices)];

        if (accessor.type != TINYGLTF_TYPE_SCALAR)
        {
            std::cerr << "Unsupported index accessor type. Expected SCALAR.\n";
            return false;
        }

        const unsigned char *data = GetAccessorData(model, accessor);
        const std::size_t stride = GetAccessorStride(model, accessor);

        outIndices.resize(static_cast<std::size_t>(accessor.count));

        for (std::size_t i = 0; i < accessor.count; ++i)
        {
            const unsigned char *indexAddress = data + i * stride;

            switch (accessor.componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            {
                std::uint8_t value = 0;
                std::memcpy(&value, indexAddress, sizeof(value));
                outIndices[i] = value;
                break;
            }

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                std::uint16_t value = 0;
                std::memcpy(&value, indexAddress, sizeof(value));
                outIndices[i] = value;
                break;
            }

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            {
                std::uint32_t value = 0;
                std::memcpy(&value, indexAddress, sizeof(value));
                outIndices[i] = value;
                break;
            }

            default:
                std::cerr << "Unsupported index component type.\n";
                return false;
            }
        }

        return true;
    }

    bool LoadGltfModelFromFile(
        const std::string &path,
        tinygltf::Model &model)
    {
        tinygltf::TinyGLTF loader;

        std::string error;
        std::string warning;

        const std::string lowerPath = ToLower(path);

        bool loaded = false;

        if (EndsWith(lowerPath, ".glb"))
        {
            loaded = loader.LoadBinaryFromFile(
                &model,
                &error,
                &warning,
                path);
        }
        else if (EndsWith(lowerPath, ".gltf"))
        {
            loaded = loader.LoadASCIIFromFile(
                &model,
                &error,
                &warning,
                path);
        }
        else
        {
            std::cerr << "Unsupported file extension: " << path << '\n';
            return false;
        }

        if (!warning.empty())
        {
            std::cout << "glTF warning: " << warning << '\n';
        }

        if (!error.empty())
        {
            std::cerr << "glTF error: " << error << '\n';
        }

        if (!loaded)
        {
            std::cerr << "Failed to load glTF file: " << path << '\n';
            return false;
        }

        return true;
    }

    Transform ReadNodeLocalTransform(const tinygltf::Node &node)
    {
        Transform transform{};

        if (node.translation.size() == 3)
        {
            transform.translation = glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]));
        }

        if (node.rotation.size() == 4)
        {
            // glTF stores quaternion as x, y, z, w.
            // GLM constructor wants w, x, y, z.
            transform.rotation = glm::quat(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]));
        }

        if (node.scale.size() == 3)
        {
            transform.scale = glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2]));
        }

        return transform;
    }

    bool ReadInverseBindMatrices(
        const tinygltf::Model &model,
        const tinygltf::Skin &skin,
        std::vector<glm::mat4> &outMatrices)
    {
        outMatrices.clear();

        if (skin.inverseBindMatrices < 0)
        {
            outMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
            return true;
        }

        const tinygltf::Accessor &accessor =
            model.accessors[static_cast<std::size_t>(skin.inverseBindMatrices)];

        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
            accessor.type != TINYGLTF_TYPE_MAT4)
        {
            std::cerr << "Unsupported inverseBindMatrices format. Expected FLOAT MAT4.\n";
            return false;
        }

        const unsigned char *data = GetAccessorData(model, accessor);
        const std::size_t stride = GetAccessorStride(model, accessor);

        outMatrices.resize(static_cast<std::size_t>(accessor.count));

        for (std::size_t i = 0; i < accessor.count; ++i)
        {
            glm::mat4 matrix{1.0f};
            std::memcpy(&matrix, data + i * stride, sizeof(glm::mat4));
            outMatrices[i] = matrix;
        }

        return true;
    }

    void BuildNodeParentTable(
        const tinygltf::Model &model,
        std::vector<int> &outNodeParents)
    {
        outNodeParents.assign(model.nodes.size(), -1);

        for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
        {
            const tinygltf::Node &node = model.nodes[nodeIndex];

            for (int childNodeIndex : node.children)
            {
                if (childNodeIndex >= 0 &&
                    childNodeIndex < static_cast<int>(outNodeParents.size()))
                {
                    outNodeParents[static_cast<std::size_t>(childNodeIndex)] =
                        static_cast<int>(nodeIndex);
                }
            }
        }
    }
}

bool GltfLoader::LoadFirstStaticMesh(
    const std::string &path,
    StaticMeshData &outMesh)
{
    outMesh.vertices.clear();
    outMesh.indices.clear();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;

    std::string error;
    std::string warning;

    const std::string lowerPath = ToLower(path);

    bool loaded = false;

    if (EndsWith(lowerPath, ".glb"))
    {
        loaded = loader.LoadBinaryFromFile(
            &model,
            &error,
            &warning,
            path);
    }
    else if (EndsWith(lowerPath, ".gltf"))
    {
        loaded = loader.LoadASCIIFromFile(
            &model,
            &error,
            &warning,
            path);
    }
    else
    {
        std::cerr << "Unsupported file extension: " << path << '\n';
        return false;
    }

    if (!warning.empty())
    {
        std::cout << "glTF warning: " << warning << '\n';
    }

    if (!error.empty())
    {
        std::cerr << "glTF error: " << error << '\n';
    }

    if (!loaded)
    {
        std::cerr << "Failed to load glTF file: " << path << '\n';
        return false;
    }

    if (model.meshes.empty())
    {
        std::cerr << "glTF file contains no meshes.\n";
        return false;
    }

    const tinygltf::Mesh &gltfMesh = model.meshes[0];

    for (const tinygltf::Primitive &primitive : gltfMesh.primitives)
    {
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
        {
            continue;
        }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;

        if (!ReadVec3Attribute(model, primitive, "POSITION", positions))
        {
            std::cerr << "Mesh primitive has no POSITION attribute.\n";
            continue;
        }

        const bool hasNormals =
            ReadVec3Attribute(model, primitive, "NORMAL", normals);

        const bool hasTexCoords =
            ReadVec2Attribute(model, primitive, "TEXCOORD_0", texCoords);

        outMesh.vertices.resize(positions.size());

        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            StaticVertex vertex{};
            vertex.position = positions[i];

            if (hasNormals && i < normals.size())
            {
                vertex.normal = normals[i];
            }

            if (hasTexCoords && i < texCoords.size())
            {
                vertex.texCoord = texCoords[i];
            }

            outMesh.vertices[i] = vertex;
        }

        if (!ReadIndices(
                model,
                primitive,
                outMesh.vertices.size(),
                outMesh.indices))
        {
            return false;
        }

        std::cout << "Loaded mesh from " << path << '\n';
        std::cout << "Vertices: " << outMesh.vertices.size() << '\n';
        std::cout << "Indices: " << outMesh.indices.size() << '\n';

        return true;
    }

    std::cerr << "No triangle mesh primitive found.\n";
    return false;
}

bool GltfLoader::LoadFirstSkeleton(
    const std::string &path,
    Skeleton &outSkeleton)
{
    outSkeleton.joints.clear();

    tinygltf::Model model;

    if (!LoadGltfModelFromFile(path, model))
    {
        return false;
    }

    if (model.skins.empty())
    {
        std::cerr << "glTF file contains no skins/skeletons: " << path << '\n';
        return false;
    }

    const tinygltf::Skin &skin = model.skins[0];

    if (skin.joints.empty())
    {
        std::cerr << "First skin contains no joints.\n";
        return false;
    }

    std::vector<glm::mat4> inverseBindMatrices;

    if (!ReadInverseBindMatrices(model, skin, inverseBindMatrices))
    {
        return false;
    }

    std::vector<int> nodeParents;
    BuildNodeParentTable(model, nodeParents);

    // Maps glTF node index -> our joint index.
    std::vector<int> nodeToJoint(model.nodes.size(), -1);

    for (std::size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
    {
        const int nodeIndex = skin.joints[jointIndex];

        if (nodeIndex < 0 ||
            nodeIndex >= static_cast<int>(model.nodes.size()))
        {
            std::cerr << "Invalid joint node index: " << nodeIndex << '\n';
            return false;
        }

        nodeToJoint[static_cast<std::size_t>(nodeIndex)] =
            static_cast<int>(jointIndex);
    }

    outSkeleton.joints.resize(skin.joints.size());

    for (std::size_t jointIndex = 0; jointIndex < skin.joints.size(); ++jointIndex)
    {
        const int nodeIndex = skin.joints[jointIndex];
        const tinygltf::Node &node = model.nodes[static_cast<std::size_t>(nodeIndex)];

        Joint joint{};

        if (!node.name.empty())
        {
            joint.name = node.name;
        }
        else
        {
            joint.name = "Joint_" + std::to_string(jointIndex);
        }

        const int parentNodeIndex =
            nodeParents[static_cast<std::size_t>(nodeIndex)];

        if (parentNodeIndex >= 0 &&
            parentNodeIndex < static_cast<int>(nodeToJoint.size()))
        {
            joint.parent =
                nodeToJoint[static_cast<std::size_t>(parentNodeIndex)];
        }
        else
        {
            joint.parent = -1;
        }

        if (jointIndex < inverseBindMatrices.size())
        {
            joint.inverseBindMatrix = inverseBindMatrices[jointIndex];
        }
        else
        {
            joint.inverseBindMatrix = glm::mat4(1.0f);
        }

        joint.bindLocalTransform = ReadNodeLocalTransform(node);

        outSkeleton.joints[jointIndex] = joint;
    }

    std::cout << "Loaded skeleton from " << path << '\n';
    std::cout << "Joint count: " << outSkeleton.joints.size() << '\n';

    return true;
}
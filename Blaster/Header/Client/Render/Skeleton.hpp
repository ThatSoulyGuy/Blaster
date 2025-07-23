#pragma once

#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
#include "Independent/Math/Matrix.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::Render
{
    struct BoneInformation
    {
        uint32_t parent{ std::numeric_limits<uint32_t>::max() };

        Matrix<float, 4, 4> offset = Matrix<float, 4, 4>::Identity();
        Matrix<float, 4, 4> localBind = Matrix<float, 4, 4>::Identity();
        Matrix<float, 4, 4> globalAnimated = Matrix<float, 4, 4>::Identity();

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & parent;
            archive & offset;
            archive & localBind;
            archive & globalAnimated;
        }
    };

    struct Skeleton
    {
        std::unordered_map<std::string, std::size_t> boneIndex;
        std::vector<BoneInformation> bones;

        int32_t AddBone(const std::string& name, const aiMatrix4x4& offset, const aiMatrix4x4& localBind, uint32_t parent)
        {
            uint32_t id = static_cast<uint32_t>(bones.size());
            boneIndex[name] = id;

            BoneInformation information;

            information.parent = parent;
            information.offset = Matrix<float, 4, 4>::FromAssimpMatrix(offset);
            information.localBind = Matrix<float, 4, 4>::FromAssimpMatrix(localBind);
            information.globalAnimated = Matrix<float, 4, 4>::Identity();

            bones.push_back(information);

            return id;
        }

        uint32_t GetOrAdd(const std::string& name, const aiMatrix4x4& offset, const aiMatrix4x4& localBind, uint32_t parent)
        {
            if (auto iterator = boneIndex.find(name); iterator != boneIndex.end())
            {
                BoneInformation& information = bones[iterator->second];

                if (information.offset == Matrix<float, 4, 4>::Identity())
                    information.offset = Matrix<float, 4, 4>::FromAssimpMatrix(offset);

                if (information.localBind == Matrix<float, 4, 4>::Identity())
                    information.localBind = Matrix<float, 4, 4>::FromAssimpMatrix(localBind);

                if (information.parent == std::numeric_limits<uint32_t>::max() && parent != std::numeric_limits<uint32_t>::max())
                    information.parent = parent;

                return iterator->second;
            }

            return AddBone(name, offset, localBind, parent);
        }

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boneIndex;
            archive & bones;
        }
    };
}

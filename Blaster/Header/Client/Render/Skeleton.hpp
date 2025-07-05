#pragma once

#include <string>
#include <assimp/matrix4x4.h>

#include "Independent/Math/Matrix.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::Render
{
    struct BoneInfo
    {
        Matrix<float, 4, 4> offset;
        Matrix<float, 4, 4> finalPose;
    };

    struct Skeleton
    {
        std::unordered_map<std::string, std::size_t> boneIndex;
        std::vector<BoneInfo> bones;

        std::size_t AddBone(const std::string& name, const aiMatrix4x4& offset)
        {
            const std::size_t id = bones.size();

            boneIndex[name] = id;

            BoneInfo bone;

            bone.offset = *reinterpret_cast<const Matrix<float,4,4>*>(&offset);
            bone.finalPose = Matrix<float,4,4>::Identity();

            bones.push_back(bone);

            return id;
        }

        std::size_t GetOrAdd(const std::string& name, const aiMatrix4x4& offset)
        {
            if (const auto iterator = boneIndex.find(name); iterator != boneIndex.end())
                return iterator->second;

            return AddBone(name, offset);
        }

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boneIndex;
            archive & bones;
        }
    };
}

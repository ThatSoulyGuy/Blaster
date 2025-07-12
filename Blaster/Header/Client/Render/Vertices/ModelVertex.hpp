#pragma once

#include "Client/Render/Mesh.hpp"
#include "Client/Render/Vertex.hpp"
#include "Independent/Math/Vector.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::Render::Vertices
{
    struct ModelVertex final
    {
        static VertexBufferLayout GetLayout()
        {
            VertexBufferLayout result;

            result.Add(0, 3, GL_FLOAT, offsetof(ModelVertex, position));
            result.Add(1, 4, GL_FLOAT, offsetof(ModelVertex, color));
            result.Add(2, 2, GL_FLOAT, offsetof(ModelVertex, uv0));
            result.Add(3, 4, GL_UNSIGNED_INT, offsetof(ModelVertex, boneIds));
            result.Add(4, 4, GL_FLOAT, offsetof(ModelVertex, weights));

            return result;
        }

        [[nodiscard]]
        bool operator==(const ModelVertex& other) const
        {
            return OPERATOR_CHECK(position, color, uv0, boneIds, weights);
        }

        [[nodiscard]]
        bool operator!=(const ModelVertex& other) const
        {
            return !(*this == other);
        }

        template <typename Archive>
        void serialize(Archive &archive, const unsigned)
        {
            archive & boost::serialization::make_nvp("position", position);
            archive & boost::serialization::make_nvp("color", color);
            archive & boost::serialization::make_nvp("uv0", uv0);
            archive & boost::serialization::make_nvp("boneIds", boneIds);
            archive & boost::serialization::make_nvp("weights", weights);
        }

        Vector<float, 3> position;
        Vector<float, 4> color{1,1,1,1};
        Vector<float, 2> uv0;

        Vector<uint32_t, 4> boneIds;
        Vector<float, 4> weights;
    };
}

BOOST_CLASS_EXPORT(Blaster::Client::Render::Vertices::ModelVertex)
REGISTER_COMPONENT(Blaster::Client::Render::Mesh<Blaster::Client::Render::Vertices::ModelVertex>, 25567)
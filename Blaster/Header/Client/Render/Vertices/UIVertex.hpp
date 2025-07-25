#pragma once

#include "Client/Render/Mesh.hpp"
#include "Client/Render/Vertex.hpp"
#include "Independent/Math/Vector.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::Render::Vertices
{
    struct UIVertex final
    {
        static VertexBufferLayout GetLayout()
        {
            VertexBufferLayout result;

            result.Add(0, 3, GL_FLOAT, offsetof(UIVertex, position));
            result.Add(1, 3, GL_FLOAT, offsetof(UIVertex, color));
            result.Add(2, 2, GL_FLOAT, offsetof(UIVertex, uvs));

            return result;
        }

        [[nodiscard]]
        bool operator==(const UIVertex& other) const
        {
            return OPERATOR_CHECK(position, color, uvs);
        }

        [[nodiscard]]
        bool operator!=(const UIVertex& other) const
        {
            return !(*this == other);
        }

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive& boost::serialization::make_nvp("position", position);
            archive& boost::serialization::make_nvp("color", color);
            archive& boost::serialization::make_nvp("uvs", uvs);
        }

        Vector<float, 3> position;
        Vector<float, 3> color{ 1.0f, 1.0f, 1.0f };
        Vector<float, 2> uvs;
    };
}

BOOST_CLASS_EXPORT(Blaster::Client::Render::Vertices::UIVertex)
REGISTER_COMPONENT(Blaster::Client::Render::Mesh<Blaster::Client::Render::Vertices::UIVertex>, 29867)
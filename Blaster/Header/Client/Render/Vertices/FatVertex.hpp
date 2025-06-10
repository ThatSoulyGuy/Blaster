#pragma once

#include "Client/Render/Mesh.hpp"
#include "Client/Render/Vertex.hpp"
#include "Independent/Math/Vector.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Client::Render::Vertices
{
    struct FatVertex final
    {
        FatVertex(const Vector<float, 3> position, const Vector<float, 3> color, const Vector<float, 3> normal, const Vector<float, 2> uvs) : position(position), color(color), normal(normal), uvs(uvs) { }

        FatVertex() = default;

        static VertexBufferLayout GetLayout()
        {
            VertexBufferLayout layout;

            layout.Add(0, 3, GL_FLOAT, offsetof(FatVertex, position));
            layout.Add(1, 3, GL_FLOAT, offsetof(FatVertex, color));
            layout.Add(2, 3, GL_FLOAT, offsetof(FatVertex, normal));
            layout.Add(3, 2, GL_FLOAT, offsetof(FatVertex, uvs));

            return layout;
        }

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::make_nvp("position", position);
            archive & boost::serialization::make_nvp("color", color);
            archive & boost::serialization::make_nvp("normal", normal);
            archive & boost::serialization::make_nvp("uvs", uvs);
        }

        Vector<float, 3> position;
        Vector<float, 3> color;
        Vector<float, 3> normal;
        Vector<float, 2> uvs;
    };
}

BOOST_CLASS_EXPORT(Blaster::Client::Render::Vertices::FatVertex)
REGISTER_COMPONENT(Blaster::Client::Render::Mesh<Blaster::Client::Render::Vertices::FatVertex>)
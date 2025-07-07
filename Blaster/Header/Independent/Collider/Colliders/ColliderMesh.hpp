#pragma once

#include "Independent/Collider/ColliderBase.hpp"
#include "Independent/ECS/GameObject.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Collider::Colliders
{
    class ColliderMesh final : public ColliderBase
    {

    public:

        void Initialize() override
        {
            mesh = std::make_unique<btTriangleMesh>();

            for (std::size_t i = 0; i < indices.size(); i += 3)
            {
                const auto& a = vertices[indices[i]];
                const auto& b = vertices[indices[i + 1]];
                const auto& c_ = vertices[indices[i + 2]];

                mesh->addTriangle({ a.x(),a.y(),a.z() }, { b.x(),b.y(),b.z() }, { c_.x(),c_.y(),c_.z() }, true);

                mesh->addTriangle({ c_.x(), c_.y(), c_.z()}, {b.x(), b.y(), b.z()}, {a.x(), a.y(), a.z()}, true);
            }

            shape = std::make_unique<btBvhTriangleMeshShape>(mesh.get(), true);

            shape->setMargin(0.0f);
        }

        static std::shared_ptr<ColliderMesh> Create(const std::vector<Vector<float, 3>>& vertices, const std::vector<std::uint32_t>& indices)
        {
            std::shared_ptr<ColliderMesh> result(new ColliderMesh);

            result->vertices = vertices;
            result->indices = indices;

            return result;
        }

    private:

        ColliderMesh() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(vertices);
            archive & BOOST_SERIALIZATION_NVP(indices);
        }

        std::vector<Vector<float, 3>> vertices;
        std::vector<std::uint32_t> indices;

        std::unique_ptr<btTriangleMesh> mesh;

        DESCRIBE_AND_REGISTER(ColliderMesh, (ColliderBase), (), (), (vertices, indices))
    };
}

REGISTER_COMPONENT(Blaster::Independent::Collider::Colliders::ColliderMesh)
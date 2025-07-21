#pragma once

#include "Independent/Math/Vector.hpp"
#include "Independent/Physics/Collider.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Physics::Colliders
{
    class ColliderMesh final : public Collider
    {

    public:

        void Initialize() override
        {
            auto* triangleMesh = new btTriangleMesh();

            for (std::size_t i = 0; i < indices.size(); i += 3)
            {
                const auto& first = vertices[indices[i]];
                const auto& second = vertices[indices[i + 1]];
                const auto& third = vertices[indices[i + 2]];

                triangleMesh->addTriangle(btVector3(first.x(), first.y(), first.z()),
                    btVector3(second.x(), second.y(), second.z()),
                    btVector3(third.x(), third.y(), third.z()));

                triangleMesh->addTriangle(btVector3(third.x(), third.y(), third.z()),
                    btVector3(second.x(), second.y(), second.z()),
                    btVector3(first.x(), first.y(), first.z()));
            }

            shape = new btBvhTriangleMeshShape(triangleMesh, true);
        }

        static std::shared_ptr<ColliderMesh> Create(const std::vector<Vector<float, 3>>& vertices, const std::vector<std::uint32_t>& indices, bool shouldSynchronize = true)
        {
            std::shared_ptr<ColliderMesh> result(new ColliderMesh());

            result->vertices = vertices;
            result->indices = indices;
            result->shouldSerialize = shouldSynchronize;

            return result;
        }

        [[nodiscard]]
        COLLIDER_TYPE GetColliderType() const override
        {
            return COLLIDER_TYPE::MESH;
        }

    private:

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
            
            archive & BOOST_SERIALIZATION_NVP(shouldSerialize);

            if (shouldSerialize)
            {
                archive & BOOST_SERIALIZATION_NVP(vertices);
                archive & BOOST_SERIALIZATION_NVP(indices);
            }
        }

        std::vector<Vector<float, 3>> vertices;
        std::vector<unsigned int> indices;

        bool shouldSerialize = true;

        DESCRIBE_AND_REGISTER(ColliderMesh, (Collider), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Independent::Physics::Colliders::ColliderMesh, 92877)
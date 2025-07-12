#pragma once

#include "Independent/Collider/ColliderBase.hpp"
#include "Independent/ECS/GameObject.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Collider::Colliders
{
    class ColliderBox final : public ColliderBase
    {

    public:

        void Initialize() override
        {
            shape = std::make_unique<btBoxShape>(btVector3 { dimensions.x() / 2, dimensions.y() / 2, dimensions.z() / 2});
        }

        static std::shared_ptr<ColliderBox> Create(const Vector<float, 3>& dimensions)
        {
            std::shared_ptr<ColliderBox> result(new ColliderBox());

            result->dimensions = dimensions;

            return result;
        }

    private:

        ColliderBox() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(dimensions);
        }

        Vector<float, 3> dimensions;

        DESCRIBE_AND_REGISTER(ColliderBox, (ColliderBase), (), (), (dimensions))
    };
}

REGISTER_COMPONENT(Blaster::Independent::Collider::Colliders::ColliderBox, 97472)
#pragma once

#include "Independent/Collider/ColliderBase.hpp"
#include "Independent/ECS/GameObject.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Collider::Colliders
{
    class ColliderCapsule final : public ColliderBase
    {

    public:

        void Initialize() override
        {
            shape = std::make_unique<btCapsuleShape>(radius, height);
        }

        static std::shared_ptr<ColliderCapsule> Create(const float radius, const float height)
        {
            std::shared_ptr<ColliderCapsule> result(new ColliderCapsule());

            result->radius = radius;
            result->height = height;

            return result;
        }

    private:

        ColliderCapsule() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(radius);
            archive & BOOST_SERIALIZATION_NVP(height);
        }

        float radius;
        float height;

        DESCRIBE_AND_REGISTER(ColliderCapsule, (ColliderBase), (), (), (radius, height))
    };
}

REGISTER_COMPONENT(Blaster::Independent::Collider::Colliders::ColliderCapsule)
#pragma once

#include "Independent/Math/Vector.hpp"
#include "Independent/Physics/Collider.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Physics::Colliders
{
    class ColliderCapsule final : public Collider
    {

    public:

        void Initialize() override
        {
            shape = new btCapsuleShape(radius, height);
        }

        static std::shared_ptr<ColliderCapsule> Create(float radius, float height)
        {
            std::shared_ptr<ColliderCapsule> result(new ColliderCapsule());

            result->radius = radius;
            result->height = height;

            return result;
        }

        [[nodiscard]]
        COLLIDER_TYPE GetColliderType() const override
        {
            return COLLIDER_TYPE::CAPSULE;
        }

    private:

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
            
            archive & BOOST_SERIALIZATION_NVP(radius);
            archive & BOOST_SERIALIZATION_NVP(height);
        }

        float radius, height;

        DESCRIBE_AND_REGISTER(ColliderCapsule, (Collider), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Independent::Physics::Colliders::ColliderCapsule, 90832)
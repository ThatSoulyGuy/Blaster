#pragma once

#include "Independent/Math/Vector.hpp"
#include "Independent/Physics/Collider.hpp"

using namespace Blaster::Independent::Math;

namespace Blaster::Independent::Physics::Colliders
{
    class ColliderBox final : public Collider
    {

    public:

        void Initialize() override
        {
            shape = new btBoxShape(btVector3(extent.x() / 2, extent.y() / 2, extent.z() / 2));
        }

        static std::shared_ptr<ColliderBox> Create(const Vector<float, 3>& extent)
        {
            std::shared_ptr<ColliderBox> result(new ColliderBox());

            result->extent = extent;

            return result;
        }

        [[nodiscard]]
        COLLIDER_TYPE GetColliderType() const override
        {
            return COLLIDER_TYPE::BOX;
        }

    private:

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
            
            archive & BOOST_SERIALIZATION_NVP(extent);
        }

        Vector<float, 3> extent;

        DESCRIBE_AND_REGISTER(ColliderBox, (Collider), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Independent::Physics::Colliders::ColliderBox, 83729)
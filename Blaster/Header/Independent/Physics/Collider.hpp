#pragma once

#include <btBulletDynamicsCommon.h>
#include "Independent/ECS/Component.hpp"

using namespace Blaster::Independent::ECS;

namespace Blaster::Independent::Physics
{
    enum class COLLIDER_TYPE
    {
        BOX, CAPSULE, MESH
    };

    class Collider : public Component
    {

    public:

        virtual ~Collider() { delete shape; }

        [[nodiscard]]
        btCollisionShape* GetShape() const
        {
            return shape;
        }

        [[nodiscard]]
        virtual COLLIDER_TYPE GetColliderType() const = 0;

    protected:

        btCollisionShape* shape = nullptr;

        DESCRIBE_AND_REGISTER(Collider, (Component), (), (), ())

    };
}
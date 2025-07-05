#pragma once

#include <btBulletDynamicsCommon.h>
#include "Independent/ECS/Component.hpp"

namespace Blaster::Independent::Math
{
    class Rigidbody;
}

using namespace Blaster::Independent::ECS;

namespace Blaster::Independent::Collider
{
    class ColliderBase : public Component
    {

    public:

        virtual ~ColliderBase() = default;

        btCollisionShape* GetShape() const
        {
            return shape.get();
        }

    protected:

        std::unique_ptr<btCollisionShape> shape;

        DESCRIBE_AND_REGISTER(ColliderBase, (Component), (), (), ())

    };
}
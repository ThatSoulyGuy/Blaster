#pragma once

#include "Server/Entity/EntityBase.hpp"

namespace Blaster::Server::Entity
{
    class LivingEntity : public EntityBase
    {

    public:

        [[nodiscard]]
        virtual float GetMovementSpeed() const = 0;

        [[nodiscard]]
        virtual float GetRunningMultiplier() const = 0;

        [[nodiscard]]
        virtual float GetJumpHeight() const { return 0.0f; }

        [[nodiscard]]
        virtual bool GetCanJump() const = 0;

    private:

        DESCRIBE_AND_REGISTER(LivingEntity, (EntityBase), (), (), ())

    };
}

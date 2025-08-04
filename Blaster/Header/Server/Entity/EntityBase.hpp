#pragma once

#include "Independent/ECS/Component.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Server::Entity
{
    template <typename T>
    class EntityBase : public Component
    {

    public:

        [[nodiscard]]
        virtual std::string GetRegistryName() const = 0;

        [[nodiscard]]
        virtual float GetCurrentHealth() const = 0;

        [[nodiscard]]
        virtual float GetMaximumHealth() const = 0;

        [[nodiscard]]
        virtual float GetMovementSpeed() const = 0;

        [[nodiscard]]
        virtual float GetRunningMultiplier() const = 0;

        [[nodiscard]]
        virtual float GetJumpHeight() const { return 0.0f; }

        [[nodiscard]]
        virtual bool GetCanJump() const = 0;

    private:

        DESCRIBE_AND_REGISTER(EntityBase<T>, (Component), (), (), ())

    };
}

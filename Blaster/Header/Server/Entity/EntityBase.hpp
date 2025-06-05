#pragma once

#include "Independent/ECS/Component.hpp"
#include "Independent/Utility/Builder.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Server::Entity
{
    template <typename T>
    class EntityBase : public Component
    {

    public:

        [[nodiscard]]
        std::string GetRegistryName() const
        {
            return RegistryName;
        }

        [[nodiscard]]
        float GetCurrentHealth() const
        {
            return CurrentHealth;
        }

        [[nodiscard]]
        float GetMaximumHealth() const
        {
            return MaximumHealth;
        }

        [[nodiscard]]
        float GetMovementSpeed() const
        {
            return MovementSpeed;
        }

        [[nodiscard]]
        float GetRunningMultiplier() const
        {
            return RunningMultiplier;
        }

        [[nodiscard]]
        float GetJumpHeight() const
        {
            return JumpHeight;
        }

        [[nodiscard]]
        bool GetCanJump() const
        {
            return CanJump;
        }

        void TakeDamage(const float amount)
        {
            CurrentHealth -= abs(amount);
        }

        void Heal(const float amount)
        {
            CurrentHealth += abs(amount);
        }

    protected:

        friend class Builder<EntityBase>;

        template <typename Class, typename MemberType, MemberType Class::* MemberPtr>
        friend struct Setter;

        BUILDABLE_PROPERTY(RegistryName, std::string, EntityBase)

        BUILDABLE_PROPERTY(CurrentHealth, float, EntityBase)
        BUILDABLE_PROPERTY(MaximumHealth, float, EntityBase)

        BUILDABLE_PROPERTY(MovementSpeed, float, EntityBase)
        BUILDABLE_PROPERTY(RunningMultiplier, float, EntityBase)

        BUILDABLE_PROPERTY(JumpHeight, float, EntityBase)
        BUILDABLE_PROPERTY(CanJump, bool, EntityBase)

    };
}

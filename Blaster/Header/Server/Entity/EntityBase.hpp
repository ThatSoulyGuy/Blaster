#pragma once

#include "Independent/ECS/Component.hpp"

namespace Blaster::Independent::ECS
{
    class GameObject;
}

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Server::Entity
{
    class EntityBase : public Component
    {

    public:

        enum class Team
        {
            RED,
            BLUE,
            NONE
        };

        [[nodiscard]]
        virtual std::string GetRegistryName() const = 0;

        [[nodiscard]]
        virtual Team GetTeam() const = 0;

        [[nodiscard]]
        virtual std::uint8_t GetCurrentHealth() const = 0;

        [[nodiscard]]
        virtual std::uint8_t GetMaximumHealth() const = 0;

        virtual void DealDamage(std::uint8_t) = 0;

        virtual void HealDamage(std::uint8_t) = 0;

        [[nodiscard]]
        virtual std::optional<std::shared_ptr<GameObject>> GetEntityModel() const { return std::nullopt; }

    private:

        DESCRIBE_AND_REGISTER(EntityBase, (Component), (), (), ())

    };
}

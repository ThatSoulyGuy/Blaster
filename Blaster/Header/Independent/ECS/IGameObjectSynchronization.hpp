#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>

namespace Blaster::Independent::ECS
{
    class Component;

    struct IGameObjectSynchronization //TODO: I don't like this, feels hacky
    {
        virtual ~IGameObjectSynchronization() = default;

        [[nodiscard]]
        virtual bool WasJustCreated() const noexcept = 0;

        [[nodiscard]]
        virtual bool IsDestroyed() const noexcept = 0;

        [[nodiscard]]
        virtual std::string GetAbsolutePath() const = 0;

        [[nodiscard]]
        virtual std::string GetTypeName() const = 0;

        [[nodiscard]]
        virtual void MarkDestroyed() noexcept = 0;

        [[nodiscard]]
        virtual const std::unordered_map<std::type_index, std::shared_ptr<Component>>& GetComponentMap() const = 0;

        [[nodiscard]]
        virtual const std::unordered_map<std::string, std::shared_ptr<GameObject>>& GetChildMap() const = 0;
    };
}
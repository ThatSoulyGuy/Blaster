#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <boost/serialization/shared_ptr.hpp>
#include "Independent/ECS/Component.hpp"
#include "Independent/Utility/TypeRegistrar.hpp"

namespace Blaster::Independent::ECS
{
    class ComponentFactory
    {

    public:

        using Creator = std::function<std::shared_ptr<Component>()>;

        template <typename T>
        static void Register()
        {
            const std::uint64_t key = Blaster::Independent::Utility::TypeRegistrar::GetTypeId<T>();

            std::lock_guard guard(GetMutex());

            GetRegistry()[key] = []()
            {
                return std::static_pointer_cast<Component>(std::shared_ptr<T>(new T()));
            };
        }

        static std::shared_ptr<Component> Instantiate(const std::uint64_t& id)
        {
            std::lock_guard guard(GetMutex());

            auto& registry = GetRegistry();
            const auto iterator = registry.find(id);

            if (iterator == registry.end())
                return nullptr;

            return iterator->second();
        }

    private:

        static std::unordered_map<std::uint64_t, Creator>& GetRegistry()
        {
            static std::unordered_map<std::uint64_t, Creator> registry;

            return registry;
        }

        static std::mutex& GetMutex()
        {
            static std::mutex mutex;

            return mutex;
        }
    };
}
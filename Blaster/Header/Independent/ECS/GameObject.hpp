#pragma once

#include <iostream>
#include <typeindex>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include "Client/Network/ClientNetwork.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/IGameObjectSynchronization.hpp"
#include "Independent/Math/Transform.hpp"
#include "Independent/Network/CommonNetwork.hpp"
#include "Server/Network/ServerSynchronization.hpp"

using namespace Blaster::Client::Network;
using namespace Blaster::Independent::Math;
using namespace Blaster::Independent::Network;

namespace Blaster::Independent::ECS
{
    class GameObjectManager;

    class GameObject final : public std::enable_shared_from_this<GameObject>, public IGameObjectSynchronization
    {

    public:

        GameObject(const GameObject&) = delete;
        GameObject(GameObject&&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject& operator=(GameObject&&) = delete;

        template <typename T> requires (std::is_base_of_v<Component, T>)
        std::shared_ptr<T> AddComponent(std::shared_ptr<T> component)
        {
            if (componentMap.contains(typeid(T)))
            {
                std::cout << "Component map for game object '" << name << "' already contains component '" << typeid(T).name() << "'!" << std::endl;
                return nullptr;
            }

            component->gameObject = shared_from_this();
            component->Initialize();

            componentMap.insert({ typeid(T), std::move(component) });

            componentMap[typeid(T)]->wasAdded = true;

#ifdef IS_SERVER
            Blaster::Server::Network::ServerSynchronization::MarkDirty(shared_from_this());
#endif

            return std::static_pointer_cast<T>(componentMap[typeid(T)]);
        }

        std::shared_ptr<Component> AddComponentDynamic(std::shared_ptr<Component> component)
        {
            const auto type = std::type_index(typeid(*component));

            if (componentMap.contains(type))
            {
                std::cout << "Component map for game object '" << name << "' already contains component '" << type.name() << "'!" << std::endl;
                return nullptr;
            }

            component->gameObject = shared_from_this();
            component->Initialize();

            componentMap.insert({ type, std::move(component) });

            componentMap[type]->wasAdded = true;

#ifdef IS_SERVER
            Blaster::Server::Network::ServerSynchronization::MarkDirty(shared_from_this());
#endif

            return componentMap[type];
        }

        template <typename T> requires (std::is_base_of_v<Component, T>)
        bool HasComponent() const
        {
            return componentMap.contains(typeid(T));
        }

        bool HasComponentDynamic(const std::string& typeName) const
        {
            return [this, typeName]
            {
                for (const auto& component : componentMap | std::views::values)
                {
                    if (component->GetTypeName() == typeName)
                        return true;
                }

                return false;
            }();
        }

        template <typename T> requires (std::is_base_of_v<Component, T>)
        std::optional<std::shared_ptr<T>> GetComponent()
        {
            if (!componentMap.contains(typeid(T)))
            {
                std::cout << "Component map for game object '" << name << "' doesn't contain component '" << typeid(T).name() << "'!" << std::endl;
                return std::nullopt;
            }

            return std::make_optional(std::static_pointer_cast<T>(componentMap[typeid(T)]));
        }

        std::optional<std::shared_ptr<Component>> GetComponentDynamic(const std::string& typeName)
        {
            std::optional<std::type_index> typeIndex = std::nullopt;

            for (auto& [type, component] : componentMap)
            {
                if (component->GetTypeName() == typeName)
                    typeIndex = type;
            }

            if (!typeIndex.has_value())
            {
                std::cout << "Component map for game object '" << name << "' doesn't contain component '" << typeName << "'!" << std::endl;
                return std::nullopt;
            }

            return std::make_optional(componentMap[typeIndex.value()]);
        }

        std::shared_ptr<Transform> GetTransform()
        {
            return std::static_pointer_cast<Transform>(componentMap[typeid(Transform)]);
        }

        template <typename T> requires (std::is_base_of_v<Component, T>)
        void RemoveComponent()
        {
            if (!componentMap.contains(typeid(T)))
            {
                std::cout << "Component map for game object '" << name << "' doesn't contain component '" << typeid(T).name() << "'!" << std::endl;
                return;
            }

            componentMap[typeid(T)]->wasRemoved = true;

#ifdef IS_SERVER
            Blaster::Server::Network::ServerSynchronization::MarkDirty(shared_from_this());
#endif

            componentMap.erase(typeid(T));
        }

        void RemoveComponentDynamic(const std::string& typeName)
        {
            for (auto iterator = componentMap.begin(); iterator != componentMap.end(); ++iterator)
            {
                if (iterator->second->GetTypeName() == typeName)
                {
                    iterator->second->wasRemoved = true;

                    componentMap.erase(iterator);
                    return;
                }
            }

#ifdef IS_SERVER
            Blaster::Server::Network::ServerSynchronization::MarkDirty(shared_from_this());
#endif
        }

        void SetParent(std::shared_ptr<GameObject> parent)
        {
            if (parent != nullptr)
            {
                owningClient = parent->GetOwningClient();

                GetTransform()->SetParent(parent->GetTransform());

                this->parent = std::make_optional(parent);
            }
            else
            {
                owningClient = std::nullopt;

                GetTransform()->SetParent(nullptr);

                this->parent = std::nullopt;
            }
        }

        const std::unordered_map<std::type_index, std::shared_ptr<Component>>& GetComponentMap() const override
        {
            return componentMap;
        }

        const std::unordered_map<std::string, std::shared_ptr<GameObject>>& GetChildMap() const override
        {
            return childMap;
        }

        std::optional<std::weak_ptr<GameObject>> GetParent()
        {
            return parent;
        }

        [[nodiscard]]
        std::string GetAbsolutePath() const override
        {
            std::vector<std::string_view> segments;

            auto current = this;

            while (current != nullptr)
            {
                segments.emplace_back(current->name);

                if (current->parent && !current->parent->expired())
                    current = current->parent->lock().get();
                else
                    current = nullptr;
            }

            std::string path;
            path.reserve(segments.size() * 8);

            for (auto it = segments.rbegin(); it != segments.rend(); ++it)
            {
                if (!path.empty())
                    path.push_back('.');

                path.append(it->data(), it->size());
            }

            return path;
        }

        std::string GetName() const
        {
            return name;
        }

        std::optional<NetworkId> GetOwningClient() const
        {
            return owningClient;
        }

        void SetOwningClient(const std::optional<NetworkId> owningClient)
        {
            this->owningClient = owningClient;
        }

        bool IsAuthoritative() const noexcept
        {
            return authoritative;
        }

        bool IsLocallyControlled() const noexcept
        {
            return !IsAuthoritative() && GetOwningClient().has_value() && GetOwningClient().value() == ClientNetwork::GetInstance().GetNetworkId();
        }

        [[nodiscard]]
        bool WasJustCreated() const noexcept override
        {
            return justCreated;
        }

        [[nodiscard]]
        bool IsDestroyed() const noexcept override
        {
            return destroyed;
        }

        [[nodiscard]]
        std::string GetTypeName() const override
        {
            return typeid(*this).name();
        }

        void MarkDestroyed() noexcept override
        {
            destroyed = true;
        }

        void Update()
        {
            if (!IsAuthoritative() && owningClient.has_value() && owningClient.value() != ClientNetwork::GetInstance().GetNetworkId())
                return;

            for (const auto& component : componentMap | std::views::values)
                component->Update();

            for (const auto& child : childMap | std::views::values)
                child->Update();

            justCreated = false;

            for (const auto& component : componentMap | std::views::values)
                component->ClearTransientFlags();
        }

        void Render(const std::shared_ptr<Client::Render::Camera>& camera)
        {
            for (const auto& component : componentMap | std::views::values)
                component->Render(camera);

            for (const auto& child : childMap | std::views::values)
                child->Render(camera);
        }

        static std::shared_ptr<GameObject> Create(const std::string& name, const std::optional<NetworkId>& owningClient = std::nullopt)
        {
            std::shared_ptr<GameObject> result(new GameObject());

            result->name = name;
            result->owningClient = owningClient;
            result->AddComponent(Transform::Create({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }));

            return result;
        }

    private:

        GameObject() = default;

        friend class Blaster::Server::Network::ServerSynchronization;
        friend class Blaster::Independent::ECS::GameObjectManager;

        std::shared_ptr<GameObject> AddChild(std::shared_ptr<GameObject> child)
        {
            if (childMap.contains(child->GetName()))
            {
                std::cout << "Child map for game object '" << name << "' already contains child '" << child->GetName() << "'!" << std::endl;
                return nullptr;
            }

            child->SetParent(shared_from_this());

            std::string childName = child->GetName();

            childMap.insert({ childName, std::move(child) });

#ifdef IS_SERVER
            Blaster::Server::Network::ServerSynchronization::MarkDirty(shared_from_this());
#endif

            return childMap[childName];
        }

        bool HasChild(const std::string& childName) const
        {
            return childMap.contains(childName);
        }

        std::optional<std::shared_ptr<GameObject>> GetChild(const std::string& childName)
        {
            if (!childMap.contains(childName))
            {
                std::cout << "Child map for game object '" << name << "' doesn't contain child '" << childName << "'!" << std::endl;
                return std::nullopt;
            }

            return std::make_optional(childMap[childName]);
        }

        void RemoveChild(const std::string& childName)
        {
            if (!childMap.contains(childName))
            {
                std::cout << "Child map for game object '" << name << "' doesn't contain child '" << childName << "'!" << std::endl;
                return;
            }

#ifdef IS_SERVER
            Blaster::Server::Network::ServerSynchronization::MarkDirty(shared_from_this());
#endif

            childMap.erase(childName);
        }

        std::string name;

        bool authoritative = false;

        bool justCreated = true;
        bool destroyed = false;

        std::optional<NetworkId> owningClient = std::nullopt;

        std::optional<std::weak_ptr<GameObject>> parent;

        std::unordered_map<std::type_index, std::shared_ptr<Component>> componentMap = {};
        std::unordered_map<std::string, std::shared_ptr<GameObject>> childMap = {};

    };
}
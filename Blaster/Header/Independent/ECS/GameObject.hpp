#pragma once

#include <iostream>
#include <typeindex>
#include <memory>
#include <optional>
#include <ranges>
#include <map>
#include "Client/Network/ClientNetwork.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/IGameObjectSynchronization.hpp"
#include "Independent/Math/Transform3d.hpp"
#include "Independent/Math/Transform2d.hpp"
#include "Independent/Network/CommonNetwork.hpp"

#ifdef IS_SERVER
#include "Server/Network/ServerNetwork.hpp"
#endif

using namespace Blaster::Client::Network;
using namespace Blaster::Independent::Math;
using namespace Blaster::Independent::Network;

namespace Blaster::Independent::ECS
{
    class GameObjectManager;

    class GameObject final : public std::enable_shared_from_this<GameObject>, public IGameObjectSynchronization
    {

    public:

        ~GameObject()
        {
#ifdef IS_SERVER
            if (owningClient.has_value() && owningClient.value() != 0 && Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(owningClient.value()).has_value())
                Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(owningClient.value()).value()->ownedGameObjectList.erase(GetAbsolutePath());
#endif
        }

        GameObject(const GameObject&) = delete;
        GameObject(GameObject&&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject& operator=(GameObject&&) = delete;

        template <typename T> requires (std::is_base_of_v<Component, T>)
        std::shared_ptr<T> AddComponent(std::shared_ptr<T> component, bool markDirty = true)
        {
            std::unique_lock lock(mutex);

            if (componentMap.contains(typeid(T)))
            {
                std::cout << "Component map for game object '" << name << "' already contains component '" << typeid(T).name() << "'!" << std::endl;
                return nullptr;
            }

            component->gameObject = shared_from_this();
            component->Initialize();

            componentMap.insert({ typeid(T), std::move(component) });
            componentOrder.push_back(componentMap[typeid(T)]);

            componentMap[typeid(T)]->wasAdded = true;

            if (markDirty)
                Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(shared_from_this(), typeid(T));

            return std::static_pointer_cast<T>(componentMap[typeid(T)]);
        }

        std::shared_ptr<Component> AddComponentDynamic(std::shared_ptr<Component> component, bool markDirty = true)
        {
            const auto type = std::type_index(typeid(*component));

            std::unique_lock lock(mutex);

            if (componentMap.contains(type))
            {
                std::cout << "Component map for game object '" << name << "' already contains component '" << type.name() << "'!" << std::endl;
                return nullptr;
            }

            component->gameObject = shared_from_this();
            component->Initialize();

            componentMap.insert({ type, std::move(component) });
            componentOrder.push_back(componentMap[type]);

            componentMap[type]->wasAdded = markDirty;

            if (markDirty)
                Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(shared_from_this(), type);

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
            std::shared_lock lock(mutex);

            if (const auto exactHit = componentMap.find(typeid(T)); exactHit != componentMap.end())
                return std::make_optional(std::static_pointer_cast<T>(exactHit->second));


            for (const auto& [storedType, storedComponent] : componentMap)
            {
                if (auto casted = std::dynamic_pointer_cast<T>(storedComponent); casted != nullptr)
                    return std::make_optional(std::move(casted));
            }

            std::cout << "Component map for game object '" << name << "' does not contain a component derived from '" << typeid(T).name() << "'!" << std::endl;

            return std::nullopt;
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

        std::shared_ptr<Component>* UnsafeFindComponentPointer(const std::string& typeName)
        {
            for (auto& [type, comp] : componentMap)
                if (comp->GetTypeName() == typeName)
                    return &comp;

            return nullptr;
        }

        std::shared_ptr<Transform3d> GetTransform3d()
        {
            return std::static_pointer_cast<Transform3d>(componentMap[typeid(Transform3d)]);
        }

        std::shared_ptr<Transform2d> GetTransform2d()
        {
            return std::static_pointer_cast<Transform2d>(componentMap[typeid(Transform2d)]);
        }

        template <typename T> requires (std::is_base_of_v<Component, T>)
        void RemoveComponent()
        {
            std::unique_lock lock(mutex);

            if (!componentMap.contains(typeid(T)))
            {
                std::cout << "Component map for game object '" << name << "' doesn't contain component '" << typeid(T).name() << "'!" << std::endl;
                return;
            }

            componentMap[typeid(T)]->wasRemoved = true;

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(shared_from_this(), typeid(T));

            componentMap.erase(typeid(T));
        }

        void RemoveComponentDynamic(const std::string& typeName, bool markDirty = true)
        {
            std::unique_lock lock(mutex);

            for (auto iterator = componentMap.begin(); iterator != componentMap.end(); ++iterator)
            {
                if (iterator->second->GetTypeName() == typeName)
                {
                    if (markDirty)
                        Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(shared_from_this(), iterator->first);

                    iterator->second->wasRemoved = true;

                    componentMap.erase(iterator);
                    return;
                }
            }
        }

        void SetParent(std::shared_ptr<GameObject> parent)
        {
            if (parent != nullptr)
            {
                owningClient = parent->GetOwningClient();

                const bool childIs3D = HasComponent<Transform3d>();
                const bool parentIs3D = parent->HasComponent<Transform3d>();

                if (childIs3D != parentIs3D)
                    throw std::logic_error("Cannot parent 2D object to 3D object (or vice-versa)");

                if (childIs3D)
                    GetTransform3d()->SetParent(parent->GetTransform3d());
                else
                    GetTransform2d()->SetParent(parent->GetTransform2d());

                this->parent = std::make_optional(parent);
            }
            else
            {
                owningClient = std::nullopt;

                if (HasComponent<Transform3d>())
                    GetTransform3d()->SetParent(nullptr);
                else
                    GetTransform2d()->SetParent(nullptr);

                this->parent = std::nullopt;
            }
        }

        const std::unordered_map<std::type_index, std::shared_ptr<Component>>& GetComponentMap() const override
        {
            return componentMap;
        }

        const std::map<std::string, std::shared_ptr<GameObject>>& GetChildMap() const override
        {
            return childMap;
        }

        const std::vector<std::shared_ptr<Component>>& GetComponentOrder() const override
        {
            return componentOrder;
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

        std::optional<NetworkId> GetOwningClient() const override
        {
            return owningClient;
        }

        void SetOwningClient(const std::optional<NetworkId> owningClient)
        {
            this->owningClient = owningClient;
        }

        bool IsAuthoritative() const noexcept
        {
            return isAuthoritative;
        }

        bool IsLocallyControlled() const noexcept override
        {
#ifdef IS_SERVER
            return !owningClient.has_value();
#else
            return owningClient.has_value() && owningClient.value() == ClientNetwork::GetInstance().GetNetworkId();
#endif
        }

        [[nodiscard]]
        bool WasJustCreated() const noexcept override
        {
            return justCreated;
        }

        void ClearJustCreated() override
        {
            justCreated = false;
        }

        [[nodiscard]]
        bool IsDestroyed() const noexcept override
        {
            return destroyed;
        }

        void SetLocal(bool isLocal)
        {
            this->isLocal = isLocal;
        }

        [[nodiscard]]
        bool IsLocal() const noexcept override
        {
            return isLocal;
        }

        [[nodiscard]]
        std::string GetTypeName() const override
        {
            return "class Blaster::Independent::ECS::GameObject";
        }

        [[nodiscard]]
        std::shared_mutex& GetMutex() noexcept override
        {
            return mutex;
        }

        void SetLocallyActive(bool isLocallyActive)
        {
            this->isLocallyActive = isLocallyActive;
        }

        bool IsLocallyActive() const
        {
            return isLocallyActive;
        }

        void MarkDestroyed() noexcept override
        {
            destroyed = true;
        }

        void Update()
        {
            std::shared_lock lock(mutex);

            for (const auto& component : componentMap | std::views::values)
                component->Update();

            for (const auto& child : childMap | std::views::values)
                child->Update();
        }

        void Render(const std::shared_ptr<Client::Render::Camera>& camera)
        {
            if (!isLocallyActive)
                return;

            std::shared_lock lock(mutex);

            for (const auto& component : componentMap | std::views::values)
                component->Render(camera);

            for (const auto& child : childMap | std::views::values)
                child->Render(camera);
        } 

        static std::shared_ptr<GameObject> Create(const std::string& name, bool isLocal = false, const std::optional<NetworkId>& owningClient = std::nullopt, bool isUI = false)
        {
            std::shared_ptr<GameObject> result(new GameObject());

            result->name = name;
            result->isLocal = isLocal;
            result->owningClient = owningClient;
#ifdef IS_SERVER
            result->isAuthoritative = true;

            if (owningClient.has_value() && owningClient.value() != 0 && Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(owningClient.value()).has_value())
                Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(owningClient.value()).value()->ownedGameObjectList.insert({ result->GetAbsolutePath(), std::static_pointer_cast<IGameObjectSynchronization>(result) });
#endif

            if (isUI)
                result->AddComponent(Transform2d::Create({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0.0f));
            else
                result->AddComponent(Transform3d::Create({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }));

            return result;
        }

    private:

        GameObject() = default;

        friend class Blaster::Independent::ECS::Synchronization::SenderSynchronization;
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

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(childMap[childName]);

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

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(shared_from_this());

            childMap.erase(childName);
        }

        std::string name;

        mutable std::shared_mutex mutex;

        bool isLocal = false;
        bool isAuthoritative = false;

        bool justCreated = true;
        bool destroyed = false;

        bool isLocallyActive = true;

        std::optional<NetworkId> owningClient = std::nullopt;

        std::optional<std::weak_ptr<GameObject>> parent;

        std::unordered_map<std::type_index, std::shared_ptr<Component>> componentMap;

        std::vector<std::shared_ptr<Component>> componentOrder;

        std::map<std::string, std::shared_ptr<GameObject>> childMap = {};

    };
}
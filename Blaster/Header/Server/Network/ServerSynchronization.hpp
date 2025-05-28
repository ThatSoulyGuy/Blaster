#pragma once

#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/NetworkSerialize.hpp"
#include "Server/Network/ServerNetwork.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Server::Network
{
    class ServerSynchronization final
    {

    public:

        ServerSynchronization(const ServerSynchronization&) = delete;
        ServerSynchronization(ServerSynchronization&&) = delete;
        ServerSynchronization& operator=(const ServerSynchronization&) = delete;
        ServerSynchronization& operator=(ServerSynchronization&&) = delete;

        static std::shared_ptr<GameObject> SpawnGameObject(std::string name)
        {
            auto gameObject = GameObject::Create(std::move(name));

            gameObject->networkId = ++nextId;
            gameObject->authoritative = true;

            GameObjectManager::GetInstance().Register(gameObject);

            BroadcastGameObject(gameObject);

            return gameObject;
        }

        static void DestroyGameObject(const std::shared_ptr<GameObject>& go)
        {
            GameObjectManager::GetInstance().Unregister(go->GetName());

            std::vector<std::uint8_t> buffer(go->GetName().begin(), go->GetName().end());

            BroadcastPayload(PacketType::S2C_DestroyGameObject, buffer);
        }

        static void AddComponent(const std::shared_ptr<GameObject>& gameObject, const std::shared_ptr<Component>& component)
        {
            if (!gameObject->AddComponentDynamic(component))
                return;

            auto componentBuffer = NetworkSerialize::ObjectToBytes(component);

            const std::string& name = gameObject->GetName();

            std::vector<std::uint8_t> buffer(name.begin(), name.end());
            buffer.push_back('\0');

            auto type = component->GetTypeName();

            buffer.insert(buffer.end(), type.begin(), type.end());
            buffer.push_back('\0');
            buffer.insert(buffer.end(), componentBuffer.begin(), componentBuffer.end());

            BroadcastPayload(PacketType::S2C_AddComponent, buffer);
        }

        static void RemoveComponent(const std::shared_ptr<GameObject>& gameObject, const std::string_view typeName)
        {
            gameObject->RemoveComponentDynamic(std::string(typeName));

            const std::string& name = gameObject->GetName();

            std::vector<std::uint8_t> buffer(name.begin(), name.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), typeName.begin(), typeName.end());

            BroadcastPayload(PacketType::S2C_RemoveComponent, buffer);
        }

        static void AddChild(const std::shared_ptr<GameObject>& parent, const std::shared_ptr<GameObject>& child)
        {
            if (!parent->AddChild(child))
                return;

            const std::string& parentName = parent->GetName();

            std::vector<std::uint8_t> buffer(parentName.begin(), parentName.end());

            buffer.push_back('\0');

            const std::string& childName = child->GetName();

            buffer.insert(buffer.end(), childName.begin(), childName.end());

            BroadcastPayload(PacketType::S2C_AddChild, buffer);
        }

        static void RemoveChild(const std::shared_ptr<GameObject>& parent, const std::string_view childName)
        {
            parent->RemoveChild(std::string(childName));

            const std::string& parentName = parent->GetName();

            std::vector<std::uint8_t> buffer(parentName.begin(), parentName.end());

            buffer.push_back('\0');

            buffer.insert(buffer.end(), childName.begin(), childName.end());

            BroadcastPayload(PacketType::S2C_RemoveChild, buffer);
        }

        static void SendGameObject(const std::optional<NetworkId> recipient, const std::shared_ptr<GameObject>& gameObject)
        {
            auto blob = NetworkSerialize::ObjectToBytes(gameObject->GetTransform());

            const std::string& name = gameObject->GetName();

            std::vector<std::uint8_t> payload(name.begin(), name.end());

            payload.push_back('\0');
            payload.insert(payload.end(), blob.begin(), blob.end());

            if (recipient.has_value())
                ServerNetwork::GetInstance().SendTo(recipient.value(), PacketType::S2C_CreateGameObject, payload);
            else
                ServerNetwork::GetInstance().Broadcast(PacketType::S2C_CreateGameObject, payload);

            for (auto&& [typeName, component] : gameObject->GetComponentMap())
            {
                if (typeName == typeid(Transform))
                    continue;

                auto componentBytes = NetworkSerialize::ObjectToBytes(component);

                std::vector<std::uint8_t> buffer(name.begin(), name.end());

                buffer.push_back('\0');

                const std::string componentTypeName = component->GetTypeName();

                buffer.insert(buffer.end(), componentTypeName.begin(), componentTypeName.end());
                buffer.push_back('\0');
                buffer.insert(buffer.end(), componentBytes.begin(), componentBytes.end());

                if (recipient.has_value())
                    ServerNetwork::GetInstance().SendTo(recipient.value(), PacketType::S2C_AddComponent, buffer);
                else
                    ServerNetwork::GetInstance().Broadcast(PacketType::S2C_AddComponent, buffer);
            }
        }

        static void SynchronizeFullTree(const NetworkId recipient)
        {
            for (auto& gameObject : GameObjectManager::GetInstance().GetAll())
                SendGameObject(recipient, gameObject);
        }

    private:

        ServerSynchronization() = default;

        static void BroadcastGameObject(const std::shared_ptr<GameObject>& gameObject)
        {
            SendGameObject(std::nullopt, gameObject);
        }

        static void BroadcastPayload(const PacketType type, const std::span<const std::uint8_t> payload)
        {
            ServerNetwork::GetInstance().Broadcast(type, payload);
        }

        static inline std::atomic<NetworkId> nextId = 0;

    };
}
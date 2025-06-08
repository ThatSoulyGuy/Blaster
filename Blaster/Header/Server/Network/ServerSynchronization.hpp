#pragma once

#include <ranges>

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

        static std::shared_ptr<GameObject> SpawnGameObject(const std::string& name, const std::optional<NetworkId> owningClient = std::nullopt, const std::optional<NetworkId> except = std::nullopt)
        {
            const auto gameObject = GameObject::Create(name, owningClient);

            gameObject->authoritative = true;

            auto movedGameObject = GameObjectManager::GetInstance().Register(gameObject);

            if (owningClient.has_value() && ServerNetwork::GetInstance().HasClient(owningClient.value()))
                ServerNetwork::GetInstance().GetClient(owningClient.value()).value()->ownedGameObjectList.push_back(movedGameObject);

            BroadcastGameObject(movedGameObject, except);

            return movedGameObject;
        }

        static void DestroyGameObject(const std::shared_ptr<GameObject>& go)
        {
            GameObjectManager::GetInstance().Unregister(go->GetName());

            std::vector<std::uint8_t> buffer(go->GetName().begin(), go->GetName().end());

            BroadcastPayload(PacketType::S2C_DestroyGameObject, buffer);
        }

        static std::shared_ptr<Component> AddComponent(const std::shared_ptr<GameObject>& gameObject, const std::shared_ptr<Component>& component, const std::optional<NetworkId> except = std::nullopt)
        {
            auto result = gameObject->AddComponentDynamic(component);

            if (!result)
                return nullptr;

            auto componentBuffer = NetworkSerialize::ObjectToBytes(component);

            const std::string& name = gameObject->GetName();

            std::vector<std::uint8_t> buffer(name.begin(), name.end());
            buffer.push_back('\0');

            auto type = component->GetTypeName();

            buffer.insert(buffer.end(), type.begin(), type.end());
            buffer.push_back('\0');
            buffer.insert(buffer.end(), componentBuffer.begin(), componentBuffer.end());

            BroadcastPayload(PacketType::S2C_AddComponent, buffer, except);

            return result;
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

        static std::shared_ptr<GameObject> AddChild(const std::shared_ptr<GameObject>& parent, const std::shared_ptr<GameObject>& child, const std::optional<NetworkId> except = std::nullopt)
        {
            auto result = parent->AddChild(child);

            if (!result)
                return nullptr;

            const std::string& parentName = parent->GetName();

            std::vector<std::uint8_t> buffer(parentName.begin(), parentName.end());

            buffer.push_back('\0');

            const std::string& childName = child->GetName();

            buffer.insert(buffer.end(), childName.begin(), childName.end());

            BroadcastPayload(PacketType::S2C_AddChild, buffer, except);

            return result;
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

        static void SendGameObject(const std::optional<NetworkId> recipient, const std::shared_ptr<GameObject>& node, const std::optional<NetworkId>& except, std::string_view parentPath = {})
        {
            std::string absPath;

            if (parentPath.empty())
                absPath = BuildPath(node);
            else
            {
                absPath.reserve(parentPath.size() + 1 + node->GetName().size());
                absPath.append(parentPath).push_back('.');
                absPath.append(node->GetName());
            }

            const auto transformBlob = NetworkSerialize::ObjectToBytes(node->GetTransform());

            const std::uint32_t owner = node->GetOwningClient().value_or(4096);

            std::array<std::uint8_t,4> ownerBytes{};
            std::memcpy(ownerBytes.data(), &owner, sizeof owner);

            std::vector<std::uint8_t> payload;
            payload.reserve(absPath.size() + 1 + ownerBytes.size() + transformBlob.size());

            payload.insert(payload.end(), absPath.begin(), absPath.end());
            payload.push_back('\0');
            payload.insert(payload.end(), ownerBytes.begin(), ownerBytes.end());
            payload.insert(payload.end(), transformBlob.begin(), transformBlob.end());

            const auto sendCreate = [&](const NetworkId id)
            {
                ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_CreateGameObject, payload);
            };

            if (recipient && !except)
                sendCreate(*recipient);
            else if (except)
            {
                for (const auto id : ServerNetwork::GetInstance().GetConnectedClients())
                {
                    if (id != *except)
                        sendCreate(id);
                }
            }
            else
                ServerNetwork::GetInstance().Broadcast(PacketType::S2C_CreateGameObject, payload);

            for (auto&& [idx, component] : node->GetComponentMap())
            {
                if (idx == typeid(Transform))
                    continue;

                const auto blob = NetworkSerialize::ObjectToBytes(component);

                std::vector<std::uint8_t> buffer;

                buffer.reserve(absPath.size() + 1 + component->GetTypeName().size() + 1 + blob.size());

                buffer.insert(buffer.end(), absPath.begin(), absPath.end());
                buffer.push_back('\0');

                const auto& typeName = component->GetTypeName();

                buffer.insert(buffer.end(), typeName.begin(), typeName.end());
                buffer.push_back('\0');
                buffer.insert(buffer.end(), blob.begin(), blob.end());

                const auto sendAdd = [&](NetworkId id)
                {
                    ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_AddComponent, buffer);
                };

                if (recipient && !except)
                    sendAdd(*recipient);
                else if (except)
                {
                    for (const auto id : ServerNetwork::GetInstance().GetConnectedClients())
                    {
                        if (id != *except)
                            sendAdd(id);
                    }
                }
                else
                    ServerNetwork::GetInstance().Broadcast(PacketType::S2C_AddComponent, buffer);
            }

            for (const auto& child: node->GetChildMap() | std::views::values)
                SendGameObject(recipient, child, except, absPath);
        }

        static void SynchronizeFullTree(const NetworkId recipient)
        {
            for (auto& root : GameObjectManager::GetInstance().GetAll())
                SendSubtree(recipient, root, std::nullopt);
        }

    private:

        ServerSynchronization() = default;

        static std::string BuildPath(const std::shared_ptr<GameObject>& node)
        {
            std::vector<std::string_view> segments;

            for (auto current = node; current;)
            {
                segments.emplace_back(current->GetName());

                if (const auto parent = current->GetParent(); parent && !parent->expired())
                    current = parent->lock();
                else
                    current.reset();
            }

            std::string full;

            for (auto& segment : std::ranges::reverse_view(segments))
            {
                if (!full.empty())
                    full.push_back('.');

                full.append(segment.data(), segment.size());
            }

            return full;
        }

        static void SendSubtree(const std::optional<NetworkId> recipient, const std::shared_ptr<GameObject>& node, const std::optional<NetworkId>& except)
        {
            SendGameObject(recipient, node, except);

            for (const auto &child: node->GetChildMap() | std::views::values)
                SendSubtree(recipient, child, except);
        }

        static void BroadcastGameObject(const std::shared_ptr<GameObject>& gameObject)
        {
            BroadcastGameObject(gameObject, std::nullopt);
        }

        static void BroadcastGameObject(const std::shared_ptr<GameObject>& gameObject, const std::optional<NetworkId>& except)
        {
            SendGameObject(std::nullopt, gameObject, except);
        }

        static void BroadcastPayload(const PacketType type, const std::span<const std::uint8_t> payload, const std::optional<NetworkId> except = std::nullopt)
        {
            ServerNetwork::GetInstance().Broadcast(type, payload, except);
        }

    };
}
#pragma once

#include "Client/Network/ClientNetwork.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Network/CommonRpc.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Client::Network
{
    class ClientRpc final
    {

    public:

        using PromiseBase = std::shared_ptr<void>;

        template <class T>
        using Promise = std::shared_ptr<std::promise<T>>;

        template <class T>
        using Future = std::future<T>;

        ClientRpc(const ClientRpc&) = delete;
        ClientRpc(ClientRpc&&) = delete;
        ClientRpc& operator=(const ClientRpc&) = delete;
        ClientRpc& operator=(ClientRpc&&) = delete;

        static void OnGameObjectReplicated(const std::string& name)
        {
            auto& entry = pendingGameObjectMap[name];

            entry.created = true;

            if (entry.promise)
            {
                auto gameObject = GameObjectManager::GetInstance().Get(name);

                entry.promise->set_value(gameObject ? *gameObject : nullptr);

                pendingGameObjectMap.erase(name);
            }
        }

        static Future<std::shared_ptr<GameObject>> CreateGameObject(std::string name)
        {
            const std::vector<std::uint8_t> buffer(name.begin(), name.end());

            return MakeCall<std::shared_ptr<GameObject>>(RpcType::C2S_CreateGameObject, buffer);
        }

        static Future<void> DestroyGameObject(std::string name)
        {
            std::vector<std::uint8_t> buffer(name.begin(), name.end());

            return MakeCall<void>(RpcType::C2S_DestroyGameObject, buffer);
        }

        static Future<std::shared_ptr<Component>> AddComponent(std::string gameObjectName, const std::shared_ptr<Component>& component)
        {
            auto componentBytes = NetworkSerialize::ObjectToBytes(component);

            std::vector<std::uint8_t> buffer(gameObjectName.begin(), gameObjectName.end());

            buffer.push_back('\0');

            const auto type = component->GetTypeName();

            buffer.insert(buffer.end(), type.begin(), type.end());
            buffer.push_back('\0');
            buffer.insert(buffer.end(), componentBytes.begin(), componentBytes.end());

            return MakeCall<std::shared_ptr<Component>>(RpcType::C2S_AddComponent, buffer);
        }

        static Future<void> RemoveComponent(std::string gameObjectName, std::string type)
        {
            std::vector<std::uint8_t> buffer(gameObjectName.begin(), gameObjectName.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), type.begin(), type.end());

            return MakeCall<void>(RpcType::C2S_RemoveComponent, buffer);
        }

        static Future<void> AddChild(std::string parent, std::string child)
        {
            std::vector<std::uint8_t> buffer(parent.begin(), parent.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), child.begin(), child.end());

            return MakeCall<void>(RpcType::C2S_AddChild, buffer);
        }

        static Future<void> RemoveChild(std::string parent, std::string child)
        {
            std::vector<std::uint8_t> buffer(parent.begin(), parent.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), child.begin(), child.end());

            return MakeCall<void>(RpcType::C2S_RemoveChild, buffer);
        }

        static Future<void> TranslateTo(std::string gameObject, const Vector<float, 3> target, const float seconds)
        {
            std::vector<std::uint8_t> buffer(gameObject.begin(), gameObject.end());
            buffer.push_back('\0');

            auto pushBlob = [&](const auto& obj)
            {
                auto blob = NetworkSerialize::ObjectToBytes(obj);
                const std::uint32_t length = static_cast<std::uint32_t>(blob.size());

                buffer.insert(buffer.end(), reinterpret_cast<const std::uint8_t*>(&length), reinterpret_cast<const std::uint8_t*>(&length) + sizeof length);
                buffer.insert(buffer.end(), blob.begin(), blob.end());
            };

            pushBlob(target);
            pushBlob(seconds);

            return MakeCall<void>(RpcType::C2S_TranslateTo, buffer);
        }

        static void HandleReply(std::vector<std::uint8_t> packetData)
        {
            RpcHeader header;

            std::memcpy(&header, packetData.data(), sizeof header);

            std::cout << "[RPC-RX] id=" << header.id
                << "  type=" << static_cast<int>(header.type) << '\n';

            packetData.erase(packetData.begin(), packetData.begin() + sizeof header);

            const auto iterator = pendingMap.find(header.id);

            if (iterator == pendingMap.end())
                return;

            switch (header.type)
            {
                case RpcType::S2C_CreateGameObject:
                {
                    const std::string name(packetData.begin(), packetData.end());

                    auto& [created, replied, promise] = pendingGameObjectMap[name];

                    replied = true;
                    promise = std::static_pointer_cast<std::promise<std::shared_ptr<GameObject>>>(iterator->second);

                    if (created)
                    {
                        auto gameObject = GameObjectManager::GetInstance().Get(name);

                        promise->set_value(gameObject ? *gameObject : nullptr);

                        pendingGameObjectMap.erase(name);
                    }

                    break;
                }

                case RpcType::S2C_AddComponent:
                {
                    const auto promise = std::static_pointer_cast<std::promise<std::shared_ptr<Component>>>(iterator->second);

                    const auto lineEndIterator = std::ranges::find(packetData, '\0');

                    const std::string gameObject(packetData.begin(), lineEndIterator);
                    const std::string type(lineEndIterator+1, packetData.end());

                    const auto optionalGameObject = GameObjectManager::GetInstance().Get(gameObject);

                    promise->set_value(optionalGameObject ? (*optionalGameObject)->GetComponentDynamic(type).value() : nullptr);

                    break;
                }

                default:
                    std::static_pointer_cast<std::promise<void>>(iterator->second)->set_value();
            }

            pendingMap.erase(iterator);
        }

    private:

        ClientRpc() = default;

        template <class T>
        static Future<T> MakeCall(const RpcType type, std::vector<std::uint8_t> payload)
        {
            const std::uint64_t id = nextId++;

            std::cout << "[RPC-TX] id=" << id
                      << "  type=" << static_cast<int>(type)
                      << "  bytes=" << payload.size() << '\n';

            const RpcHeader header{ id, type };

            std::vector<std::uint8_t> packet(sizeof header);
            std::memcpy(packet.data(), &header, sizeof header);

            packet.insert(packet.end(), payload.begin(), payload.end());

            auto promise = std::make_shared<std::promise<T>>();

            pendingMap[id] = promise;

            std::cout << "Sent packet with RPC type " << static_cast<int>(type) << std::endl;

            ClientNetwork::GetInstance().Send(PacketType::C2S_Rpc, packet);

            return promise->get_future();
        }

        struct PendingGameObject
        {
            bool created = false;
            bool replied = false;

            std::shared_ptr<std::promise<std::shared_ptr<GameObject>>> promise;
        };

        static std::unordered_map<std::string, PendingGameObject> pendingGameObjectMap;

        static std::unordered_map<std::uint64_t, PromiseBase> pendingMap;
        static std::atomic_uint64_t nextId;

    };

    std::unordered_map<std::string, ClientRpc::PendingGameObject> ClientRpc::pendingGameObjectMap;
    std::unordered_map<std::uint64_t, ClientRpc::PromiseBase> ClientRpc::pendingMap = {};
    std::atomic_uint64_t ClientRpc::nextId = 1;
}
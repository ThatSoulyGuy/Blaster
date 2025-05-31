#pragma once

#include "Client/Network/ClientNetwork.hpp"
#include "Independent/ECS/GameObject.hpp"
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

            auto targetBytes = NetworkSerialize::ObjectToBytes(target);

            buffer.insert(buffer.end(), targetBytes.begin(), targetBytes.end());

            auto secondsBytes = NetworkSerialize::ObjectToBytes(seconds);

            buffer.insert(buffer.end(), secondsBytes.begin(), secondsBytes.end());

            return MakeCall<void>(RpcType::C2S_TranslateTo, buffer);
        }

        static void HandleReply(std::vector<std::uint8_t> packetData)
        {
            RpcHeader header;

            std::memcpy(&header, packetData.data(), sizeof header);

            packetData.erase(packetData.begin(), packetData.begin() + sizeof header);

            const auto iterator = pending.find(header.id);

            if (iterator == pending.end())
                return;

            switch (header.type)
            {
                case RpcType::S2C_CreateGameObject:
                {
                    const auto promise = std::static_pointer_cast<std::promise<std::shared_ptr<GameObject>>>(iterator->second);

                    const std::string name(packetData.begin(), packetData.end());

                    auto gameObject = GameObjectManager::GetInstance().Get(name);

                    promise->set_value(gameObject ? *gameObject : nullptr);

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

            pending.erase(iterator);
        }

    private:

        ClientRpc() = default;

        template <class T>
        static Future<T> MakeCall(const RpcType type, std::vector<std::uint8_t> payload)
        {
            const std::uint64_t id = nextId++;

            const RpcHeader header{ id, type };

            std::vector<std::uint8_t> packet(sizeof header);
            std::memcpy(packet.data(), &header, sizeof header);

            packet.insert(packet.end(), payload.begin(), payload.end());

            auto promise = std::make_shared<std::promise<T>>();

            pending[id] = promise;

            ClientNetwork::GetInstance().Send(PacketType::C2S_Rpc, packet);

            return promise->get_future();
        }

        static std::unordered_map<std::uint64_t, PromiseBase> pending;
        static std::atomic_uint64_t nextId;

    };

    std::unordered_map<std::uint64_t, ClientRpc::PromiseBase> ClientRpc::pending = {};
    std::atomic_uint64_t ClientRpc::nextId = 1;
}
#pragma once

#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Network/CommonRpc.hpp"
#include "Server/Network/ServerNetwork.hpp"
#include "Server/Network/ServerSynchronization.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Server::Network
{
    class ServerRpc final
    {

    public:

        ServerRpc(const ServerRpc&) = delete;
        ServerRpc(ServerRpc&&) = delete;
        ServerRpc& operator=(const ServerRpc&) = delete;
        ServerRpc& operator=(ServerRpc&&) = delete;

        static void HandleRequest(NetworkId who, std::vector<std::uint8_t> packet)
        {
            RpcHeader header;

            std::memcpy(&header, packet.data(), sizeof header);
            packet.erase(packet.begin(), packet.begin() + sizeof header);

            std::cout << "[RPC-RX] id=" << header.id
                << "  type=" << static_cast<int>(header.type)
                << "  from=" << who << '\n';

            std::cout << "Packet received!" << std::endl;

            switch (header.type)
            {
                case RpcType::C2S_CreateGameObject:
                {
                    std::string name(packet.begin(), packet.end());

                    auto gameObject = ServerSynchronization::SpawnGameObject(name);

                    SendReply(who, header.id, RpcType::S2C_CreateGameObject, std::span(reinterpret_cast<const std::uint8_t*>(name.data()), name.size()));

                    std::cout << "C2S_CreateGameObject" << std::endl;

                    break;
                }

                case RpcType::C2S_DestroyGameObject:
                {
                    std::string name(packet.begin(), packet.end());

                    if (auto gameObject = GameObjectManager::GetInstance().Get(name))
                        ServerSynchronization::DestroyGameObject(*gameObject);

                    SendReply(who, header.id, RpcType::S2C_DestroyGameObject, {});

                    break;
                }

                case RpcType::C2S_AddComponent:
                {
                    auto nul = std::ranges::find(packet, '\0');

                    std::string goName(packet.begin(), nul);
                    std::string type(nul + 1, std::find(nul + 1, packet.end(), '\0'));
                    std::vector blob(std::find(nul + 1, packet.end(), '\0') + 1, packet.end());

                    std::shared_ptr<Component> component;
                    NetworkSerialize::ObjectFromBytes(blob, component);

                    if (!component)
                    {
                        if (auto raw = ComponentFactory::Instantiate(type))
                            component = std::static_pointer_cast<Component>(raw);
                    }

                    if (component)
                    {
                        if (auto go = GameObjectManager::GetInstance().Get(goName))
                            ServerSynchronization::AddComponent(*go, component);
                    }

                    std::vector<std::uint8_t> pay(goName.begin(), goName.end());

                    pay.push_back('\0');
                    pay.insert(pay.end(), type.begin(), type.end());

                    SendReply(who, header.id, RpcType::S2C_AddComponent, pay);

                    std::cout << "C2S_AddComponent" << std::endl;

                    break;
                }

                case RpcType::C2S_RemoveComponent:
                {
                    auto lineEndIterator = std::ranges::find(packet, '\0');

                    std::string goName(packet.begin(), lineEndIterator);
                    std::string type(lineEndIterator + 1, packet.end());

                    if (auto gameObject = GameObjectManager::GetInstance().Get(goName))
                        ServerSynchronization::RemoveComponent(*gameObject, type);

                    std::vector<std::uint8_t> pay(goName.begin(), goName.end());
                    pay.push_back('\0');
                    pay.insert(pay.end(), type.begin(), type.end());

                    SendReply(who, header.id, RpcType::S2C_RemoveComponent, pay);

                    break;
                }

                case RpcType::C2S_AddChild:
                {
                    auto lineEndIterator = std::ranges::find(packet, '\0');

                    std::string parentName(packet.begin(), lineEndIterator);
                    std::string childName(lineEndIterator + 1, packet.end());

                    auto parent = GameObjectManager::GetInstance().Get(parentName);
                    auto child  = GameObjectManager::GetInstance().Get(childName);

                    if (parent && child)
                        ServerSynchronization::AddChild(*parent, *child);

                    std::vector<std::uint8_t> pay(parentName.begin(), parentName.end());
                    pay.push_back('\0');
                    pay.insert(pay.end(), childName.begin(), childName.end());

                    SendReply(who, header.id, RpcType::S2C_AddChild, pay);

                    break;
                }

                case RpcType::C2S_RemoveChild:
                {
                    auto lineEndIterator = std::ranges::find(packet, '\0');

                    std::string parentName(packet.begin(), lineEndIterator);
                    std::string childName(lineEndIterator + 1, packet.end());

                    if (auto parent = GameObjectManager::GetInstance().Get(parentName))
                        ServerSynchronization::RemoveChild(*parent, childName);

                    std::vector<std::uint8_t> pay(parentName.begin(), parentName.end());
                    pay.push_back('\0');
                    pay.insert(pay.end(), childName.begin(), childName.end());

                    SendReply(who, header.id, RpcType::S2C_RemoveChild, pay);

                    break;
                }

                case RpcType::C2S_TranslateTo:
                {
                    auto nul = std::ranges::find(packet, '\0');
                    std::string goName(packet.begin(), nul);

                    auto cursor = nul + 1;

                    auto readBlob = [&](auto& outObj)
                    {
                        if (cursor + 4 > packet.end())
                            throw std::runtime_error("malformed packet");

                        std::uint32_t len;
                        std::memcpy(&len, &*cursor, 4);

                        cursor += 4;

                        if (cursor + len > packet.end())
                            throw std::runtime_error("malformed packet");

                        NetworkSerialize::ObjectFromBytes({cursor, cursor + len}, outObj);

                        cursor += len;
                    };

                    Vector<float,3> target;
                    float seconds = 0.f;

                    readBlob(target);
                    readBlob(seconds);

                    if (auto gameObject = GameObjectManager::GetInstance().Get(goName))
                        (*gameObject)->GetTransform()->Translate(target);

                    SendReply(who, header.id, RpcType::S2C_TranslateTo, {});

                    std::cout << "C2S_TranslateTo" << std::endl;

                    break;
                }

                default:
                    break;
            }
        }

    private:

        ServerRpc() = default;

        static void SendReply(const NetworkId who, const std::uint64_t id, const RpcType type, std::span<const std::uint8_t> payload)
        {
            const RpcHeader header{ id, type };

            std::cout << "[RPC-RX] id=" << header.id
                << "  type=" << static_cast<int>(header.type)
                << "  from=" << who << '\n';

            std::vector<std::uint8_t> packet(sizeof header);
            std::memcpy(packet.data(), &header, sizeof header);

            packet.insert(packet.end(), payload.begin(), payload.end());

            ServerNetwork::GetInstance().SendTo(who, PacketType::S2C_Rpc, packet);
        }
    };
}
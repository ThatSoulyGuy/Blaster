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
            RpcHeader header = {};

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
                    std::string fullPath(packet.begin(), packet.end());

                    if (auto existing = FindGameObject(fullPath))
                    {
                        SendReply(who, header.id, RpcType::S2C_CreateGameObject, std::span(reinterpret_cast<const std::uint8_t*>(fullPath.data()), fullPath.size()));

                        break;
                    }

                    auto toView = [](auto &&sub)
                    {
                        return std::string_view(&*sub.begin(), std::ranges::distance(sub));
                    };

                    auto tokenView = std::string_view(fullPath) | std::views::split('.') | std::views::transform(toView);

                    auto tokenIterator = tokenView.begin();

                    if (tokenIterator == tokenView.end())
                    {
                        SendReply(who, header.id, RpcType::S2C_CreateGameObject, {});
                        break;
                    }

                    auto masterToken = *tokenIterator;

                    auto parent = GameObjectManager::GetInstance().Get(std::string{ masterToken });

                    if (!parent)
                        parent = ServerSynchronization::SpawnGameObject(std::string{ masterToken }, std::make_optional(who));

                    for (auto token : tokenView | std::views::drop(1))
                    {
                        if (auto child = parent.value()->GetChild(std::string{ token }))
                            parent = child;
                        else
                        {
                            auto fresh = ServerSynchronization::SpawnGameObject(std::string{ token }); //TODO: Does this need 'except: std::make_optional(who)'?

                            ServerSynchronization::AddChild(parent.value(), fresh, std::make_optional(who));

                            parent = fresh;
                        }
                    }

                    SendReply(who, header.id, RpcType::S2C_CreateGameObject, std::span( reinterpret_cast<const std::uint8_t*>( fullPath.data() ), fullPath.size() ) );

                    std::cout << "C2S_CreateGameObject" << std::endl;

                    break;
                }

                case RpcType::C2S_DestroyGameObject:
                {
                    std::string fullPath(packet.begin(), packet.end());

                    if (auto leaf = FindGameObject(fullPath))
                        ServerSynchronization::DestroyGameObject(*leaf);

                    SendReply(who, header.id, RpcType::S2C_DestroyGameObject, {});

                    break;
                }

                case RpcType::C2S_AddComponent:
                {
                    auto nul = std::ranges::find(packet, '\0');

                    std::string path(packet.begin(), nul);
                    std::string type(nul + 1, std::find(nul + 1, packet.end(), '\0' ));
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
                        if (auto target = FindGameObject(path))
                            ServerSynchronization::AddComponent(*target, component, std::make_optional(who));
                    }

                    std::vector<std::uint8_t> reply(path.begin(), path.end());

                    reply.push_back('\0' );
                    reply.insert(reply.end(), type.begin(), type.end());
                    reply.push_back('\0' );
                    reply.insert(reply.end(), blob.begin(), blob.end());

                    SendReply(who, header.id, RpcType::S2C_AddComponent, reply);

                    std::cout << "C2S_AddComponent" << std::endl;

                    break;
                }

                case RpcType::C2S_RemoveComponent:
                {
                    auto nul = std::ranges::find(packet, '\0');

                    std::string path(packet.begin(), nul);
                    std::string type(nul + 1, packet.end());

                    if (auto target = FindGameObject(path))
                        ServerSynchronization::RemoveComponent(*target, type);

                    std::vector<std::uint8_t> reply(path.begin(), path.end());

                    reply.push_back('\0');
                    reply.insert(reply.end(), type.begin(), type.end());

                    SendReply(who, header.id, RpcType::S2C_RemoveComponent, reply);

                    break;
                }

                case RpcType::C2S_AddChild:
                {
                    auto nul = std::ranges::find(packet, '\0');

                    std::string parentPath(packet.begin(), nul);
                    std::string childPath(nul + 1, packet.end());

                    auto current = [&](const std::string& path)
                    {
                        std::scoped_lock lock(worldMutex);
                        return EnsureGameObjectExists(path, who);
                    };

                    auto parentGO = current(parentPath);
                    auto childGO = current(childPath);

                    ServerSynchronization::AddChild(parentGO, childGO, std::make_optional(who));

                    std::vector<std::uint8_t> reply(parentPath.begin(), parentPath.end());

                    reply.push_back('\0');
                    reply.insert(reply.end(), childPath.begin(), childPath.end());

                    SendReply(who, header.id, RpcType::S2C_AddChild, reply);

                    std::cout << "C2S_AddChild\n";

                    break;
                }

                case RpcType::C2S_RemoveChild:
                {
                    auto nul = std::ranges::find(packet, '\0');

                    std::string parentPath(packet.begin(), nul);
                    std::string childPath(nul + 1, packet.end());

                    if (auto parent = FindGameObject(parentPath))
                        ServerSynchronization::RemoveChild(*parent, childPath);

                    std::vector<std::uint8_t> reply(parentPath.begin(), parentPath.end());

                    reply.push_back('\0');
                    reply.insert(reply.end(), childPath.begin(), childPath.end());

                    SendReply(who, header.id, RpcType::S2C_RemoveChild, reply);

                    break;
                }

                case RpcType::C2S_TranslateTo:
                {
                    auto nul = std::ranges::find(packet, '\0');
                    std::string path(packet.begin(), nul);

                    auto cursor = nul + 1;

                    auto readBlob = [&](auto &dst)
                    {
                        std::uint32_t len;
                        std::memcpy(&len, &*cursor, 4);

                        cursor += 4;

                        NetworkSerialize::ObjectFromBytes({cursor, cursor + len}, dst);

                        cursor += len;
                    };

                    Vector<float, 3> target{};
                    float seconds = 0.0f;

                    readBlob(target);
                    readBlob(seconds);

                    if (auto gameObject = FindGameObject(path))
                        (*gameObject)->GetTransform()->SetLocalPosition(target);

                    std::vector<std::uint8_t> payload;

                    payload.reserve(packet.size() + 1);
                    payload.insert(payload.end(), path.begin(), path.end());
                    payload.push_back('\0');

                    auto pushBlob = [&](const auto& obj)
                    {
                        auto blob = NetworkSerialize::ObjectToBytes(obj);

                        auto len = static_cast<std::uint32_t>(blob.size());

                        payload.insert(payload.end(), reinterpret_cast<std::uint8_t*>(&len), reinterpret_cast<std::uint8_t*>(&len) + sizeof len);
                        payload.insert(payload.end(), blob.begin(), blob.end());
                    };

                    pushBlob(target);
                    pushBlob(seconds);

                    SendReply(who, header.id, RpcType::S2C_TranslateTo, {});

                    ServerNetwork::GetInstance().Broadcast(PacketType::S2C_TranslateTo, payload, who);

                    std::cout << "C2S_TranslateTo from " << who << std::endl;
                    break;
                }

                default:
                    break;
            }
        }

    private:

        ServerRpc() = default;

        static std::shared_ptr<GameObject> EnsureGameObjectExists(std::string_view fullPath, const std::optional<NetworkId> exceptClient = std::nullopt)
        {
            auto toView = [](auto &&sub)
            {
                return std::string_view(&*sub.begin(), std::ranges::distance(sub));
            };

            auto tokenView = fullPath | std::views::split('.') | std::views::transform(toView);
            const auto tokenIter = tokenView.begin();

            if (tokenIter == tokenView.end())
                throw std::runtime_error("empty game-object path");

            auto current = GameObjectManager::GetInstance().Get(std::string{ *tokenIter });

            if (!current)
                current = ServerSynchronization::SpawnGameObject(std::string{ *tokenIter }, std::nullopt, exceptClient);

            for (auto seg : tokenView | std::views::drop(1))
            {
                if (const auto child = current.value()->GetChild(std::string{ seg }))
                    current = child;
                else
                {
                    auto fresh = ServerSynchronization::SpawnGameObject(std::string{ seg }, std::nullopt, exceptClient);
                    ServerSynchronization::AddChild(*current, fresh, exceptClient);
                    current = fresh;
                }
            }

            return *current;
        }

        static std::optional<std::shared_ptr<GameObject>> FindGameObject(std::string_view path)
        {
            auto toView = [](auto &&sub)
            {
                return std::string_view(&*sub.begin(), std::ranges::distance(sub));
            };

            auto tokenView = path | std::views::split( '.' ) | std::views::filter([](auto sub){ return !sub.empty(); }) | std::views::transform(toView);
            const auto tokenIter = tokenView.begin();

            if (tokenIter == tokenView.end())
                return std::nullopt;

            auto current = GameObjectManager::GetInstance().Get( std::string{ *tokenIter } );

            for(auto seg : tokenView | std::views::drop(1))
            {
                current = current.and_then([&](const std::shared_ptr<GameObject> &node)
                {
                    return node->GetChild(std::string{ seg });
                });
            }

            return current;
        };

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

        static std::mutex worldMutex;
    };

    std::mutex ServerRpc::worldMutex;
}
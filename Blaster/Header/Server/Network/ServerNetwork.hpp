#pragma once

#include <memory>
#include <mutex>
#include <array>
#include <deque>
#include <ranges>
#include <thread>
#include <boost/asio.hpp>

#include "Independent/Network/CommonNetwork.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Server::Network
{
    class ServerNetwork final
    {

    public:

        ServerNetwork(const ServerNetwork&) = delete;
        ServerNetwork(ServerNetwork&&) = delete;
        ServerNetwork& operator=(const ServerNetwork&) = delete;
        ServerNetwork& operator=(ServerNetwork&&) = delete;

        struct ClientReference
        {
            TcpProtocol::socket socket;
            boost::asio::basic_stream_socket<boost::asio::ip::tcp>::executor_type strand;

            std::deque<std::shared_ptr<std::vector<std::uint8_t>>> writeQueue;

            std::vector<std::weak_ptr<GameObject>> ownedGameObjectList;

            NetworkId id{};
            std::array<std::uint8_t, 512> readBuffer{};
            std::vector<std::uint8_t> inbox;

            explicit ClientReference(TcpProtocol::socket sock) : socket(std::move(sock)), strand(socket.get_executor()) { }
        };

        void Initialize(const std::uint16_t port)
        {
            if (running)
                return;

            acceptor.emplace(ioContext, TcpProtocol::endpoint(TcpProtocol::v4(), port));

            DoAccept();

            ioThread = std::thread([this]{ ioContext.run(); });
            running  = true;
        }

        void RegisterReceiver(const PacketType type, std::function<void(NetworkId, std::vector<std::uint8_t>)> function)
        {
            packetHandlerMap[type].push_back(std::move(function));
        }

        void SendTo(const NetworkId id, const PacketType type, const std::span<const std::uint8_t> payload)
        {
            const auto iterator = clientMap.find(id);

            if (iterator == clientMap.end())
                return;

            auto buffer = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(type, 0, payload));

            boost::asio::dispatch(iterator->second->strand, [this, client = iterator->second, buf = std::move(buffer)]() mutable
            {
                client->writeQueue.push_back(std::move(buf));

                if (client->writeQueue.size() == 1)
                    StartWrite(client);
            });
        }

        void Broadcast(const PacketType type, const std::span<const std::uint8_t> payload, const std::optional<NetworkId> except = std::nullopt)
        {
            for (const auto& id: clientMap | std::views::keys)
            {
                if (except.has_value() && id == except.value())
                    continue;

                SendTo(id, type, payload);
            }
        }

        bool IsRunning() const
        {
            return running;
        }

        bool HasClient(const NetworkId id) const
        {
            return clientMap.contains(id);
        }

        std::optional<std::shared_ptr<ClientReference>> GetClient(const NetworkId id)
        {
            if (!clientMap.contains(id))
            {
                std::cerr << "Client map doesn't contain client id '" << id << "'!";
                return std::nullopt;
            }

            return std::make_optional(clientMap[id]);
        }

        std::vector<NetworkId> GetConnectedClients() const
        {
            auto result = clientMap | std::views::keys;

            return { result.begin(), result.end() };
        }

        void Uninitialize()
        {
            if (!running)
                return;

            ioContext.stop();

            if (ioThread.joinable())
                ioThread.join();

            for (auto &client: clientMap | std::views::values)
                CleanupOwnedObjects(client);

            clientMap.clear();

            running = false;
        }

        static ServerNetwork& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<ServerNetwork>(new ServerNetwork());
            });

            return *instance;
        }

    private:

        ServerNetwork() = default;

        void StartWrite(const std::shared_ptr<ClientReference>& client)
        {
            boost::asio::async_write(client->socket, boost::asio::buffer(*client->writeQueue.front()), boost::asio::bind_executor(client->strand, [client, this](const boost::system::error_code& error, std::size_t)
            {
                client->writeQueue.pop_front();

                if (error)
                {
                    std::cerr << "Error during packet sending! " << error << std::endl;

                    clientMap.erase(client->id);
                    CleanupOwnedObjects(client);

                    return;
                }

                if (!client->writeQueue.empty())
                    StartWrite(client);
            }));
        }

        void DoAccept()
        {
            acceptor->async_accept([this](const ErrorCode& errorCode, TcpProtocol::socket socket)
                {
                    if (!errorCode)
                    {
                        socket.set_option(TcpProtocol::no_delay(true));

                        const auto client  = std::make_shared<ClientReference>(ClientReference{std::move(socket)});

                        client->id = nextId += 1;
                        clientMap[client->id] = client;

                        std::array<std::uint8_t, sizeof(NetworkId)> networkIdBuffer{};
                        std::memcpy(networkIdBuffer.data(), &client->id, sizeof(NetworkId));

                        auto assign = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(PacketType::S2C_AssignNetworkId,
                                                                                      0,
                                                                                      std::span(networkIdBuffer.data(), networkIdBuffer.size())));
                        boost::asio::async_write(client->socket, boost::asio::buffer(*assign), [assign](auto, auto){ });

                        auto ask = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(PacketType::S2C_RequestStringId, 0, { }));

                        boost::asio::async_write(client->socket, boost::asio::buffer(*ask), [ask](auto, auto){ });

                        BeginRead(client);
                    }

                    DoAccept();
                });
        }

        void BeginRead(const std::shared_ptr<ClientReference>& client)
        {
            client->socket.async_read_some(boost::asio::buffer(client->readBuffer), [this, client](const ErrorCode& errorCode, const std::size_t number)
                {
                    if (errorCode)
                    {
                        clientMap.erase(client->id);

                        CleanupOwnedObjects(client);

                        return;
                    }

                    client->inbox.insert(client->inbox.end(), client->readBuffer.data(), client->readBuffer.data() + number);

                    while (client->inbox.size() >= sizeof(PacketHeader))
                    {
                        auto* header = reinterpret_cast<const PacketHeader*>(client->inbox.data());

                        const std::size_t needed = sizeof(PacketHeader) + header->size;

                        if (client->inbox.size() < needed)
                            break;

                        std::vector<std::uint8_t> payload;

                        payload.resize(header->size);

                        std::memcpy(payload.data(), client->inbox.data() + sizeof(PacketHeader), header->size);

                        HandlePacket(client->id, *header, std::move(payload));

                        client->inbox.erase(client->inbox.begin(), client->inbox.begin() + needed);
                    }

                    BeginRead(client);
                });
        }

        void HandlePacket(const NetworkId from, const PacketHeader& header, std::vector<std::uint8_t>&& data)
        {
            if (const auto iterator = packetHandlerMap.find(header.type); iterator != packetHandlerMap.end())
            {
                for (auto& function : iterator->second)
                    function(from, std::move(data));
            }
        }

        static void BroadcastDestroy(const std::string& goName)
        {
            std::vector<std::uint8_t> pkt(goName.begin(), goName.end());
            GetInstance().Broadcast(PacketType::S2C_DestroyGameObject, pkt);
        }

        static void DestroySubtree(const std::shared_ptr<GameObject>& node)
        {
            std::vector<std::shared_ptr<GameObject>> children;

            for (const auto& ch : node->GetChildMap() | std::views::values)
                children.emplace_back(ch);

            for (auto& child : children)
            {
                node->RemoveChild(child->GetName());
                DestroySubtree(child);
            }

            if (auto parentW = node->GetParent(); parentW && !parentW->expired())
            {
                auto parent = parentW->lock();
                parent->RemoveChild(node->GetName());
            }
            else
                GameObjectManager::GetInstance().Unregister(node->GetName());

            BroadcastDestroy(node->GetName());
        }

        static void CleanupOwnedObjects(const std::shared_ptr<ClientReference>& client)
        {
            for (auto& weak : client->ownedGameObjectList)
                if (auto root = weak.lock())
                    DestroySubtree(root);

            client->ownedGameObjectList.clear();
        }

        boost::asio::io_context ioContext;
        std::optional<TcpProtocol::acceptor> acceptor;
        std::thread ioThread;
        std::atomic<bool> running = false;
        std::atomic<NetworkId> nextId = 0;

        std::unordered_map<NetworkId, std::shared_ptr<ClientReference>> clientMap;

        std::unordered_map<PacketType, std::vector<std::function<void(NetworkId, std::vector<std::uint8_t>)>>> packetHandlerMap;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ServerNetwork> instance;

    };

    std::once_flag ServerNetwork::initializationFlag;
    std::unique_ptr<ServerNetwork> ServerNetwork::instance;
}

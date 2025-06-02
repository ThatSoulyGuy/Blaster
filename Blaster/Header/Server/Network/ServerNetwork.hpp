#pragma once

#include <memory>
#include <mutex>
#include <array>
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
            const auto iterator = clients.find(id);

            if (iterator == clients.end())
                return;

            auto buffer = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(type, 0, payload));

            boost::asio::async_write(iterator->second->sock, boost::asio::buffer(*buffer), [buffer](auto, auto) { });
        }

        void Broadcast(const PacketType type, const std::span<const std::uint8_t> payload)
        {
            for (const auto& id: clients | std::views::keys)
                SendTo(id, type, payload);
        }

        bool IsRunning() const
        {
            return running;
        }

        void Uninitialize()
        {
            if (!running)
                return;

            ioContext.stop();

            if (ioThread.joinable())
                ioThread.join();

            clients.clear();

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

        struct Client
        {
            TcpProtocol::socket sock;

            NetworkId id;

            std::array<std::uint8_t, 512> readBuffer;
            std::vector<std::uint8_t> inbox;
        };

        void DoAccept()
        {
            acceptor->async_accept([this](const ErrorCode& errorCode, TcpProtocol::socket socket)
                {
                    if (!errorCode)
                    {
                        const auto client  = std::make_shared<Client>(Client{std::move(socket)});

                        client->id = nextId += 1;
                        clients[client->id] = client;

                        std::array<std::uint8_t, sizeof(NetworkId)> networkIdBuffer;
                        std::memcpy(networkIdBuffer.data(), &client->id, sizeof(NetworkId));

                        auto assign = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(PacketType::S2C_AssignNetworkId,
                                                                                      0,
                                                                                      std::span(networkIdBuffer.data(), networkIdBuffer.size())));
                        boost::asio::async_write(client->sock, boost::asio::buffer(*assign), [assign](auto, auto){ });

                        auto ask = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(PacketType::S2C_RequestStringId, 0, { }));

                        boost::asio::async_write(client->sock, boost::asio::buffer(*ask), [ask](auto, auto){ });

                        BeginRead(client);
                    }

                    DoAccept();
                });
        }

        void BeginRead(const std::shared_ptr<Client>& client)
        {
            client->sock.async_read_some(boost::asio::buffer(client->readBuffer), [this, client](const ErrorCode& errorCode, const std::size_t number)
                {
                    if (errorCode)
                    {
                        clients.erase(client->id);
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

                        std::memcpy(payload.data(),
                                    client->inbox.data() + sizeof(PacketHeader),
                                    header->size);

                        HandlePacket(client->id, *header, std::move(payload));

                        client->inbox.erase(client->inbox.begin(), client->inbox.begin() + needed);
                    }

                    BeginRead(client);
                });
        }

        void HandlePacket(const NetworkId from,
                          const PacketHeader& header,
                          std::vector<std::uint8_t>&& data)
        {
            if (const auto iterator = packetHandlerMap.find(header.type); iterator != packetHandlerMap.end())
            {
                for (auto& function : iterator->second)
                    function(from, std::move(data));
            }
        }

        boost::asio::io_context ioContext;
        std::optional<TcpProtocol::acceptor> acceptor;
        std::thread ioThread;
        std::atomic<bool> running = false;
        std::atomic<NetworkId> nextId = 0;

        std::unordered_map<NetworkId, std::shared_ptr<Client>> clients;

        std::unordered_map<PacketType, std::vector<std::function<void(NetworkId, std::vector<std::uint8_t>)>>> packetHandlerMap;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ServerNetwork> instance;

    };

    std::once_flag ServerNetwork::initializationFlag;
    std::unique_ptr<ServerNetwork> ServerNetwork::instance;
}

#pragma once

#include <memory>
#include <mutex>
#include <array>
#include <deque>
#include <queue>
#include <ranges>
#include <thread>
#include <iostream>
#include <boost/asio.hpp>
#include "Independent/Network/CommonNetwork.hpp"

using namespace Blaster::Independent::ECS;
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

            boost::asio::strand<boost::asio::any_io_executor> strand;

            std::deque<std::shared_ptr<std::vector<std::uint8_t>>> writeQueue;
            std::unordered_map<std::string, std::weak_ptr<IGameObjectSynchronization>> ownedGameObjectList;

            NetworkId id{};
            std::string stringId = "!";

            std::array<std::uint8_t, 512> readBuffer{};
            std::vector<std::uint8_t> inbox;

            boost::asio::steady_timer disconnectTimer{ socket.get_executor() };

            explicit ClientReference(TcpProtocol::socket sock) : socket(std::move(sock)), strand(boost::asio::make_strand(socket.get_executor())) { }
        };

        void Initialize(const std::uint16_t port)
        {
            if (running)
                return;

            acceptor.emplace(ioContext, TcpProtocol::endpoint(TcpProtocol::v4(), port));

            DoAccept();

            ioThread = std::thread([this]
            {
                try
                {
                    ioContext.run();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "io_context.run() aborted: " << e.what() << '\n';
                }
            });

            running  = true;
        }

        void RegisterReceiver(const PacketType type, std::function<void(NetworkId, std::vector<std::uint8_t>)> function)
        {
            packetHandlerMap[type].push_back(std::move(function));
        }

        template <typename... Args> requires DataConvertible<Args...>
        void SendTo(const NetworkId id, const PacketType type, Args&&... args)
        {
            const auto resul = clientMap.find(id);

            if (resul == clientMap.end())
                return;

            auto buf = std::make_shared<std::vector<std::uint8_t>>(CommonNetwork::BuildPacket(type, 0, std::forward<Args>(args)...));

            boost::asio::post(resul->second->strand, [this, client = resul->second, buf]()
            {
                client->writeQueue.push_back(buf);

                if (client->writeQueue.size() == 1)
                    StartWrite(client);
            });
        }
        
        void ForwardTo(const NetworkId id, const PacketType type, std::vector<std::uint8_t> dataIn)
        {
            const auto resul = clientMap.find(id);

            if (resul == clientMap.end())
                return;

            auto data = std::make_shared<std::vector<std::uint8_t>>(dataIn);

            boost::asio::post(resul->second->strand, [this, client = resul->second, data]()
                {
                    client->writeQueue.push_back(data);

                    if (client->writeQueue.size() == 1)
                        StartWrite(client);
                });
        }

        template <typename... Args> requires DataConvertible<Args...>
        void Broadcast(const PacketType type, const std::optional<NetworkId> except, Args&&... args)
        {
            for (const auto& id : clientMap | std::views::keys)
            {
                if (except.has_value() && id == except.value())
                    continue;

                SendTo(id, type, std::forward<Args>(args)...);
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

        void AddOnClientDisconnectedCallback(const std::function<void(std::shared_ptr<ClientReference>)>& callback)
        {
            onClientDisconnectedCallbackList.push_back(callback);
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

        auto& GetIoContext()
        {
            return ioContext;
        }

        void DisconnectClient(NetworkId id)
        {
            if (auto client = GetClient(id))
            {
                boost::system::error_code errorCode;
                client->get()->socket.close(errorCode);
            }
        }

        void Uninitialize()
        {
            if (!running)
                return;

            ioContext.stop();

            if (ioThread.joinable())
                ioThread.join();

            for (auto& client : clientMap | std::views::values)
            {
                for (auto& callback : onClientDisconnectedCallbackList)
                    callback(client);
            }

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

                    StartDisconnectTimer(client);

                    return;
                }

                CancelDisconnectTimer(client);

                if (!client->writeQueue.empty())
                    StartWrite(client);
            }));
        }

        void DoAccept()
        {
            auto peerSocket = std::make_shared<TcpProtocol::socket>(ioContext);

            acceptor->async_accept(*peerSocket, [this, peerSocket](const ErrorCode& errorCode)
            {
                if (errorCode)
                {
                    std::cerr << "accept failed: " << errorCode.message() << '\n';
                    DoAccept();

                    return;
                }

                if (!peerSocket->is_open())
                {
                    std::cerr << "accept produced a closed socket\n";
                    DoAccept();

                    return;
                }

                {
                    boost::system::error_code localErrorCode;

                    (void)peerSocket->set_option(TcpProtocol::no_delay(true), localErrorCode);

                    if (localErrorCode)
                        std::cerr << "TCP_NODELAY failed: " << localErrorCode.message() << '\n';
                }

                #if defined(__APPLE__)
                {
                    int one = 1;
                    ::setsockopt(peerSocket->native_handle(), SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
                }
                #endif

                auto client = std::make_shared<ClientReference>(TcpProtocol::socket(std::move(*peerSocket)));

                client->id = AcquireId();
                clientMap[client->id] = client;

                auto assign = std::make_shared<std::vector<std::uint8_t>>(CommonNetwork::BuildPacket(PacketType::S2C_AssignNetworkId, 0, client->id));
                auto ask = std::make_shared<std::vector<std::uint8_t>>(CommonNetwork::BuildPacket(PacketType::S2C_RequestStringId, 0, 0));

                boost::asio::post(client->strand, [this, client, assign, ask]()
                {
                    const bool idle = client->writeQueue.empty();

                    client->writeQueue.push_back(assign);
                    client->writeQueue.push_back(ask);

                    if (idle)
                        StartWrite(client);
                });

                BeginRead(client);
                DoAccept();
            });
        }

        void BeginRead(const std::shared_ptr<ClientReference>& client)
        {
            client->socket.async_read_some(boost::asio::buffer(client->readBuffer), boost::asio::bind_executor(client->strand, [this, client](const ErrorCode& errorCode, const std::size_t number)
            {
                if (errorCode) { StartDisconnectTimer(client); return; }

                CancelDisconnectTimer(client);
                client->inbox.insert(client->inbox.end(), client->readBuffer.data(), client->readBuffer.data() + number);

                constexpr std::size_t kMaxPayload = 4 * 1024 * 1024;

                while (client->inbox.size() >= CommonNetwork::kHeaderBytes)
                {
                    auto hdr = CommonNetwork::ReadHeader(std::span<const std::uint8_t>(client->inbox.data(), CommonNetwork::kHeaderBytes));
                    const std::size_t needed = CommonNetwork::kHeaderBytes + hdr.size;

                    if (hdr.size > kMaxPayload) 
                    {
                        std::cerr << "ServerNetwork: invalid packet size " << hdr.size << " from client " << client->id << '\n';

                        StartDisconnectTimer(client);

                        return;
                    }

                    if (client->inbox.size() < needed)
                        break;

                    std::vector<std::uint8_t> payload(hdr.size);
                    std::memcpy(payload.data(), client->inbox.data() + CommonNetwork::kHeaderBytes, hdr.size);

                    client->inbox.erase(client->inbox.begin(), client->inbox.begin() + needed);

                    PacketHeader header{};

                    header.type = static_cast<PacketType>(hdr.type);
                    header.size = hdr.size;
                    header.from = hdr.from;
                    header.sequence = hdr.sequence;

                    try
                    {
                        HandlePacket(client->id, header, std::move(payload));
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "ServerNetwork: packet handler threw: " << e.what() << '\n';
                    }
                    catch (...)
                    {
                        std::cerr << "ServerNetwork: packet handler threw unknown exception\n";
                    }
                }

                BeginRead(client);
            }));
        }

        void HandleDisconnect(const std::shared_ptr<ClientReference>& client)
        {
            for (auto& callback : onClientDisconnectedCallbackList)
                callback(client);

            boost::system::error_code ignored;

            client->socket.shutdown(boost::asio::socket_base::shutdown_both, ignored);
            client->socket.close(ignored);

            clientMap.erase(client->id);

            std::cout << "Client '" << client->stringId << "' with id '" << client->id << "' has disconnected!\n";
        }

        void HandlePacket(const NetworkId from, const PacketHeader& header, std::vector<std::uint8_t>&& data)
        {
            if (const auto iterator = packetHandlerMap.find(header.type); iterator != packetHandlerMap.end())
            {
                for (auto& function : iterator->second)
                {
                    try
                    {
                        function(from, data);
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "ServerNetwork: receiver for packet " << (int)header.type << " threw: " << e.what() << '\n';
                    }
                }
            }
        }

        NetworkId AcquireId()
        {
            return ++nextId;
        }

        void StartDisconnectTimer(const std::shared_ptr<ClientReference>& client)
        {
            if (client->disconnectTimer.expiry() != boost::asio::steady_timer::time_point::max())
                return;

            client->disconnectTimer.expires_after(std::chrono::seconds(2));
            client->disconnectTimer.async_wait([this, wp = std::weak_ptr(client)](const boost::system::error_code& ec)
                {
                    if (ec == boost::asio::error::operation_aborted)
                        return;

                    if (auto sp = wp.lock())
                        HandleDisconnect(sp);
                });
        }

        void CancelDisconnectTimer(const std::shared_ptr<ClientReference>& client)
        {
            client->disconnectTimer.expires_at(boost::asio::steady_timer::time_point::max());

            client->disconnectTimer.cancel();
        }

        boost::asio::io_context ioContext;
        std::optional<TcpProtocol::acceptor> acceptor;
        std::thread ioThread;
        std::atomic<bool> running = false;
        std::atomic<NetworkId> nextId = 0;

        std::vector<std::function<void(std::shared_ptr<ClientReference>)>> onClientDisconnectedCallbackList;

        std::unordered_map<NetworkId, std::shared_ptr<ClientReference>> clientMap;

        std::unordered_map<PacketType, std::vector<std::function<void(NetworkId, std::vector<std::uint8_t>)>>> packetHandlerMap;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ServerNetwork> instance;

    };
}
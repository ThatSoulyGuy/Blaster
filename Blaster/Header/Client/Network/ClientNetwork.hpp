#pragma once

#include <memory>
#include <mutex>
#include <array>
#include <deque>
#include <thread>
#include <boost/asio.hpp>
#include "Independent/Network/CommonNetwork.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Client::Network
{
    class ClientNetwork final
    {

    public:

        ClientNetwork(const ClientNetwork&) = delete;
        ClientNetwork(ClientNetwork&&) = delete;
        ClientNetwork& operator=(const ClientNetwork&) = delete;
        ClientNetwork& operator=(ClientNetwork&&) = delete;

        void Initialize(const std::string_view host, const std::uint16_t port, const std::string& stringId)
        {
            if (running)
                return;

            this->stringId = stringId;

            TcpProtocol::resolver res{ioContext};

            const auto resolution = res.resolve(host, std::to_string(port));

            try
            {
                boost::asio::connect(socket, resolution);
            }
            catch (const boost::system::system_error& error)
            {
                std::cerr << "Connect failed: " << error.what() << '\n';
                return;
            }

            socket.set_option(TcpProtocol::no_delay(true));

            BeginRead();

            ioThread = std::thread([this]{ ioContext.run(); });
            running  = true;
        }

        void RegisterReceiver(const PacketType type, std::function<void(std::vector<std::uint8_t>)> function)
        {
            boost::asio::post(strand, [this, type, receiver = std::move(function)]() mutable
                {
                    packetHandlerMap[type].push_back(std::move(receiver));
                });
        }

        void Send(const PacketType type, const std::span<const std::uint8_t> payload)
        {
            auto buf = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(type, networkId, payload));

            boost::asio::post(strand, [this, buf]
                {
                    writeQueue.push_back(buf);

                    if (writeQueue.size() == 1)
                        StartWrite();
                });
        }

        std::string GetStringId() const
        {
            return stringId;
        }

        NetworkId GetNetworkId() const
        {
            return networkId;
        }

        void Uninitialize()
        {
            if (!running)
                return;

            ioContext.stop();

            if (ioThread.joinable())
                ioThread.join();

            running = false;
        }

        static ClientNetwork& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<ClientNetwork>(new ClientNetwork());
            });

            return *instance;
        }

    private:

        ClientNetwork() = default;

        void StartWrite()
        {
            boost::asio::async_write(socket, boost::asio::buffer(*writeQueue.front()), boost::asio::bind_executor(strand,
                [this](const ErrorCode& error, std::size_t)
                {
                    writeQueue.pop_front();

                    if (error)
                    {
                        std::cerr << "ClientNetwork: write failed: " << error.message() << '\n';
                        return;
                    }

                    if (!writeQueue.empty())
                        StartWrite();
                }));
        }

        void BeginRead()
        {
            socket.async_read_some(boost::asio::buffer(readBuffer), boost::asio::bind_executor(strand, [this] (const ErrorCode& errorCode, const std::size_t number)
                {
                    if (errorCode)
                        return;

                    inbox.insert(inbox.end(), readBuffer.data(), readBuffer.data() + number);

                    while (inbox.size() >= sizeof(PacketHeader))
                    {
                        auto* header = reinterpret_cast<const PacketHeader*>(inbox.data());

                        const std::size_t need = sizeof(PacketHeader) + header->size;

                        if (inbox.size() < need)
                            break;

                        std::vector<std::uint8_t> payload(header->size);

                        std::memcpy(payload.data(), inbox.data() + sizeof(PacketHeader), header->size);

                        HandlePacket(*header, std::move(payload));

                        inbox.erase(inbox.begin(), inbox.begin() + need);
                    }

                    BeginRead();
                }));
        }

        void HandlePacket(const PacketHeader& header, std::vector<std::uint8_t>&& data)
        {
            if (header.type == PacketType::S2C_RequestStringId)
            {
                Send(PacketType::C2S_StringId, std::span(reinterpret_cast<const std::uint8_t*>(stringId.data()), stringId.size()));
                return;
            }

            if (header.type == PacketType::S2C_AssignNetworkId)
            {
                NetworkId id = 0;

                std::memcpy(&id, data.data(), sizeof(NetworkId));

                networkId = id;

                return;
            }

            if (const auto hit = packetHandlerMap.find(header.type); hit != packetHandlerMap.end())
            {
                for (auto& function: hit->second)
                    function(std::move(data));
            }
        }

        boost::asio::io_context ioContext;
        TcpProtocol::socket socket{ioContext};
        std::thread ioThread;
        std::atomic<bool> running = false;

        std::string stringId;
        NetworkId networkId = 0;

        std::array<std::uint8_t, 512> readBuffer = { };
        std::vector<std::uint8_t> inbox;

        boost::asio::strand<boost::asio::io_context::executor_type> strand = boost::asio::make_strand(ioContext);

        std::unordered_map<PacketType, std::vector<std::function<void(std::vector<std::uint8_t>)>>> packetHandlerMap;

        std::deque<std::shared_ptr<std::vector<std::uint8_t>>> writeQueue;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientNetwork> instance;

    };

    std::once_flag ClientNetwork::initializationFlag;
    std::unique_ptr<ClientNetwork> ClientNetwork::instance;
}
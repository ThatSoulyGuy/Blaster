#pragma once

#include <memory>
#include <mutex>
#include <array>
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

            boost::asio::connect(socket, resolution);

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
            auto buffer = std::make_shared<std::vector<std::uint8_t>>(CreatePacket(type, networkId, payload));

            boost::asio::post(strand, [this, buffer]
                {
                    boost::asio::async_write(socket, boost::asio::buffer(*buffer), [buffer](auto, auto) {});
                });
        }

        std::string GetStringId() const
        {
            return stringId;
        }

        NetworkID GetNetworkId() const
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
                NetworkID id = 0;

                std::memcpy(&id, data.data(), sizeof(NetworkID));

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
        NetworkID networkId = 0;

        std::array<std::uint8_t, 512> readBuffer = { };
        std::vector<std::uint8_t> inbox;

        boost::asio::strand<boost::asio::io_context::executor_type> strand = boost::asio::make_strand(ioContext);

        std::unordered_map<PacketType, std::vector<std::function<void(std::vector<std::uint8_t>)>>> packetHandlerMap;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientNetwork> instance;

    };

    std::once_flag ClientNetwork::initializationFlag;
    std::unique_ptr<ClientNetwork> ClientNetwork::instance;
}
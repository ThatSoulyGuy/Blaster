#pragma once

#include <memory>
#include <mutex>
#include <array>
#include <deque>
#include <thread>
#include <boost/asio.hpp>
#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"

using namespace Blaster::Independent::Network;
using namespace Blaster::Independent::Thread;

namespace Blaster::Client::Network
{
    class ClientNetwork final
    {

    public:

        ClientNetwork(const ClientNetwork&) = delete;
        ClientNetwork(ClientNetwork&&) = delete;
        ClientNetwork& operator=(const ClientNetwork&) = delete;
        ClientNetwork& operator=(ClientNetwork&&) = delete;

        void Initialize(std::string_view host, std::uint16_t port, const std::string& stringId)
        {
            if (running)
                return;

            this->stringId = stringId;

            haveNetworkId = false;
            pendingStringId = false;
            sentStringId = false;
            networkId = 0;

            ioContext.restart();

            strand = boost::asio::make_strand(ioContext);
            workGuard.emplace(ioContext.get_executor());

            {
                disconnectTimer.cancel();
                disconnectTimerActive = false;

                boost::system::error_code ec;
                socket.close(ec);
            }

            socket = TcpProtocol::socket{ ioContext };

            TcpProtocol::resolver resolver{ ioContext };
            const auto resolution = resolver.resolve(host, std::to_string(port));

            try 
            {
                boost::asio::connect(socket, resolution);
            }
            catch (const boost::system::system_error& error)
            {
                std::cerr << "Connect failed: " << error.what() << '\n';
                workGuard.reset();

                return;
            }

            socket.set_option(TcpProtocol::no_delay(true));

            BeginRead();

            ioThread = std::thread([this] { ioContext.run(); });
            running = true;
        }

        void RegisterReceiver(const PacketType type, std::function<void(std::vector<std::uint8_t>)> function)
        {
            boost::asio::post(strand, [this, type, receiver = std::move(function)]() mutable
                {
                    packetHandlerMap[type].push_back(std::move(receiver));
                });
        }

        template <typename... Args> requires DataConvertible<Args...>
        void Send(const PacketType type, Args&&... args)
        {
            auto buffer = std::make_shared<std::vector<std::uint8_t>>(CommonNetwork::BuildPacket(type, networkId, std::forward<Args>(args)...));

            boost::asio::post(strand, [this, buffer]
                {
                    writeQueue.push_back(buffer);

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

        bool IsRunning() const
        {
            return running.load();
        }

        void AddOnServerConnectionLostCallback(const std::function<void()>& callback)
        {
            onServerConnectionLostCallbackList.push_back(callback);
        }

        auto& GetIoContext()
        {
            return ioContext;
        }

        void Disconnect(bool notifyCallbacks = true)
        {
            if (!running.load(std::memory_order_acquire))
                return;

            boost::asio::post(strand, [this, notifyCallbacks]()
                {
                    if (disconnecting.exchange(true, std::memory_order_acq_rel))
                        return;

                    disconnectTimer.cancel();
                    disconnectTimerActive = false;

                    writeQueue.clear();

                    boost::system::error_code ec;
                    socket.shutdown(TcpProtocol::socket::shutdown_both, ec);
                    socket.close(ec);

                    MainThreadExecutor::GetInstance().EnqueueTask(this, [this, notifyCallbacks]()
                        {
                            if (notifyCallbacks)
                                NotifyConnectionLost();
                            else
                                Uninitialize();
                        });
                });
        }

        void Uninitialize()
        {
            disconnectTimer.cancel();
            disconnectTimerActive = false;

            boost::system::error_code errorCode;
            socket.close(errorCode);

            inbox.clear();
            writeQueue.clear();

            workGuard.reset();

            ioContext.stop();

            if (ioThread.joinable())
                ioThread.join();

            networkId = 0;
            haveNetworkId = false;
            pendingStringId = false;
            sentStringId = false;
            running = false;

            disconnecting.store(false, std::memory_order_release);
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

        using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

        ClientNetwork() = default;

        void NotifyConnectionLost()
        {
            std::vector<std::function<void()>> toRun;

            {
                std::lock_guard lock(callbackMutex);

                toRun = onServerConnectionLostCallbackList;
            }

            Uninitialize();

            for (auto& callback : toRun)
            {
                try
                {
                    callback();
                }
                catch (std::exception exception)
                {
                    std::cerr << "Execption thrown from functionIn: '" << exception.what() << "'!" << std::endl;
                }
            }
        }

        void StartWrite()
        {
            boost::asio::async_write(socket, boost::asio::buffer(*writeQueue.front()), boost::asio::bind_executor(strand, [this](const ErrorCode& error, std::size_t)
                {
                    writeQueue.pop_front();

                    if (error)
                    {
                        std::cerr << "ClientNetwork: write failed: " << error.message() << '\n';
                        StartDisconnectCountdown();

                        return;
                    }

                    CancelDisconnectCountdown();

                    if (!writeQueue.empty())
                        StartWrite();
                }));
        }

        void BeginRead()
        {
            socket.async_read_some(boost::asio::buffer(readBuffer), boost::asio::bind_executor(strand, [this](const ErrorCode& errorCode, const std::size_t number)
            {
                if (errorCode)
                {
                    std::cerr << "ClientNetwork: read failed: " << errorCode.message() << '\n';
                    StartDisconnectCountdown();

                    return;
                }

                CancelDisconnectCountdown();

                inbox.insert(inbox.end(), readBuffer.data(), readBuffer.data() + number);

                constexpr std::size_t kMaxPayload = 4 * 1024 * 1024;

                while (inbox.size() >= CommonNetwork::kHeaderBytes)
                {
                    const auto hdr = CommonNetwork::ReadHeader(std::span<const std::uint8_t>(inbox.data(), CommonNetwork::kHeaderBytes));

                    const std::size_t need = CommonNetwork::kHeaderBytes + hdr.size;

                    if (hdr.size > kMaxPayload)
                    {
                        std::cerr << "ClientNetwork: invalid packet size " << hdr.size << " – dropping connection.\n";
                        StartDisconnectCountdown();

                        return;
                    }

                    if (inbox.size() < need)
                        break;

                    std::vector<std::uint8_t> payload(hdr.size);
                    std::memcpy(payload.data(), inbox.data() + CommonNetwork::kHeaderBytes, hdr.size);

                    inbox.erase(inbox.begin(), inbox.begin() + need);

                    PacketHeader header{};

                    header.type = static_cast<PacketType>(hdr.type);
                    header.size = hdr.size;
                    header.from = hdr.from;
                    header.sequence = hdr.sequence;

                    try
                    {
                        HandlePacket(header, std::move(payload));
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "ClientNetwork: packet handler threw: " << e.what() << '\n';
                    }
                    catch (...)
                    {
                        std::cerr << "ClientNetwork: packet handler threw unknown exception\n";
                    }
                }

                BeginRead();
            }));
        }

        void HandlePacket(const PacketHeader& header, std::vector<std::uint8_t>&& data)
        {
            if (header.type == PacketType::S2C_RequestStringId)
            {
                if (haveNetworkId.load(std::memory_order_relaxed))
                    SendStringIdOnce();
                else
                    pendingStringId.store(true, std::memory_order_relaxed);

                return;
            }

            if (header.type == PacketType::S2C_AssignNetworkId)
            {
                const NetworkId id = std::any_cast<NetworkId>(CommonNetwork::DisassembleData(data)[0]);

                std::cout << "Received NetworkId ('" << id << "') from the server.\n";

                this->networkId = id;
                haveNetworkId.store(true, std::memory_order_relaxed);

                SendStringIdOnce();
                pendingStringId.store(false, std::memory_order_relaxed);

                return;
            }

            if (const auto packet = packetHandlerMap.find(header.type); packet != packetHandlerMap.end())
            {
                for (auto& functionIn : packet->second)
                {
                    auto copy = data;

                    MainThreadExecutor::GetInstance().EnqueueTask(this, [function = functionIn, payload = std::move(copy)]() mutable
                        {
                            try
                            {
                                function(std::move(payload));
                            }
                            catch (const std::exception& e)
                            {
                                std::cerr << "ClientNetwork: receiver for packet threw: " << e.what() << '\n';
                            }
                            catch (...)
                            {
                                std::cerr << "ClientNetwork: receiver for packet threw unknown exception\n";
                            }
                        }
                    );
                }
            }
        }


        void StartDisconnectCountdown()
        {
            if (disconnecting.load(std::memory_order_relaxed))
                return;

            if (disconnectTimerActive.exchange(true))
                return;

            disconnectTimer.expires_after(std::chrono::seconds(2));
            disconnectTimer.async_wait(boost::asio::bind_executor(strand, [this](const ErrorCode& errorCode)
                {
                    if (!errorCode && !disconnecting.load(std::memory_order_relaxed))
                    {
                        MainThreadExecutor::GetInstance().EnqueueTask(this, [&]()
                            {
                                NotifyConnectionLost();
                            });
                    }

                    disconnectTimerActive = false;
                }));
        }

        void CancelDisconnectCountdown()
        {
            if (!disconnectTimerActive)
                return;

            disconnectTimerActive = false;

            disconnectTimer.cancel();
        }

        void SendStringIdOnce()
        {
            bool expected = false;

            if (sentStringId.compare_exchange_strong(expected, true))
                Send(PacketType::C2S_StringId, stringId);
        }

        boost::asio::io_context ioContext;
        TcpProtocol::socket socket{ioContext};
        std::thread ioThread;
        std::atomic<bool> running = false;
        std::atomic<bool> haveNetworkId{ false };
        std::atomic<bool> pendingStringId{ false };
        std::atomic<bool> sentStringId{ false };

        boost::asio::steady_timer disconnectTimer{ ioContext };
        std::atomic<bool> disconnectTimerActive{ false };
        std::atomic<bool> disconnecting{ false };

        std::vector<std::function<void()>> onServerConnectionLostCallbackList;

        std::mutex callbackMutex;

        std::string stringId;
        NetworkId networkId = 0;

        std::array<std::uint8_t, 512> readBuffer = { };
        std::vector<std::uint8_t> inbox;

        boost::asio::strand<boost::asio::io_context::executor_type> strand = boost::asio::make_strand(ioContext);

        std::unordered_map<PacketType, std::vector<std::function<void(std::vector<std::uint8_t>)>>> packetHandlerMap;

        std::deque<std::shared_ptr<std::vector<std::uint8_t>>> writeQueue;
        std::optional<WorkGuard> workGuard;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientNetwork> instance;

    };

    std::once_flag ClientNetwork::initializationFlag;
    std::unique_ptr<ClientNetwork> ClientNetwork::instance;
}
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

                {
                    boost::system::error_code localErrorCode;

                    (void)socket.set_option(TcpProtocol::no_delay(true), localErrorCode);

                    if (localErrorCode)
                        std::cerr << "TCP_NODELAY failed (client): " << localErrorCode.message() << '\n';
                }

#if defined(__APPLE__)
                {
                    constexpr int one = 1;
                    ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
                }
#endif
            }
            catch (const boost::system::system_error& error)
            {
                std::cerr << "Connect failed: " << error.what() << '\n';
                workGuard.reset();

                return;
            }

            socket.set_option(TcpProtocol::no_delay(true));
            ioThread = std::thread([this] { ioContext.run(); });

            BeginRead();

            running = true;
        }

        void RegisterReceiver(const PacketType type, std::function<void(std::vector<std::uint8_t>)> function)
        {
            boost::asio::post(strand, [this, type, receiver = std::move(function)]() mutable
            {
                std::lock_guard guard(packetMapMutex);

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

        struct InboundMessage
        {
            PacketType type;
            std::vector<std::uint8_t> payload;
        };

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
                        std::cerr << "ClientNetwork: invalid packet size " << hdr.size << " � dropping connection.\n";
                        StartDisconnectCountdown();

                        return;
                    }

                    if (inbox.size() < need)
                        break;

                    std::vector<std::uint8_t> payload(hdr.size);
                    std::memcpy(payload.data(), inbox.data() + CommonNetwork::kHeaderBytes, hdr.size);

                    inbox.erase(inbox.begin(), inbox.begin() + need);

                    const auto messageType = static_cast<PacketType>(hdr.type);

                    if (messageType == PacketType::S2C_AssignNetworkId)
                    {
                        auto temporary = payload;
                        const std::span payloadData(temporary.data(), temporary.size());

                        if (const auto parts = CommonNetwork::DisassembleData(payloadData); !parts.empty())
                        {
                            const auto id = std::any_cast<NetworkId>(parts[0]);

                            networkId = id;
                            haveNetworkId.store(true, std::memory_order_relaxed);

                            SendStringIdOnce();
                            pendingStringId.store(false, std::memory_order_relaxed);
                        }
                        else
                            std::cerr << "ClientNetwork: S2C_AssignNetworkId had empty payload\n";

                        BeginRead();

                        return;
                    }

                    if (messageType == PacketType::S2C_RequestStringId)
                    {
                        if (haveNetworkId.load(std::memory_order_relaxed))
                            SendStringIdOnce();
                        else
                            pendingStringId.store(true, std::memory_order_relaxed);

                        BeginRead();
                        return;
                    }

                    EnqueueInbound(messageType, std::move(payload));
                }

                BeginRead();
            }));
        }

        void EnqueueInbound(const PacketType type, std::vector<std::uint8_t>&& payload)
        {
            bool schedule = false;

            {
                std::lock_guard guard(inboundMessageMutex);

                inboundMessageQueue.push_back(InboundMessage{ type, std::move(payload) });

                if (!inboundMessagePumpScheduled)
                {
                    inboundMessagePumpScheduled = true;
                    schedule = true;
                }
            }

            if (schedule)
                MainThreadExecutor::GetInstance().EnqueueTask(this, [this]() { PumpInboundOnMainThread(); });
        }

        void PumpInboundOnMainThread()
        {
            for (;;)
            {
                InboundMessage message;

                {
                    std::lock_guard guard(inboundMessageMutex);

                    if (inboundMessageQueue.empty())
                    {
                        inboundMessagePumpScheduled = false;
                        break;
                    }

                    message = std::move(inboundMessageQueue.front());

                    inboundMessageQueue.pop_front();
                }

                if (message.type == PacketType::S2C_RequestStringId)
                {
                    if (haveNetworkId.load(std::memory_order_relaxed))
                        SendStringIdOnce();
                    else
                        pendingStringId.store(true, std::memory_order_relaxed);

                    continue;
                }

                if (message.type == PacketType::S2C_AssignNetworkId)
                {
                    auto tmp = message.payload;
                    std::span<std::uint8_t> sp(tmp.data(), tmp.size());

                    auto parts = CommonNetwork::DisassembleData(sp);

                    if (!parts.empty())
                    {
                        const NetworkId id = std::any_cast<NetworkId>(parts[0]);

                        networkId = id;
                        haveNetworkId.store(true, std::memory_order_relaxed);

                        SendStringIdOnce();
                        pendingStringId.store(false, std::memory_order_relaxed);
                    }
                    else
                        std::cerr << "ClientNetwork: S2C_AssignNetworkId had empty payload\n";

                    continue;
                }

                std::vector<std::function<void(std::vector<std::uint8_t>)>> handlers;

                {
                    std::lock_guard guard(packetMapMutex);

                    if (auto iterator = packetHandlerMap.find(message.type); iterator != packetHandlerMap.end())
                        handlers = iterator->second;
                }

                for (auto& function : handlers)
                {
                    try
                    {
                        auto copy = message.payload;

                        function(std::move(copy));
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

        std::deque<InboundMessage> inboundMessageQueue;
        bool inboundMessagePumpScheduled = false;

        std::mutex packetMapMutex;
        std::mutex callbackMutex;
        std::mutex inboundMessageMutex;

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
}
#pragma once

#include <memory>
#include <mutex>
#include <iostream>
#include <random>
#include <boost/asio.hpp>
#include "Network/ServerRpc.hpp"
#include "Server/Network/ServerNetwork.hpp"
#include "Server/Network/ServerSynchronization.hpp"

using namespace Blaster::Server::Network;

namespace Blaster::Server
{
    class ServerApplication final
    {

    public:

        ServerApplication(const ServerApplication&) = delete;
        ServerApplication(ServerApplication&&) = delete;
        ServerApplication& operator=(const ServerApplication&) = delete;
        ServerApplication& operator=(ServerApplication&&) = delete;

        void PreInitialize()
        {

        }

        void Initialize()
        {
            std::uint16_t port;

            std::cout << "Enter PORT: ";
            std::cin >> port;

            ServerNetwork::GetInstance().Initialize(port);
            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_StringId, [](const NetworkId who, std::vector<std::uint8_t> data)
                {
                    const std::string name(reinterpret_cast<char*>(data.data()), data.size());

                    std::cout << "Client " << who << " is '" << name << "'\n";

                    ServerSynchronization::SynchronizeFullTree(who);
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rpc, [](const NetworkId who, std::vector<std::uint8_t> pk)
                {
                    ServerRpc::HandleRequest(who, std::move(pk));
                });
        }

        bool IsRunning()
        {
            return ServerNetwork::GetInstance().IsRunning();
        }

        void Update()
        {
            GameObjectManager::GetInstance().Update();
        }

        void Uninitialize()
        {
            ServerNetwork::GetInstance().Uninitialize();
        }

        static ServerApplication& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<ServerApplication>(new ServerApplication());
            });

            return *instance;
        }

    private:

        ServerApplication() = default;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ServerApplication> instance;

    }; 

    std::once_flag ServerApplication::initializationFlag;
    std::unique_ptr<ServerApplication> ServerApplication::instance;
}
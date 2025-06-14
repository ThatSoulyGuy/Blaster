#pragma once

#include <memory>
#include <mutex>
#include <iostream> 
#include <random>
#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Utility/Time.hpp"
//#include "Server/Entity/Entities/EntityPlayer.hpp"
#include "Server/Network/ServerNetwork.hpp"
//#include "Server/Network/ServerRpc.hpp"
//#include "Server/Network/ServerSynchronization.hpp"

//using namespace Blaster::Server::Entity::Entities;
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
                    const std::string name = std::any_cast<std::string>(CommonNetwork::DisassembleData(data)[0]);

                    std::cout << "Client " << who << " is '" << name << "'." << std::endl;

                    ServerNetwork::GetInstance().GetClient(who).value()->stringId = name;

                    //const auto playerObject = ServerSynchronization::SpawnGameObject("player-" + name, who);

                    //ServerSynchronization::AddComponent(playerObject, EntityPlayer::Create());

                    ServerSynchronization::SynchronizeFullTree(who, GameObjectManager::GetInstance().GetAll());
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rpc, [](const NetworkId who, std::vector<std::uint8_t> pk)
                {
                    //ServerRpc::HandleRequest(who, std::move(pk));
                });

            auto myGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("someGameObject"));

            myGameObject->GetTransform()->Translate({ 80.0f, 0.0f, 4.0f });
        }

        bool IsRunning()
        {
            return ServerNetwork::GetInstance().IsRunning();
        }

        void Update()
        {
            GameObjectManager::GetInstance().Update();

            Time::GetInstance().Update();
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
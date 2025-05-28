#pragma once

#include <memory>
#include <mutex>
#include <iostream>
#include <boost/asio.hpp>
#include "Server/Network/ServerNetwork.hpp"
#include "Server/Network/ServerSynchronization.hpp"

using namespace Blaster::Server::Network;

namespace Blaster::Server
{
    class TestComponent : public Component
    {

    public:

        void Update() override
        {
            if (GetGameObject()->IsAuthoritative())
                std::cout << UwU << " from server!" << std::endl;
            else
                std::cout << UwU << " from client!" << std::endl;
        }

        std::string GetTypeName() const override
        {
            return typeid(TestComponent).name();
        }

        int UwU = 320;

        template <class Archive>
        void serialize(Archive& ar, const unsigned)
        {
            ar & boost::serialization::base_object<Component>(*this);

            ar & boost::serialization::make_nvp("UwU", UwU);
        }

    private:

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

    };

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


            auto crate = ServerSynchronization::SpawnGameObject("Crate");
            auto player = ServerSynchronization::SpawnGameObject("Player");

            ServerSynchronization::AddComponent(crate, std::make_shared<TestComponent>());
            ServerSynchronization::AddChild(player, crate);
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
#pragma once

#include <memory>
#include <mutex>
#include <iostream>
#include <random>
#include <boost/asio.hpp>
#include "Server/Network/ServerNetwork.hpp"
#include "Server/Network/ServerSynchronization.hpp"

using namespace Blaster::Server::Network;

namespace Blaster::Server
{
    class TestComponent : public Component
    {

    public:

        void Initialize() override
        {
            if (GetGameObject()->IsAuthoritative())
            {
                std::random_device device;
                std::mt19937 generator{device()};

                std::uniform_int_distribution<int> distribution{0, 255};

                myVar = distribution(generator);
            }
        }

        void Update() override
        {
            if (GetGameObject()->IsAuthoritative())
                std::cout << myVar << " from server!" << std::endl;
            else
                std::cout << myVar << " from client!" << std::endl;
        }

        std::string GetTypeName() const override
        {
            return typeid(TestComponent).name();
        }

        int myVar;

        template <class Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & boost::serialization::make_nvp("myVar", myVar);
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

BOOST_CLASS_EXPORT(Blaster::Server::TestComponent)
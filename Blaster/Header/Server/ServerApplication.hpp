#pragma once

#include <memory>
#include <mutex>
#include <iostream> 
#include <random>
#include "Client/Render/Model.hpp"
#include "Client/Render/TextureFuture.hpp"
#include "Independent/Collider/Colliders/ColliderBox.hpp"
#include "Independent/ECS/Synchronization/ReceiverSynchronization.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Independent/Utility/Time.hpp"
#include "Server/Entity/Entities/EntityPlayer.hpp"
#include "Server/Network/ServerNetwork.hpp"

using namespace Blaster::Server::Entity::Entities;
using namespace Blaster::Independent::ECS::Synchronization;
using namespace Blaster::Independent::Thread;
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
                    const auto name = std::any_cast<std::string>(CommonNetwork::DisassembleData(data)[0]);

                    std::cout << "Client " << who << " is '" << name << "'." << std::endl;

                    ServerNetwork::GetInstance().GetClient(who).value()->stringId = name;

                    const auto player = GameObjectManager::GetInstance().Register(GameObject::Create("player-" + name, false, who));

                    player->AddComponent(EntityPlayer::Create());
                    player->AddComponent(ColliderCapsule::Create(8.0f, 8.0f));
                    player->AddComponent(Rigidbody::Create(true, 8.0f));
                    player->GetComponent<Rigidbody>().value()->LockRotation(Rigidbody::Axis::X | Rigidbody::Axis::Y | Rigidbody::Axis::Z);
                    player->GetTransform()->SetLocalPosition({ 0.0f, 20.0f, 0.0f });

                    SenderSynchronization::SynchronizeFullTree(who, GameObjectManager::GetInstance().GetAll());
                });
            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Snapshot, [](const NetworkId whoIn, std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = messageIn]
                    {
                        ReceiverSynchronization::HandleSnapshotPayload(message);
                    });
                    
                    auto any = CommonNetwork::DisassembleData(messageIn);
                    
                    auto& snapshot = std::any_cast<Snapshot&>(any[0]);

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [snapshot = std::move(snapshot), who = whoIn, message = messageIn]
                    {
                        for (NetworkId id : ServerNetwork::GetInstance().GetConnectedClients())
                        {
                            if (id != who)
                                ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Snapshot, snapshot);
                        }
                    });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_AddForce, [](const NetworkId whoIn, std::vector<std::uint8_t> messageIn)
                {
                    OpRigidbodyOperation operation = std::any_cast<OpRigidbodyOperation>(CommonNetwork::DisassembleData(messageIn)[0]);

                    auto gameObject = GameObjectManager::GetInstance().Get(operation.path);

                    if (gameObject.has_value() && gameObject.value()->HasComponent<Rigidbody>())
                        gameObject.value()->GetComponent<Rigidbody>().value()->AddForce(operation.value);
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_AddImpulse, [](const NetworkId whoIn, std::vector<std::uint8_t> messageIn)
                {
                    OpRigidbodyOperation operation = std::any_cast<OpRigidbodyOperation>(CommonNetwork::DisassembleData(messageIn)[0]);

                    auto gameObject = GameObjectManager::GetInstance().Get(operation.path);

                    if (gameObject.has_value() && gameObject.value()->HasComponent<Rigidbody>())
                        gameObject.value()->GetComponent<Rigidbody>().value()->AddImpulse(operation.value);
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_SetStaticTransform, [](const NetworkId whoIn, std::vector<std::uint8_t> messageIn)
                {
                    OpRigidbodySetTransform operation = std::any_cast<OpRigidbodySetTransform>(CommonNetwork::DisassembleData(messageIn)[0]);

                    auto gameObject = GameObjectManager::GetInstance().Get(operation.path);

                    if (gameObject.has_value() && gameObject.value()->HasComponent<Rigidbody>())
                        gameObject.value()->GetComponent<Rigidbody>().value()->SetStaticTransform(operation.position, operation.rotation);
                });

            const auto crateObject = GameObjectManager::GetInstance().Register(GameObject::Create("crate"));

            crateObject->AddComponent(TextureFuture::Create("blaster.container"));
            crateObject->AddComponent(Model::Create({ "Blaster", "Model/Crate.fbx" }, false));
            crateObject->AddComponent(ColliderBox::Create({ 10, 10, 10 }));
            crateObject->AddComponent(Rigidbody::Create(true, 3.0f));

            crateObject->GetTransform()->SetLocalPosition({ 0.0f, 10.0f, 0.0f });

            const auto platformObject = GameObjectManager::GetInstance().Register(GameObject::Create("platform"));

            platformObject->AddComponent(TextureFuture::Create("blaster.stone"));
            platformObject->AddComponent(Model::Create({ "Blaster", "Model/Platform.fbx" }, true));

            platformObject->GetComponent<Rigidbody>().value()->SetStaticTransform({ 0.0f, -250.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        }

        bool IsRunning()
        {
            return ServerNetwork::GetInstance().IsRunning();
        }

        void Update()
        {
            MainThreadExecutor::GetInstance().Execute();

            GameObjectManager::GetInstance().Update();

            PhysicsWorld::GetInstance().Update();

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
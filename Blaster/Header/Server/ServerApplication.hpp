#pragma once

#include <memory>
#include <mutex>
#include <iostream> 
#include <random>
#include "Client/Render/Model.hpp"
#include "Client/Render/TextureFuture.hpp"
#include "Independent/Physics/Colliders/ColliderBox.hpp"
#include "Independent/Physics/Colliders/ColliderCapsule.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"
#include "Independent/Physics/Rigidbody.hpp"
#include "Independent/ECS/Synchronization/ReceiverSynchronization.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/Test/PhysicsDebugger.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Independent/Utility/Time.hpp"
#include "Server/Entity/Entities/EntityPlayer.hpp"
#include "Server/Network/ServerNetwork.hpp"

using namespace Blaster::Server::Entity::Entities;
using namespace Blaster::Independent::ECS::Synchronization;
using namespace Blaster::Independent::Physics::Colliders;
using namespace Blaster::Independent::Physics;
using namespace Blaster::Independent::Test;
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
#ifdef _WIN32
            PhysicsDebugger::Initialize();
#endif
        }

        void Initialize()
        {
            std::uint16_t port;

            std::cout << "Enter PORT: ";
            std::cin >> port;

            ServerNetwork::GetInstance().Initialize(port);

            ServerNetwork::GetInstance().AddOnClientDisconnectedCallback([&](auto client)
                {
                    for (const auto& gameObjectPath : client->ownedGameObjectList | std::views::keys)
                        GameObjectManager::GetInstance().Unregister(gameObjectPath);
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_StringId, [](const NetworkId who, std::vector<std::uint8_t> data)
                {
                    const auto name = std::any_cast<std::string>(CommonNetwork::DisassembleData(data)[0]);

                    std::cout << "Client " << who << " is '" << name << "'." << std::endl;

                    ServerNetwork::GetInstance().GetClient(who).value()->stringId = name;

                    auto player = GameObjectManager::GetInstance().Register(GameObject::Create("player-" + name, false, who));

                    player->AddComponent(EntityPlayer::Create());
                    player->AddComponent(ColliderCapsule::Create(10.0f, 10.0f));
                    player->AddComponent(Rigidbody::Create(100.0f, Rigidbody::Type::DYNAMIC));
                    player->GetComponent<Rigidbody>().value()->LockRotation(Rigidbody::Axis::X | Rigidbody::Axis::Y | Rigidbody::Axis::Z);

                    SenderSynchronization::GetInstance().SynchronizeFullTree(who, GameObjectManager::GetInstance().GetAll());
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Snapshot, [](const NetworkId whoIn, std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = messageIn]
                    {
                        ReceiverSynchronization::GetInstance().HandleSnapshotPayload(message);
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

            auto validateAndApply = [](NetworkId who, const std::string& path, const std::function<void(std::shared_ptr<Rigidbody>)>& function)
                {
                    auto optionalGameObject = GameObjectManager::GetInstance().Get(path);

                    if (!optionalGameObject)
                        return;

                    auto gameObject = optionalGameObject.value();

                    if (gameObject->GetOwningClient() != who)
                        return;

                    auto rigidbodyOptional = gameObject->GetComponent<Rigidbody>();

                    if (!rigidbodyOptional)
                        return;

                    function(rigidbodyOptional.value());
                };

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_Impulse, [validateAndApply](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto anyList = CommonNetwork::DisassembleData(data);

                    if (anyList.empty())
                        return;

                    auto command = std::any_cast<ImpulseCommand>(anyList[0]);

                    validateAndApply(who, command.path, [&](auto rigidbody)
                        {
                            if (command.hasPoint)
                                rigidbody->ApplyImpulseAtPoint(command.impulse, command.point);
                            else
                                rigidbody->ApplyCentralImpulse(command.impulse);
                        });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_SetTransform, [validateAndApply](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto anyList = CommonNetwork::DisassembleData(data);

                    if (anyList.empty())
                        return;

                    auto command = std::any_cast<SetTransformCommand>(anyList[0]);

                    validateAndApply(who, command.path, [&](auto rigidbody)
                        {
                            if (rigidbody->GetBodyType() == Rigidbody::Type::STATIC)
                            {
                                rigidbody->GetGameObject()->GetTransform()->SetLocalPosition(command.position);
                                rigidbody->GetGameObject()->GetTransform()->SetLocalRotation(command.rotation);

                                rigidbody->PushTransformToPhysics();
                            }
                        });
                });

            PhysicsWorld::GetInstance().Initialize();

            const auto crateObject = GameObjectManager::GetInstance().Register(GameObject::Create("crate"));

            crateObject->AddComponent(TextureFuture::Create("blaster.container"));
            crateObject->AddComponent(Model::Create({ "Blaster", "Model/Crate.fbx" }, false));
            crateObject->AddComponent(ColliderBox::Create({ 20.0f, 20.0f, 20.0f }));
            crateObject->AddComponent(Rigidbody::Create(100.0f, Rigidbody::Type::DYNAMIC));

            crateObject->GetTransform()->SetLocalPosition({ 0.0f, 10.0f, 0.0f });

            const auto platformObject = GameObjectManager::GetInstance().Register(GameObject::Create("platform"));

            platformObject->GetTransform()->SetLocalPosition({ 0.0f, -240.0f, 0.0f });

            platformObject->AddComponent(TextureFuture::Create("blaster.stone"));
            platformObject->AddComponent(Model::Create({ "Blaster", "Model/Platform.fbx" }, true));

            platformObject->GetTransform()->SetLocalPosition({ 0.0f, -240.0f, 0.0f });
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
#ifdef _WIN32
            PhysicsDebugger::Uninitialize();
#endif

            PhysicsWorld::GetInstance().Uninitialize();

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
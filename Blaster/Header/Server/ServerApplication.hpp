#pragma once

#include <memory>
#include <mutex>
#include <iostream> 
#include <random>
#include "Client/Render/Model.hpp"
#include "Client/Render/TextureFuture.hpp"
#include "Independent/Physics/Colliders/ColliderBox.hpp"
#include "Independent/Physics/Colliders/ColliderCapsule.hpp"
#include "Independent/Physics/CharacterController.hpp"
#include "Independent/Physics/PhysicsSystem.hpp"
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
                    std::random_device device;
                    std::mt19937 generator(device());

                    constexpr int min = 1;
                    constexpr int max = 2;

                    std::uniform_int_distribution distribution(min, max);

                    const int randomNumber = distribution(generator);

                    const auto name = std::any_cast<std::string>(CommonNetwork::DisassembleData(data)[0]);

                    std::cout << "Client " << who << " is '" << name << "'." << std::endl;

                    ServerNetwork::GetInstance().GetClient(who).value()->stringId = name;

                    auto player = GameObjectManager::GetInstance().Register(GameObject::Create("player-" + name, false, who));
                    
                    if (randomNumber == 1)
                        player->GetTransform()->SetLocalPosition({ 418.87f, -190.0f, 13.19f });
                    else
                        player->GetTransform()->SetLocalPosition({ -411.66f, -190.0f, 7.50f });
                    
                    player->AddComponent(CharacterController::Create(1.45f, 8.0f));
                    player->AddComponent(EntityPlayer::Create());
                    
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

            auto validateAndApply = [](NetworkId who, const std::string& path, const std::function<void(std::shared_ptr<PhysicsBody>)>& function)
                {
                    auto optionalGameObject = GameObjectManager::GetInstance().Get(path);

                    if (!optionalGameObject)
                        return;

                    auto gameObject = optionalGameObject.value();

                    if (gameObject->GetOwningClient() != who)
                        return;

                    auto rigidbodyOptional = gameObject->GetComponent<PhysicsBody>();

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

                    validateAndApply(who, command.path, [&](auto body)
                        {
                            auto rigidbody = std::static_pointer_cast<Rigidbody>(body);

                            if (command.hasPoint)
                                rigidbody->ApplyImpulseAtPoint(command.impulse, command.point);
                            else
                                rigidbody->ApplyCentralImpulse(command.impulse);
                        });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_SetVelocity, [validateAndApply](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto anyList = CommonNetwork::DisassembleData(data);

                    if (anyList.empty())
                        return;

                    auto command = std::any_cast<SetVelocityCommand>(anyList[0]);

                    validateAndApply(who, command.path, [&](auto body)
                        {
                            auto rigidbody = std::static_pointer_cast<Rigidbody>(body);

                            rigidbody->SetHorizontalVelocity(command.velocity);
                        });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Rigidbody_SetTransform, [validateAndApply](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto anyList = CommonNetwork::DisassembleData(data);

                    if (anyList.empty())
                        return;

                    auto command = std::any_cast<SetTransformCommand>(anyList[0]);

                    validateAndApply(who, command.path, [&](auto body)
                        {
                            auto rigidbody = std::static_pointer_cast<Rigidbody>(body);

                            if (rigidbody->GetBodyType() == Rigidbody::Type::STATIC)
                            {
                                rigidbody->GetGameObject()->GetTransform()->SetLocalPosition(command.position);
                                rigidbody->GetGameObject()->GetTransform()->SetLocalRotation(command.rotation);

                                rigidbody->PushTransformToPhysics();
                            }
                        });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_CharacterController_Input, [validateAndApply](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto command = std::any_cast<CharacterControllerInputCommand>(CommonNetwork::DisassembleData(data)[0]);

                    validateAndApply(who, command.path, [&](auto body)
                        {
                            auto controller = std::static_pointer_cast<CharacterController>(body);

                            controller->SetWalkDirection(command.walkDirection);

                            if (command.wantJump)
                                controller->Jump();
                        });
                });

            PhysicsWorld::GetInstance().Initialize();

            const auto platformObject = GameObjectManager::GetInstance().Register(GameObject::Create("platform"));

            platformObject->GetTransform()->SetLocalPosition({ 0.0f, -240.0f, 0.0f });

            platformObject->AddComponent(Model::Create({ "Blaster", "Model/Map.fbx" }, false, true));

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

            PhysicsSystem::GetInstance().Update();

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
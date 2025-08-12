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

            ServerNetwork::GetInstance().AddOnClientDisconnectedCallback([&](auto clientIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [client = clientIn]
                    {
                        std::vector<std::string> paths;

                        paths.reserve(client->ownedGameObjectList.size());

                        for (const auto& path : client->ownedGameObjectList | std::views::keys)
                            paths.push_back(path);

                        client->ownedGameObjectList.clear();

                        for (auto& path : paths)
                            GameObjectManager::GetInstance().Unregister(path);
                    });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_StringId, [](const NetworkId who, std::vector<std::uint8_t> messageIn)
                {
                    const auto name = std::any_cast<std::string>(CommonNetwork::DisassembleData(messageIn)[0]);

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [who, name]()
                        {
                            std::random_device device;
                            std::mt19937 generator(device());

                            constexpr int min = 1;
                            constexpr int max = 2;

                            std::uniform_int_distribution distribution(min, max);

                            const int randomNumber = distribution(generator);

                            std::cout << "Client " << who << " is '" << name << "'." << std::endl;

                            ServerNetwork::GetInstance().GetClient(who).value()->stringId = name;

                            auto player = GameObjectManager::GetInstance().Register(GameObject::Create("player-" + name, false, who));

                            if (randomNumber == 1)
                            {
                                player->AddComponent(EntityPlayer::Create(EntityBase::Team::RED));

                                player->GetTransform3d()->SetLocalPosition({ 420.0f, -190.0f, 15.0f });
                            }
                            else
                            {
                                player->AddComponent(EntityPlayer::Create(EntityBase::Team::BLUE));

                                player->GetTransform3d()->SetLocalPosition({ -420.0f, -190.0f, 15.0f });
                            }

                            player->AddComponent(CharacterController::Create(1.45f, 18.0f));

                            SenderSynchronization::GetInstance().SynchronizeFullTree(who, GameObjectManager::GetInstance().GetAll());
                        });
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
                                rigidbody->GetGameObject()->GetTransform3d()->SetLocalPosition(command.position);
                                rigidbody->GetGameObject()->GetTransform3d()->SetLocalRotation(command.rotation);

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

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_EntityPlayer_Damage, [](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto anyList = CommonNetwork::DisassembleData(data);

                    if (anyList.empty())
                        return;

                    auto command = std::any_cast<DamageCommand>(anyList[0]);

                    if (command.isDamage)
                        GameObjectManager::GetInstance().Get(command.path).value()->GetComponent<EntityBase>().value()->DealDamage(command.damage);
                    else
                        GameObjectManager::GetInstance().Get(command.path).value()->GetComponent<EntityBase>().value()->HealDamage(command.damage);
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_EntityPlayer_Respawn, [](NetworkId who, std::vector<std::uint8_t> msg)
                {
                    auto command = std::any_cast<RespawnCommand>(CommonNetwork::DisassembleData(msg)[0]);
                    auto gameObjectOptional = GameObjectManager::GetInstance().Get(command.path);

                    if (!gameObjectOptional)
                        return;

                    auto gameObject = gameObjectOptional.value();

                    if (gameObject->GetOwningClient() != who)
                        return;

                    const auto team = gameObject->GetComponent<EntityPlayer>().value()->GetTeam();
                    const Vector<float, 3> spawnPosition = (team == EntityBase::Team::RED) ? Vector<float, 3>{ 420.f, -190.f, 15.f } : Vector<float, 3>{ -420.f, -190.f, 15.f };

                    std::shared_ptr<CharacterController> characterController = gameObject->GetComponent<CharacterController>().value();

                    characterController->SetWalkDirection({ 0.f, 0.f, 0.f });
                    characterController->TeleportTo(spawnPosition);

                    auto entityPlayer = gameObject->GetComponent<EntityPlayer>().value();
                    entityPlayer->SetCurrentHealth(entityPlayer->GetMaximumHealth());

                    SenderSynchronization::GetInstance().MarkDirty(gameObject, typeid(EntityPlayer));

                    for (NetworkId id : ServerNetwork::GetInstance().GetConnectedClients())
                        ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_CorrectTransform, CorrectTransformCommand{ gameObject->GetAbsolutePath(), spawnPosition });
                });


            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_QueryTransform, [](NetworkId who, std::vector<std::uint8_t> msg)
                {
                    auto command = std::any_cast<QueryTransformCommand>(CommonNetwork::DisassembleData(msg)[0]);

                    auto gameObject = GameObjectManager::GetInstance().Get(command.path);

                    if (!gameObject)
                        return;

                    if (gameObject.value()->GetOwningClient() != who)
                        return;

                    const auto serverPosition = gameObject.value()->GetTransform3d()->GetWorldPosition();

                    if (const float delta = Vector<float, 3>::Distance(serverPosition, command.position); delta > 0.5f)
                        ServerNetwork::GetInstance().SendTo(who, PacketType::S2C_CorrectTransform, CorrectTransformCommand{ command.path, serverPosition });
                });

            PhysicsWorld::GetInstance().Initialize();

            const auto platformObject = GameObjectManager::GetInstance().Register(GameObject::Create("platform"));

            platformObject->GetTransform3d()->SetLocalPosition({ 0.0f, -240.0f, 0.0f });

            platformObject->AddComponent(Model::Create({ "Blaster", "Model/Map.fbx" }, false, true));

            platformObject->GetTransform3d()->SetLocalPosition({ 0.0f, -240.0f, 0.0f });


            const auto redTeamBeaconObject = GameObjectManager::GetInstance().Register(GameObject::Create("red_beacon"));

            redTeamBeaconObject->AddComponent(EntityBeacon::Create(EntityBase::Team::RED));
            redTeamBeaconObject->AddComponent(ColliderBox::Create({ 10.0f, 10.0f, 10.0f }));
            redTeamBeaconObject->AddComponent(Rigidbody::Create());


            const auto blueTeamBeaconObject = GameObjectManager::GetInstance().Register(GameObject::Create("blue_beacon"));

            blueTeamBeaconObject->AddComponent(EntityBeacon::Create(EntityBase::Team::BLUE));
            blueTeamBeaconObject->AddComponent(ColliderBox::Create({ 10.0f, 10.0f, 10.0f }));
            blueTeamBeaconObject->AddComponent(Rigidbody::Create());
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
#pragma once

#include <memory>
#include <mutex>
#include <iostream> 
#include <random>
#include "Client/Render/Model.hpp"
#include "Client/Render/TextureFuture.hpp"
#include "Independent/Item/ItemBase.hpp"
#include "Independent/Item/ItemRegistry.hpp"
#include "Independent/Item/ItemWorld.hpp"
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
#include "Server/Command/CommandDefinitions.hpp"
#include "Server/Command/CommandManager.hpp"
#include "Server/Command/CommandNetworking.hpp"
#include "Server/Entity/Entities/EntityPlayer.hpp"
#include "Server/Network/ServerNetwork.hpp"

using namespace Blaster::Server::Entity::Entities;
using namespace Blaster::Independent::ECS::Synchronization;
using namespace Blaster::Independent::Physics::Colliders;
using namespace Blaster::Independent::Physics;
using namespace Blaster::Independent::Test;
using namespace Blaster::Independent::Thread;
using namespace Blaster::Server::Command;
using namespace Blaster::Server::Network;

namespace Blaster::Server
{
    class AsynchronousConsole final
    {

    public:

        AsynchronousConsole(const AsynchronousConsole&) = delete;
        AsynchronousConsole(AsynchronousConsole&&) = delete;
        AsynchronousConsole& operator=(const AsynchronousConsole&) = delete;
        AsynchronousConsole& operator=(AsynchronousConsole&&) = delete;

        using LineCallback = std::function<void(const std::string&)>;

        void AttachPromptToStdout()
        {
            std::lock_guard<std::mutex> guard(mutex);

            if (attached)
                return;

            originalBuffer = std::cout.rdbuf();

            promptBuffer.parent = this;
            promptBuffer.under = originalBuffer;

            std::cout.rdbuf(&promptBuffer);

            attached = true;
        }

        void DetachPromptFromStdout()
        {
            std::lock_guard<std::mutex> guard(mutex);

            if (!attached)
                return;

            std::cout.rdbuf(originalBuffer);
            attached = false;
        }

        void Start(LineCallback onLine)
        {
            bool expected = false;

            if (!running.compare_exchange_strong(expected, true))
                return;

            this->onLine = std::move(onLine);
            inputThread = std::thread([this]
                {
                    PrintPrompt();

                    std::string line;
                    while (running.load(std::memory_order_relaxed))
                    {
                        if (!std::getline(std::cin, line))
                        {
                            std::this_thread::sleep_for(50ms);

                            continue;
                        }

                        if (this->onLine)
                            this->onLine(line);

                        PrintPrompt();
                    }
                });
        }

        void Stop()
        {
            if (!running.exchange(false))
                return;

            if (inputThread.joinable())
                inputThread.join();
        }

        void ReprintPrompt()
        {
            std::lock_guard<std::mutex> guard(promptMutex);

            if (originalBuffer)
            {
                originalBuffer->sputn("> ", 2);
                originalBuffer->pubsync();
            }
        }

        static AsynchronousConsole& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<AsynchronousConsole>(new AsynchronousConsole());
            });

            return *instance;
        }

    private:

        AsynchronousConsole() = default;

        struct PromptBuffer : std::streambuf
        {
            AsynchronousConsole* parent = nullptr;

            std::streambuf* under = nullptr;
            std::mutex writeMutex;

            int_type overflow(int_type ch) override
            {
                if (ch == traits_type::eof())
                    return traits_type::eof();

                std::lock_guard<std::mutex> lk(writeMutex);
                under->sputc(static_cast<char>(ch));

                if (ch == '\n')
                    parent->PrintPromptUnsafe();

                return ch;
            }

            std::streamsize xsputn(const char* s, std::streamsize n) override
            {
                std::lock_guard<std::mutex> lk(writeMutex);

                auto w = under->sputn(s, n);

                if (n > 0 && s[n - 1] == '\n')
                    parent->PrintPromptUnsafe();

                return w;
            }

            int sync() override
            {
                return under->pubsync();
            }

        } promptBuffer;

        void PrintPrompt()
        {
            std::lock_guard<std::mutex> guard(promptMutex);

            if (originalBuffer)
            {
                originalBuffer->sputn("> ", 2);
                originalBuffer->pubsync();
            }
        }

        void PrintPromptUnsafe()
        {
            if (!promptBuffer.under)
                return;

            promptBuffer.under->sputn("> ", 2);
            promptBuffer.under->pubsync();
        }

        std::mutex mutex;
        std::mutex promptMutex;
        std::atomic<bool> running{ false };
        bool attached = false;
        std::thread inputThread;
        LineCallback onLine;

        std::streambuf* originalBuffer = nullptr;

        static std::once_flag initializationFlag;
        static std::unique_ptr<AsynchronousConsole> instance;

    };

    std::once_flag AsynchronousConsole::initializationFlag;
    std::unique_ptr<AsynchronousConsole> AsynchronousConsole::instance;

    class ServerApplication final
    {

    public:

        ServerApplication(const ServerApplication&) = delete;
        ServerApplication(ServerApplication&&) = delete;
        ServerApplication& operator=(const ServerApplication&) = delete;
        ServerApplication& operator=(ServerApplication&&) = delete;

        void PreInitialize()
        {
            CommandManager::GetInstance().Register(CommandSpeak::Create());

#ifdef _WIN32
            PhysicsDebugger::Initialize();
#endif
        }

        void Initialize()
        {
            std::uint16_t port;

            std::cout << "Enter PORT: \n";
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

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_StringId, [this](const NetworkId who, std::vector<std::uint8_t> messageIn)
                {
                    const auto name = std::any_cast<std::string>(CommonNetwork::DisassembleData(messageIn)[0]);

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [this, who, name]()
                        {
                            std::cout << "Client " << who << " is '" << name << "'.\n";
                            ServerNetwork::GetInstance().GetClient(who).value()->stringId = name;

                            auto player = GameObjectManager::GetInstance().Register(GameObject::Create("player-" + name, false, who));

                            const auto team = PopNextTeam();
                            player->AddComponent(EntityPlayer::Create(team));

                            if (team == EntityBase::Team::RED)
                                player->GetTransform3d()->SetLocalPosition({ 420.0f, -190.0f, 15.0f });
                            else
                                player->GetTransform3d()->SetLocalPosition({ -420.0f, -190.0f, 15.0f });

                            player->AddComponent(CharacterController::Create(1.45f, 18.0f));

                            SenderSynchronization::GetInstance().SynchronizeFullTree(who, GameObjectManager::GetInstance().GetAll());

                            BroadcastAnnouncement(name + " joined the game on team " + std::string(team == EntityBase::Team::RED ? "RED" : "BLUE") + ".");
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

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_EntityPlayer_Damage, [&](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto command = std::any_cast<DamageCommand>(CommonNetwork::DisassembleData(data)[0]);

                    auto gameObjectOptional = GameObjectManager::GetInstance().Get(command.path);

                    if (!gameObjectOptional)
                        return;

                    auto gameObject = gameObjectOptional.value();
                    auto entityOptional = gameObject->GetComponent<EntityBase>();

                    if (!entityOptional)
                        return;

                    auto entity = entityOptional.value();
                    const std::uint8_t before = entity->GetCurrentHealth();

                    if (command.isDamage)
                        entity->DealDamage(command.damage);
                    else
                        entity->HealDamage(command.damage);

                    const std::uint8_t after = entity->GetCurrentHealth();

                    if (!gameObject->HasComponent<EntityPlayer>())
                        return;

                    if (before > 0 && after == 0)
                    {
                        std::string victim = "Unknown";

                        if (auto client = ServerNetwork::GetInstance().GetClient(gameObject->GetOwningClient().value()))
                            victim = client.value()->stringId;

                        BroadcastAnnouncement(victim + " was killed!");

                        CheckEliminationAndAnnounce();
                    }
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_EntityPlayer_Respawn, [](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto command = std::any_cast<RespawnCommand>(CommonNetwork::DisassembleData(data)[0]);
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

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_QueryTransform, [](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto command = std::any_cast<QueryTransformCommand>(CommonNetwork::DisassembleData(data)[0]);

                    auto gameObject = GameObjectManager::GetInstance().Get(command.path);

                    if (!gameObject)
                        return;

                    if (gameObject.value()->GetOwningClient() != who)
                        return;

                    const auto serverPosition = gameObject.value()->GetTransform3d()->GetWorldPosition();

                    if (const float delta = Vector<float, 3>::Distance(serverPosition, command.position); delta > 0.5f)
                        ServerNetwork::GetInstance().SendTo(who, PacketType::S2C_CorrectTransform, CorrectTransformCommand{ command.path, serverPosition });
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_Chat, [](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto text = std::any_cast<std::string>(CommonNetwork::DisassembleData(data)[0]);

                    if (text.size() > 256)
                        text.resize(256);

                    std::string name;

                    if (auto client = ServerNetwork::GetInstance().GetClient(who))
                        name = client.value()->stringId;
                    
                    if (name.empty())
                        name = std::to_string(who);

                    std::string line = "[" + name + "]: " + text;

                    std::cout << "[Chat] " << line << std::endl;

                    for (NetworkId id : ServerNetwork::GetInstance().GetConnectedClients())
                        ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Chat, line);
                });

            ServerNetwork::GetInstance().RegisterReceiver(PacketType::C2S_ClientCommand, [this](NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto packet = std::any_cast<CommandPacket>(CommonNetwork::DisassembleData(data)[0]);

                    std::string message;

                    ExecuteCommand(CommandSenderGeneric{ packet.senderName, packet.senderAuthority }, packet.line, message);

                    std::cout << "Command Sender '" << packet.senderName << "' executed command '" << CommandParser::ParseLine(packet.line).name << "'." << std::endl;

                    ServerNetwork::GetInstance().SendTo(who, PacketType::S2C_Chat, message);
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

            std::vector<Vector<float, 3>> assaultRiflePositionList =
            {
                { 325.0f, -230.0f, -55.0f },
                { 325.0f, -230.0f, 75.0f },
                { -325.0f, -230.0f, -55.0f },
                { -325.0f, -230.0f, 75.0f }
            };
            
            for (const auto& position : assaultRiflePositionList)
                SpawnWorldItem(position, ItemRegistry::GetInstance().Get(std::string("item_assault_rifle")).value());
            
            AsynchronousConsole::GetInstance().AttachPromptToStdout();
            AsynchronousConsole::GetInstance().Start([this](const std::string& line)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [this, line]
                        {
                            this->OnConsoleLine(line);
                        });
                });
        }

        bool IsRunning()
        {
            return ServerNetwork::GetInstance().IsRunning();
        }

        void Update()
        {
            if (!isRedBeaconDestroyed && !GameObjectManager::GetInstance().Has("red_beacon") && ServerNetwork::GetInstance().GetConnectedClients().size() != 0)
            {
                BroadcastAnnouncement("The red beacon has been destroyed!");

                isRedBeaconDestroyed = true;
            }

            if (!isBlueBeaconDestroyed && !GameObjectManager::GetInstance().Has("blue_beacon") && ServerNetwork::GetInstance().GetConnectedClients().size() != 0)
            {
                BroadcastAnnouncement("The blue beacon has been destroyed!");

                isBlueBeaconDestroyed = true;
            }

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

            AsynchronousConsole::GetInstance().Stop();
            AsynchronousConsole::GetInstance().DetachPromptFromStdout();
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

        EntityBase::Team nextTeam = EntityBase::Team::RED;

        EntityBase::Team PopNextTeam()
        {
            EntityBase::Team out = nextTeam;

            nextTeam = (nextTeam == EntityBase::Team::RED) ? EntityBase::Team::BLUE : EntityBase::Team::RED;

            return out;
        }

        void BroadcastAnnouncement(const std::string& message)
        {
            for (NetworkId id : ServerNetwork::GetInstance().GetConnectedClients())
                ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_ServerAnnouncement, "[Server]: " + message);
        }

        void CheckEliminationAndAnnounce()
        {
            int redAlive = 0, blueAlive = 0;

            for (const auto& gameObject : GameObjectManager::GetInstance().GetAll())
            {
                if (!gameObject->HasComponent<EntityPlayer>())
                    continue;

                auto player = gameObject->GetComponent<EntityPlayer>().value();

                if (player->GetCurrentHealth() > 0)
                {
                    if (player->GetTeam() == EntityBase::Team::RED)
                        ++redAlive;
                    else if (player->GetTeam() == EntityBase::Team::BLUE)
                        ++blueAlive;
                }
            }

            if (redAlive == 0 && isRedBeaconDestroyed)
                BroadcastAnnouncement("Team RED has been eliminated!");

            if (blueAlive == 0 && isBlueBeaconDestroyed)
                BroadcastAnnouncement("Team BLUE has been eliminated!");
        }

        void OnConsoleLine(std::string line)
        {
            line = TrimCopy(line);

            if (line.empty())
                return;

            if (!line.empty() && line[0] == '/')
            {
                try
                {
                    std::string message;

                    ExecuteCommand(CommandSenderServer(), line, message);

                    std::cout << message << std::endl;
                }
                catch (const std::exception& e)
                {
                    std::cout << "Command arguement error: " << e.what() << "\n";
                }

                return;
            }

            std::cout << "[Server]: " << line << "\n";
        }

        void ExecuteCommand(const CommandSender& sender, const std::string& line, std::string& out)
        {
            CommandDescriptor descriptor = CommandParser::ParseLine(line);

            if (!CommandManager::GetInstance().Has(descriptor.name))
            {
                out = "Command '" + descriptor.name + "' does not exist!";
                return;
            }

            const auto& command = CommandManager::GetInstance().Get(descriptor.name).value();

            if (command->GetRequiredAuthority() > sender.GetAuthority())
            {
                out = "Sender '" + sender.GetSenderName() + "' doesn't have authority level '" + std::to_string((uint8_t)sender.GetAuthority()) + "' required to run command '" + descriptor.name + "'!";
                return;
            }

            if (int code = command->Run(sender, descriptor); code != 0)
            {
                out = "Command '" + descriptor.name + "' failed with exit code '" + std::to_string(code) + "'! Usage: " + command->GetDescription();
                return;
            }

            out = "Command '" + descriptor.name + "' succeeded with no errors";
        }

        void SpawnWorldItem(const Vector<float, 3>& position, const std::shared_ptr<ItemBase>& item)
        {
            auto gameObject = GameObjectManager::GetInstance().Register(GameObject::Create(item->GetRegistryName() + "__world_model_" + std::to_string(rand())));

            gameObject->GetTransform3d()->SetLocalPosition(position);
            gameObject->GetTransform3d()->SetLocalScale({ 2.0f, 2.0f, 2.0f });

            gameObject->AddComponent(Model::Create(item->GetModelPath().value()));
            gameObject->AddComponent(ItemWorld::Create(item->GetId()));
        }

        std::string TrimCopy(const std::string& input)
        {
            const auto a = input.find_first_not_of(" \t\r\n");

            if (a == std::string::npos)
                return {};

            const auto b = input.find_last_not_of(" \t\r\n");

            return input.substr(a, b - a + 1);
        }

        bool isRedBeaconDestroyed = false;
        bool isBlueBeaconDestroyed = false;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ServerApplication> instance;

    };

    std::once_flag ServerApplication::initializationFlag;
    std::unique_ptr<ServerApplication> ServerApplication::instance;
}
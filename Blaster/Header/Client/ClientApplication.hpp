#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <chrono>
#include <ranges>
#include <vector>
#include <spanstream>
#include "Client/Core/Window.hpp"
#include "Client/Core/InputManager.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Model.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Independent/Physics/PhysicsSystem.hpp"
#include "Independent/ECS/Synchronization/ReceiverSynchronization.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Item/ItemRegistry.hpp"
#include "Independent/Test/PhysicsDebugger.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Independent/Utility/Time.hpp"
#include "Server/Entity/Entities/EntityPlayer.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;
using namespace Blaster::Independent::Physics; 
using namespace Blaster::Independent::ECS::Synchronization;
using namespace Blaster::Independent::Item;
using namespace Blaster::Independent::Test;
using namespace Blaster::Independent::Thread;

namespace Blaster::Client
{
    class ClientApplication final
    {

    public:

        ClientApplication(const ClientApplication&) = delete;
        ClientApplication(ClientApplication&&) = delete;
        ClientApplication& operator=(const ClientApplication&) = delete;
        ClientApplication& operator=(ClientApplication&&) = delete;

        void PreInitialize()
        {
            Window::GetInstance().Initialize("Blaster* 1.98.29", { 750, 450 });

            ShaderManager::GetInstance().Register(Shader::Create("blaster.fat", { "Blaster", "Shader/Fat" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.model", { "Blaster", "Shader/Model" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.simple", { "Blaster", "Shader/Simple" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.colored_ui", { "Blaster", "Shader/ColoredUI" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.text_ui", { "Blaster", "Shader/TextUI" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.textured_billboard", { "Blaster", "Shader/TexturedBillboard" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.textured_ui", { "Blaster", "Shader/TexturedUI" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.error", { "Blaster", "Texture/Error.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.player.mtf_lens", { "Blaster", "Texture/Player/MtfLens.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.player.mtf_diffuse_red", { "Blaster", "Texture/Player/MtfDiffuseRed.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.player.mtf_diffuse_blue", { "Blaster", "Texture/Player/MtfDiffuseBlue.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.player.assault_rifle", { "Blaster", "Texture/Player/AssaultRifle.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.map.team_red", { "Blaster", "Texture/Map/TeamRed.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.map.team_blue", { "Blaster", "Texture/Map/TeamBlue.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.map.concrete_floor", { "Blaster", "Texture/Map/ConcreteFloor.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.map.metal_wall", { "Blaster", "Texture/Map/MetalWall.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.map.beacon", { "Blaster", "Texture/Map/Beacon.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.resource.wood", { "Blaster", "Texture/Resource/Wood.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.resource.stone", { "Blaster", "Texture/Resource/Stone.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.container", { "Blaster", "Texture/Container.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.hotbar_background", { "Blaster", "Texture/UI/HotbarBackground.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.slot_background", { "Blaster", "Texture/UI/SlotBackground.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.hotbar_selector", { "Blaster", "Texture/UI/HotbarSelector.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.menu_background", { "Blaster", "Texture/UI/MenuBackground.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.chat_background", { "Blaster", "Texture/UI/ChatBackground.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.death_background", { "Blaster", "Texture/UI/DeathBackground.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.button_default", { "Blaster", "Texture/UI/ButtonDefault.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.button_selected", { "Blaster", "Texture/UI/ButtonSelected.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.crosshair", { "Blaster", "Texture/UI/Crosshair.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.crosshair_kill", { "Blaster", "Texture/UI/CrosshairKill.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.ui.crosshair_friendly_fire", { "Blaster", "Texture/UI/CrosshairFriendlyFire.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.item.resource_wood", { "Blaster", "Texture/Item/ResourceWood.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.item.resource_empty", { "Blaster", "Texture/Item/ResourceEmpty.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.item.weapon_assault_rifle", { "Blaster", "Texture/Item/WeaponAssaultRifle.png" }));

            InputManager::GetInstance().Initialize();

#ifdef _WIN32
            PhysicsDebugger::Initialize();
#endif
        }

        void Initialize()
        {
            std::string ip;
            std::uint16_t port;

            std::cout << "Enter IPv4: ";
            std::cin >> ip;

            std::cout << "Enter PORT: ";
            std::cin >> port;

            std::random_device device;
            std::mt19937 generator(device());

            constexpr int min = 1;
            constexpr int max = 100;

            std::uniform_int_distribution distribution(min, max);

            const int randomNumber = distribution(generator);

            ClientNetwork::GetInstance().Initialize(ip, port, "Player" + std::to_string(randomNumber));

            ClientNetwork::GetInstance().AddOnServerConnectionLostCallback([&]()
                {
                    GameObjectManager::GetInstance().Clear();
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_Snapshot, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = std::move(messageIn)]
                        {
                            ReceiverSynchronization::GetInstance().HandleSnapshotPayload(message);
                        });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_CorrectTransform, [](std::vector<std::uint8_t> msg)
                {
                    auto commandIn = std::any_cast<CorrectTransformCommand>(CommonNetwork::DisassembleData(msg)[0]);

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [command = std::move(commandIn)]
                        {
                            auto gameObjectOptional = GameObjectManager::GetInstance().Get(command.path);

                            if (!gameObjectOptional)
                                return;

                            auto gameObject = gameObjectOptional.value();

                            if (gameObject->HasComponent<PhysicsBody>())
                                gameObject->GetComponent<PhysicsBody>().value()->TeleportTo(command.position);
                            else
                                gameObject->GetTransform3d()->SetLocalPosition(command.position, false);
                        });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_Chat, [this](std::vector<std::uint8_t> payload)
                {
                    auto line = std::any_cast<std::string>(CommonNetwork::DisassembleData(payload)[0]);

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [this, line]()
                        {
                            if (GameObjectManager::GetInstance().GetCamera().has_value())
                                GameObjectManager::GetInstance().GetCamera().value()->GetGameObject()->GetParent().value().lock()->GetComponent<Blaster::Server::Entity::Entities::EntityPlayer>().value()->AppendChatLine(line);
                        });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_ServerAnnouncement, [this](std::vector<std::uint8_t> payload)
                {
                    auto line = std::any_cast<std::string>(CommonNetwork::DisassembleData(payload)[0]);

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [this, line]()
                        {
                            if (GameObjectManager::GetInstance().GetCamera().has_value())
                                GameObjectManager::GetInstance().GetCamera().value()->GetGameObject()->GetParent().value().lock()->GetComponent<Blaster::Server::Entity::Entities::EntityPlayer>().value()->AppendChatLine(line);
                        });
                });
            
            PhysicsWorld::GetInstance().Initialize();
        }

        bool IsRunning()
        {
            return Window::GetInstance().IsRunning();
        }

        void Update()
        {
            MainThreadExecutor::GetInstance().Execute();

            GameObjectManager::GetInstance().Update();

            PhysicsSystem::GetInstance().Update();

            TranslationBuffer::GetInstance().Update();

            Time::GetInstance().Update();
        }

        void Render()
        {
            Window::Clear();

            GameObjectManager::GetInstance().Render(GameObjectManager::GetInstance().GetCamera());
            GameObjectManager::GetInstance().RenderUI();

            if (GameObjectManager::GetInstance().GetCamera().has_value())
                PhysicsWorld::GetInstance().Render(*GameObjectManager::GetInstance().GetCamera());

            Window::GetInstance().Present();

            InputManager::GetInstance().Update();
        }

        void Uninitialize()
        {
            ClientNetwork::GetInstance().Uninitialize();

#ifdef _WIN32
            PhysicsDebugger::Uninitialize();
#endif
            PhysicsWorld::GetInstance().Uninitialize();
        }

        static ClientApplication& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<ClientApplication>(new ClientApplication());
            });

            return *instance;
        }

    private:

        ClientApplication() = default;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientApplication> instance;

    };

    std::once_flag ClientApplication::initializationFlag;
    std::unique_ptr<ClientApplication> ClientApplication::instance;
}
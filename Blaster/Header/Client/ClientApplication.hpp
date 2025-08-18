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
#include "Client/UI/Elements/UIElementButton.hpp"
#include "Client/UI/Elements/UIElementImage.hpp"
#include "Client/UI/Elements/UIElementText.hpp"
#include "Client/UI/Elements/UIElementTextField.hpp"
#include "Client/UI/Layouts/UILayoutGrid.hpp"
#include "Client/UI/Layouts/UILayoutStack.hpp"
#include "Client/UI/UIBuilder.hpp"
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
using namespace Blaster::Client::UI::Elements;
using namespace Blaster::Client::UI::Layouts;
using namespace Blaster::Client::UI;
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
            TextureManager::GetInstance().Register(Texture::Create("blaster.player.first_aid", { "Blaster", "Texture/Player/FirstAid.png" }));
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
            TextureManager::GetInstance().Register(Texture::Create("blaster.item.resource_medkit", { "Blaster", "Texture/Item/ResourceMedkit.png" }));

            InputManager::GetInstance().Initialize();

#ifdef _WIN32
            PhysicsDebugger::Initialize();
#endif
        }

        void Initialize()
        {
            RegisterNetworkHandlersOnce();

            if (!reconnectCallbackRegisteredOnce)
            {
                ClientNetwork::GetInstance().AddOnServerConnectionLostCallback([this]()
                    {
                        GameObjectManager::GetInstance().Clear();
                        SenderSynchronization::GetInstance().Reset();
                        SyncTracker::GetInstance().Reset();
                        InputManager::GetInstance().Reset();

                        awaitingReconnectPrompt = true;
                    });

                reconnectCallbackRegisteredOnce = true;
            }
            
            PhysicsWorld::GetInstance().Initialize();

            if (!ClientNetwork::GetInstance().IsRunning())
                EnsureConnectMenu();
        }

        bool IsRunning()
        {
            return Window::GetInstance().IsRunning();
        }

        void Update()
        {
            if (!ClientNetwork::GetInstance().IsRunning())
            {
                EnsureConnectMenu();

                InputManager::GetInstance().SetMouseMode(MouseMode::FREE);
            }
            else
            {
                if (connectMenuRoot)
                    DestroyConnectMenu();
            }

            MainThreadExecutor::GetInstance().Execute();

            if (awaitingReconnectPrompt)
            {
                EnsureConnectMenu();

                awaitingReconnectPrompt = false;
            }

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

        void RegisterNetworkHandlersOnce()
        {
            if (handlersRegisteredOnce)
                return;

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

            handlersRegisteredOnce = true;
        }

        void BuildConnectMenu()
        {
            if (connectMenuRoot && GameObjectManager::GetInstance().Has(connectMenuRoot->GetAbsolutePath()))
                return;

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(1, 100);

            const std::string defaultName = "Player" + std::to_string(dist(gen));

            connectMenuRoot = UIBuilder::NewMenu("ui_connect_menu")
                .AddElement<UIElementImage>("ui_background")
                    .CallAndThen<&Component::GetGameObject>([&](std::shared_ptr<GameObject> gameObject)
                    {
                        gameObject->GetTransform2d()->SetStretch(Transform2d::Stretch::TOP | Transform2d::Stretch::BOTTOM | Transform2d::Stretch::RIGHT | Transform2d::Stretch::LEFT);
                    })
                    .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.menu_background").value())
                    .Call<&UIElementImage::Generate>()
                .MoveDown()
                .AddElement<UIElementImage>("ui_panel")
                    .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.menu_background").value())
                    .Call<&UIElementImage::Generate>()
                    .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                    {
                        gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::CENTER_X | Transform2d::Anchor::CENTER_Y);
                        gameObject->GetTransform2d()->SetDimensions({ 640.0f, 480.0f });
                    })
                    .AddElement<UIElementText>("ui_title")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::CENTER_X);
                            gameObject->GetTransform2d()->SetPosition({ 0.0f, 18.0f });
                        })
                        .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 64, 0, 8))
                        .Call<&UIElementText::SetText>("BLASTER")
                        .Call<&UIElementText::Generate>()
                    .MoveDown()
                    .AddElement<UIElementText>("ui_ip_label")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::LEFT | Transform2d::Anchor::TOP);
                            gameObject->GetTransform2d()->SetPosition({ 24.0f, 86.0f });
                        })
                        .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 32, 0, 4))
                        .Call<&UIElementText::SetText>("IPv4")
                        .Call<&UIElementText::Generate>()
                    .MoveDown()
                    .AddElement<UIElementTextField>("ui_ip_field")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::CENTER_X);
                            gameObject->GetTransform2d()->SetPosition({ 0.0f, 116.0f });
                            gameObject->GetTransform2d()->SetDimensions({ 520.0f, 36.0f });
                        })
                        .Call<&UIElementTextField::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 28, 0, 4))
                        .Call<&UIElementTextField::SetPlaceholder>("e.g. 127.0.0.1")
                        .Call<&UIElementTextField::SetText>(lastIp)
                        .Call<&UIElementTextField::SetSubmitOnEnter>(false)
                        .Call<&UIElementTextField::Generate>()
                    .MoveDown()
                    .AddElement<UIElementText>("ui_port_label")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::LEFT | Transform2d::Anchor::TOP);
                            gameObject->GetTransform2d()->SetPosition({ 24.0f, 164.0f });
                        })
                        .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 32, 0, 4))
                        .Call<&UIElementText::SetText>("Port")
                        .Call<&UIElementText::Generate>()
                    .MoveDown()
                    .AddElement<UIElementTextField>("ui_port_field")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::CENTER_X);
                            gameObject->GetTransform2d()->SetPosition({ 0.0f, 194.0f });
                            gameObject->GetTransform2d()->SetDimensions({ 520.0f, 36.0f });
                        })
                        .Call<&UIElementTextField::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 28, 0, 4))
                        .Call<&UIElementTextField::SetPlaceholder>("e.g. 7777")
                        .Call<&UIElementTextField::SetText>(std::to_string(lastPort))
                        .Call<&UIElementTextField::SetSubmitOnEnter>(false)
                        .Call<&UIElementTextField::Generate>()
                    .MoveDown()
                    .AddElement<UIElementText>("ui_name_label")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::LEFT | Transform2d::Anchor::TOP);
                            gameObject->GetTransform2d()->SetPosition({ 24.0f, 242.0f });
                        })
                        .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 32, 0, 4))
                        .Call<&UIElementText::SetText>("Name")
                        .Call<&UIElementText::Generate>()
                    .MoveDown()
                    .AddElement<UIElementTextField>("ui_name_field")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::TOP | Transform2d::Anchor::CENTER_X);
                            gameObject->GetTransform2d()->SetPosition({ 0.0f, 272.0f });
                            gameObject->GetTransform2d()->SetDimensions({ 520.0f, 36.0f });
                        })
                        .Call<&UIElementTextField::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 28, 0, 4))
                        .Call<&UIElementTextField::SetPlaceholder>("Player name")
                        .Call<&UIElementTextField::SetText>(defaultName)
                        .Call<&UIElementTextField::SetSubmitOnEnter>(false)
                        .Call<&UIElementTextField::Generate>()
                    .MoveDown()
                    .AddElement<UIElementText>("ui_error")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::BOTTOM | Transform2d::Anchor::CENTER_X);
                            gameObject->GetTransform2d()->SetPosition({ 0.0f, -78.0f });
                        })
                        .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 24, 0, 4))
                        .Call<&UIElementText::SetTint>(Vector<float, 3>{ 1.0f, 0.35f, 0.35f })
                        .Call<&UIElementText::SetText>("")
                        .Call<&UIElementText::Generate>()
                    .MoveDown()
                    .AddElement<UIElementButton>("ui_connect_button")
                        .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                        {
                            gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::BOTTOM | Transform2d::Anchor::CENTER_X);
                            gameObject->GetTransform2d()->SetPosition({ 0.0f, -20.0f });
                            gameObject->GetTransform2d()->SetDimensions({ 260.0f, 54.0f });
                        })
                        .Call<&UIElementButton::SetOnClick>([this] { AttemptConnect(); })
                        .AddElement<UIElementImage>("ui_image")
                            .Call<&UIElementImage::SetTexture>(TextureManager::GetInstance().Get("blaster.ui.button_default").value())
                            .Call<&UIElementImage::Generate>()
                        .MoveDown()
                        .AddElement<UIElementText>("ui_text")
                            .CallAndThen<&Component::GetGameObject>([](std::shared_ptr<GameObject> gameObject)
                            {
                                gameObject->GetTransform2d()->SetAnchors(Transform2d::Anchor::CENTER_X | Transform2d::Anchor::CENTER_Y);
                            })
                            .Call<&UIElementText::SetFont>(UIElementText::Font::Create({ "Blaster", "Font/DS-DIGIB.TTF" }, 36, 0, 6))
                            .Call<&UIElementText::SetText>("CONNECT")
                            .Call<&UIElementText::Generate>()
                        .MoveDown()
                    .MoveDown()
                .Finish();

            const auto base = connectMenuRoot->GetAbsolutePath();

            ipInput = GameObjectManager::GetInstance().Get(base + ".ui_ip_field").value()->GetComponent<UIElementTextField>().value();
            portInput = GameObjectManager::GetInstance().Get(base + ".ui_port_field").value()->GetComponent<UIElementTextField>().value();
            nameInput = GameObjectManager::GetInstance().Get(base + ".ui_name_field").value()->GetComponent<UIElementTextField>().value();
            errorText = GameObjectManager::GetInstance().Get(base + ".ui_error").value()->GetComponent<UIElementText>().value();

            InputManager::GetInstance().SetMouseMode(MouseMode::FREE);
        }

        void DestroyConnectMenu()
        {
            if (ipInput)
                ipInput->SetFocused(false);

            if (portInput)
                portInput->SetFocused(false);

            if (nameInput)
                nameInput->SetFocused(false);
            
            GameObjectManager::GetInstance().Unregister(connectMenuRoot->GetParent()->lock()->GetAbsolutePath());
            
            connectMenuRoot.reset();

            ipInput.reset();
            portInput.reset();
            nameInput.reset();
            errorText.reset();

            (void)InputManager::GetInstance().ConsumeTextInput();
        }

        void EnsureConnectMenu()
        {
            if (!connectMenuRoot)
                BuildConnectMenu();
        }

        void ShowConnectError(const std::string& msg)
        {
            if (!errorText)
                return;

            errorText->SetText(msg);
            errorText->Generate();
        }

        void AttemptConnect()
        {
            if (!ipInput || !portInput)
                return;

            const std::string ip = ipInput->GetText();
            const std::string portStr = portInput->GetText();
            std::string name = nameInput ? nameInput->GetText() : std::string{};

            int portNum = 0;

            try
            {
                portNum = std::stoi(portStr);
            }
            catch (...)
            {
                ShowConnectError("Invalid port.");
                return;
            }

            if (portNum < 1 || portNum > 65535)
            {
                ShowConnectError("Port must be 1..65535.");
                return;
            }

            if (name.empty())
                name = "Player";

            lastIp = ip;
            lastPort = static_cast<std::uint16_t>(portNum);

            ClientNetwork::GetInstance().Initialize(ip, static_cast<std::uint16_t>(portNum), name);

            if (!ClientNetwork::GetInstance().IsRunning())
            {
                ShowConnectError("Connection failed.");
                return;
            }

            DestroyConnectMenu();
        }

        bool awaitingReconnectPrompt = false;
        bool handlersRegisteredOnce = false;
        bool reconnectCallbackRegisteredOnce = false;

        std::shared_ptr<GameObject> connectMenuRoot = nullptr;
        std::shared_ptr<UIElementTextField> ipInput = nullptr;
        std::shared_ptr<UIElementTextField> portInput = nullptr;
        std::shared_ptr<UIElementTextField> nameInput = nullptr;
        std::shared_ptr<UIElementText> errorText = nullptr;

        std::string lastIp = "127.0.0.1";
        std::uint16_t lastPort = 7777;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientApplication> instance;

    };

    std::once_flag ClientApplication::initializationFlag;
    std::unique_ptr<ClientApplication> ClientApplication::instance;
}
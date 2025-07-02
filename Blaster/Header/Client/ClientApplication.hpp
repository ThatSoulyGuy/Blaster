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
//#include "Client/Network/TranslationBuffer.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Model.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Independent/ECS/Synchronization/ReceiverSynchronization.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/NetworkSerialize.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"
#include "Independent/Utility/Time.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Render;
using namespace Blaster::Independent::ECS::Synchronization;
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
            Window::GetInstance().Initialize("Blaster* 1.23.18", { 750, 450 });

            ShaderManager::GetInstance().Register(Shader::Create("blaster.fat", { "Blaster", "Shader/Fat" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.model", { "Blaster", "Shader/Model" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.simple", { "Blaster", "Shader/Simple" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.wood", { "Blaster", "Texture/Wood.png" }));
            TextureManager::GetInstance().Register(Texture::Create("blaster.stone", { "Blaster", "Texture/Stone.png" }));

            InputManager::GetInstance().Initialize();
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

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_Snapshot, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = std::move(messageIn)]
                        {
                            ReceiverSynchronization::HandleSnapshotPayload(message);
                        });
                });

            std::thread([]() mutable
            {
                std::this_thread::sleep_for(2s);

                if (!GameObjectManager::GetInstance().Has("mod"))
                {
                    auto gameObject = GameObjectManager::GetInstance().Register(GameObject::Create("mod"));

                    gameObject->AddComponent(TextureManager::GetInstance().Get("blaster.stone").value());
                    gameObject->AddComponent(Model::Create({ "Blaster", "Model/Test.fbx" }));
                }
            }).detach();
            
            std::thread([this]() mutable
            {
                std::this_thread::sleep_for(4s);

                const auto optionalPlayer = GameObjectManager::GetInstance().Get("player-" + ClientNetwork::GetInstance().GetStringId());

                if (!optionalPlayer.has_value())
                {
                    std::cerr << "Failed to find player game object!" << std::endl;
                    return;
                }

                const auto& player = optionalPlayer.value();

                std::this_thread::sleep_for(20ms);

                const auto optionalCamera = GameObjectManager::GetInstance().Get(player->GetAbsolutePath() + ".camera");

                if (!optionalCamera.has_value())
                {
                    std::cerr << "Failed to find player's camera game object!" << std::endl;
                    return;
                }

                const auto& camera = optionalCamera.value();

                if (!camera->HasComponent<Camera>())
                {
                    std::cerr << "Failed to find player's camera's camera component!" << std::endl;
                    return;
                }

                this->camera = camera->GetComponent<Camera>();
            }).detach();
        }

        bool IsRunning()
        {
            return Window::GetInstance().IsRunning();
        }

        void Update()
        {
            MainThreadExecutor::GetInstance().Execute();

            GameObjectManager::GetInstance().Update();

            Time::GetInstance().Update();

            //TranslationBuffer::GetInstance().Update();
        }

        void Render()
        {
            Window::Clear();

            GameObjectManager::GetInstance().Render(camera);

            Window::GetInstance().Present();

            InputManager::GetInstance().Update();
        }

        void Uninitialize()
        {
            ClientNetwork::GetInstance().Uninitialize();
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

        std::optional<std::shared_ptr<Camera>> camera;

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientApplication> instance;

    };

    std::once_flag ClientApplication::initializationFlag;
    std::unique_ptr<ClientApplication> ClientApplication::instance;
}
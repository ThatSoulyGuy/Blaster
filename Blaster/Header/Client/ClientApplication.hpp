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

                if (!GameObjectManager::GetInstance().Has("mesh"))
                {
                    auto gameObject = GameObjectManager::GetInstance().Register(GameObject::Create("mesh"));

                    gameObject->AddComponent(ShaderManager::GetInstance().Get("blaster.simple").value());

                    auto mesh = gameObject->AddComponent(Mesh<FatVertex>::Create(
                    {
                        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f } },
                        { {  0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f } },
                        { {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }, {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f } },
                        { { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f }, { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f } },
                        { { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f } },
                        { {  0.5f, -0.5f,  0.5f }, { 0.0f, 1.0f, 1.0f }, {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f } },
                        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f }, {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f } },
                        { { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 0.0f }, { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f } }
                    },
                    {
                        0, 1, 2,
                        0, 2, 3,
                        4, 6, 5,
                        4, 7, 6,
                        4, 0, 3,
                        4, 3, 7,
                        1, 5, 6,
                        1, 6, 2,
                        4, 5, 1,
                        4, 1, 0,
                        3, 2, 6,
                        3, 6, 7
                    }));

                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [mesh]
                    {
                        mesh->Generate();
                    });
                }
            }).detach();
            
            /*
            if (!GameObjectManager::GetInstance().Has("mesh"))
            {
                auto meshGameObjectFuture = ClientRpc::CreateGameObject("mesh");

                std::this_thread::sleep_for(20ms);

                std::thread([future = std::move(meshGameObjectFuture), this]() mutable
                {
                    if (const auto gameObject = future.get())
                    {
                        ClientRpc::AddComponent(gameObject->GetName(), Model::Create({ "Blaster", "Model/Test.fbx" }));

                        std::this_thread::sleep_for(20ms);

                        ClientRpc::AddComponent(gameObject->GetName(), TextureManager::GetInstance().Get("blaster.stone").value());
                    }
                }).detach();
            }

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

                const auto optionalCamera = player->GetChild("camera");

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
            }).detach();*/
        }

        bool IsRunning()
        {
            return Window::GetInstance().IsRunning();
        }

        void Update()
        {
            GameObjectManager::GetInstance().Update();

            MainThreadExecutor::GetInstance().Execute();

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
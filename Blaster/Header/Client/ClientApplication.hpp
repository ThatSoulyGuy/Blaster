#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <boost/asio.hpp>
#include "Client/Core/Window.hpp"
#include "Client/Core/InputManager.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Client/Network/ClientRpc.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/NetworkSerialize.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render;
using namespace Blaster::Client::Render::Vertices;

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
            Window::GetInstance().Initialize("Blaster* 1.10.9", { 750, 450 });

            ShaderManager::GetInstance().Register(Shader::Create("blaster.fat", { "Blaster", "Shader/Fat" }));

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

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_Rpc, [](std::vector<std::uint8_t> pk)
            {
                ClientRpc::HandleReply(std::move(pk));
            });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_CreateGameObject, [](std::vector<std::uint8_t> message)
                {
                    const auto iterator = std::ranges::find(message, '\0');

                    if (iterator == message.end())
                        return;

                    const std::string name(message.begin(), iterator);
                    const std::vector blob(iterator + 1, message.end());

                    const auto gameObject = GameObject::Create(name);

                    std::shared_ptr<Transform> transform;

                    NetworkSerialize::ObjectFromBytes(blob, transform);

                    gameObject->GetTransform().swap(transform);

                    if (GameObjectManager::GetInstance().Has(name))
                        GameObjectManager::GetInstance().Unregister(name);

                    GameObjectManager::GetInstance().Register(gameObject);

                    ClientRpc::OnGameObjectReplicated(gameObject->GetName());
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_DestroyGameObject, [](std::vector<std::uint8_t> message)
                {
                    const std::string name(message.begin(), message.end());
                    GameObjectManager::GetInstance().Unregister(name);
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_AddComponent, [](std::vector<std::uint8_t> message)
                {
                    const auto first = std::ranges::find(message, '\0');

                    if (first == message.end())
                        return;

                    const auto second = std::find(first + 1, message.end(), '\0');

                    if (second == message.end())
                        return;

                    const std::string gameObjectName(message.begin(), first);
                    const std::string componentType(first + 1, second);

                    const auto optionalGameObject = GameObjectManager::GetInstance().Get(gameObjectName);

                    if (!optionalGameObject)
                        return;

                    const auto raw = ComponentFactory::Instantiate(componentType);

                    if (!raw)
                        return;

                    auto component = std::static_pointer_cast<Component>(raw);

                    const std::vector blob(second + 1, message.end());

                    NetworkSerialize::ObjectFromBytes(blob, component);

                    (*optionalGameObject)->AddComponentDynamic(component);
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_RemoveComponent, [](std::vector<std::uint8_t> msg)
                {
                    const auto nul = std::ranges::find(msg, '\0');

                    if (nul == msg.end())
                        return;

                    const std::string goName(msg.begin(), nul);

                    const std::string componentType(nul + 1, msg.end());

                    const auto optionalGameObject = GameObjectManager::GetInstance().Get(goName);

                    if (!optionalGameObject)
                        return;

                    (*optionalGameObject)->RemoveComponentDynamic(componentType);
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_AddChild, [](std::vector<std::uint8_t> message)
                {
                    const auto iterator = std::ranges::find(message, '\0');

                    if (iterator == message.end())
                        return;

                    const std::string parentName(message.begin(), iterator);
                    const std::string childName(iterator + 1, message.end());

                    const auto parentGameObject = GameObjectManager::GetInstance().Get(parentName);

                    if (const auto childGameObject = GameObjectManager::GetInstance().Get(childName); parentGameObject && childGameObject)
                        (*parentGameObject)->AddChild(*childGameObject);
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_RemoveChild, [](std::vector<std::uint8_t> message)
                {
                    const auto iterator = std::ranges::find(message, '\0');

                    if (iterator == message.end())
                        return;

                    const std::string parentName(message.begin(), iterator);
                    const std::string childName(iterator + 1, message.end());

                    if (const auto parentGameObject = GameObjectManager::GetInstance().Get(parentName))
                        (*parentGameObject)->RemoveChild(childName);
                });


            auto meshGameObjectFuture = ClientRpc::CreateGameObject("mesh");

            std::thread([future = std::move(meshGameObjectFuture)]() mutable
            {
                if (const auto gameObject = future.get())
                {
                    ClientRpc::AddComponent(gameObject->GetName(), ShaderManager::GetInstance().Get("blaster.fat").value());

                    auto meshFuture = ClientRpc::AddComponent(
                            gameObject->GetName(),
                            Mesh<FatVertex>::Create(
                            {
                                { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
                                { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
                                { { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
                                { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } }
                            },
                            {
                                0, 1, 2,
                                2, 1, 3
                            }));

                    if (const auto mesh = std::static_pointer_cast<Mesh<FatVertex>>(meshFuture.get()))
                        mesh->Generate();
                }
            }).detach();


            auto playerGameObjectFuture = ClientRpc::CreateGameObject("player");

            std::thread([this, playerFuture = std::move(playerGameObjectFuture)]() mutable
            {
                if (const auto player = playerFuture.get())
                {
                    auto cameraFuture = ClientRpc::AddComponent(player->GetName(), Camera::Create(45.f, 0.01f, 1000.f));

                    if (auto cameraComponent = std::static_pointer_cast<Camera>(cameraFuture.get()))
                        camera = std::make_optional(cameraComponent);

                    ClientRpc::TranslateTo(player->GetName(), { 0.0f, 0.0f, -5.0f }, 2.0f);
                }
            }).detach();
        }

        bool IsRunning()
        {
            return Window::GetInstance().IsRunning();
        }

        void Update()
        {
            GameObjectManager::GetInstance().Update();
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
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
#include "Client/Network/TranslationBuffer.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Client/Network/ClientRpc.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Model.hpp"
#include "Client/Render/Vertices/FatVertex.hpp"
#include "Client/Thread/MainThreadExecutor.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/NetworkSerialize.hpp"
#include "Independent/Utility/Time.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;
using namespace Blaster::Client::Render;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Client::Thread;

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
            Window::GetInstance().Initialize("Blaster* 1.16.12", { 750, 450 });

            ShaderManager::GetInstance().Register(Shader::Create("blaster.fat", { "Blaster", "Shader/Fat" }));
            ShaderManager::GetInstance().Register(Shader::Create("blaster.model", { "Blaster", "Shader/Model" }));
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

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_Rpc, [](std::vector<std::uint8_t> pk)
            {
                ClientRpc::HandleReply(std::move(pk));
            });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_CreateGameObject, [](std::vector<std::uint8_t> msg)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [msg = std::move(msg)]
                    {
                        auto nul = std::ranges::find(msg, '\0');

                        if (nul == msg.end() || std::distance(nul, msg.end()) < 5)
                            return;

                        const std::string fullPath(msg.begin(), nul);

                        std::uint32_t ownerId {};
                        std::memcpy(&ownerId, &*(nul + 1), 4);

                        const std::vector blob(nul + 1 + 4, msg.end());

                        auto toStringView = [](auto &&sub)
                        {
                            return std::string_view(&*sub.begin(),std::ranges::distance(sub));
                        };

                        auto segmentView = fullPath | std::views::split('.') | std::views::filter([](auto s){ return !s.empty(); }) | std::views::transform(toStringView);

                        std::shared_ptr<GameObject> parent {};
                        std::string currentPath;

                        for (auto segment : segmentView)
                        {
                            if (!currentPath.empty()) currentPath += '.';
                            currentPath.append(segment);

                            std::shared_ptr<GameObject> node;

                            if (auto opt = GameObjectManager::GetInstance().Get(currentPath))
                                node = *opt;
                            else
                            {
                                node = GameObject::Create(currentPath);
                                GameObjectManager::GetInstance().Register(node);
                            }

                            if (parent && !parent->HasChild(node->GetName()))
                                parent->AddChild(node);

                            parent = node;
                        }

                        if (!parent)
                            return;

                        if (ownerId != 4096)
                            parent->SetOwningClient(ownerId);

                        std::shared_ptr<Transform> transform;

                        NetworkSerialize::ObjectFromBytes(blob, transform);

                        if (transform)
                            parent->GetTransform().swap(transform);
                    });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_DestroyGameObject, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [msg = std::move(messageIn)]
                        {
                            const std::string fullPath(msg.begin(), msg.end());

                            if (fullPath.empty())
                                return;

                            auto toStringView = [](auto&& sub)
                            {
                                return std::string_view(&*sub.begin(), std::ranges::distance(sub));
                            };

                            auto view = fullPath | std::views::split('.') | std::views::filter([](auto s) { return !s.empty(); }) | std::views::transform(toStringView);

                            const std::vector<std::string_view> segments{ view.begin(), view.end() };

                            if (segments.empty())
                                return;

                            if (segments.size() == 1)
                            {
                                GameObjectManager::GetInstance().Unregister(std::string{segments.front()});

                                return;
                            }

                            auto currentOpt = GameObjectManager::GetInstance().Get(std::string{segments.front()});

                            if (!currentOpt)
                                return;

                            auto current = *currentOpt;

                            for (std::size_t i = 1; i + 1 < segments.size(); ++i)
                            {
                                auto childOpt = current->GetChild(std::string{segments[i]});

                                if (!childOpt)
                                    return;

                                current = *childOpt;
                            }

                            current->RemoveChild(std::string{segments.back()});
                        });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_AddComponent, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = std::move(messageIn)]
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
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_RemoveComponent, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = std::move(messageIn)]
                    {
                        const auto nul = std::ranges::find(message, '\0');

                        if (nul == message.end())
                            return;

                        const std::string goName(message.begin(), nul);

                        const std::string componentType(nul + 1, message.end());

                        const auto optionalGameObject = GameObjectManager::GetInstance().Get(goName);

                        if (!optionalGameObject)
                            return;

                        (*optionalGameObject)->RemoveComponentDynamic(componentType);
                    });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_AddChild, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = std::move(messageIn)]
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
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_RemoveChild, [](std::vector<std::uint8_t> bytes)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [data = std::move(bytes)]
                        {
                            auto nul = std::ranges::find(data, '\0');

                            if (nul == data.begin() || nul == data.end() || std::next(nul) == data.end())
                                return;

                            const std::string parentName(data.begin(), nul);
                            const std::string childName(std::next(nul), data.end());

                            if (const auto parentGameObject = GameObjectManager::GetInstance().Get(parentName))
                                (*parentGameObject)->RemoveChild(childName);
                        });
                });

            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_TranslateTo, [](std::vector<std::uint8_t> messageIn)
                {
                    MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [message = std::move(messageIn)]
                    {
                        auto nul = std::ranges::find(message,'\0');

                        if (nul == message.end())
                            return;

                        const std::string path(message.begin(), nul);

                        auto cursor = nul + 1;

                        auto readBlob = [&](auto& dst)
                        {
                            if (cursor + 4 > message.end())
                                return;

                            std::uint32_t len;
                            std::memcpy(&len,&*cursor,4);
                            cursor += 4;

                            if (cursor + len > message.end())
                                return;

                            NetworkSerialize::ObjectFromBytes({cursor, cursor + len}, dst);
                            cursor += len;
                        };

                        Vector<float, 3> target{};

                        float seconds = 0.0f;

                        readBlob(target);
                        readBlob(seconds);

                        TranslationBuffer::GetInstance().Push(path, target, seconds);

                        std::cout << "TranslateTo " << path << std::endl;
                    });
                });

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
            }).detach();
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

            TranslationBuffer::GetInstance().Update();
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
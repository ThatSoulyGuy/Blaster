#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <boost/asio.hpp>
#include "Client/Core/Window.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/NetworkSerialize.hpp"
#include "Network/ClientRpc.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;

namespace Blaster::Client
{
    class ClientComponent final : public Component
    {

    public:

        void Initialize() override
        {
            if (!GetGameObject()->IsAuthoritative())
            {
                std::random_device device;
                std::mt19937 generator{device()};

                std::uniform_int_distribution<int> distribution{0, 255};

                myNumber = distribution(generator);
            }
        }

        void Update() override
        {
            if (GetGameObject()->IsAuthoritative())
                std::cout << "From RPC! " << myNumber << std::endl;
        }

        std::string GetTypeName() const override
        {
            return typeid(ClientComponent).name();
        }

    private:

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        int myNumber;

    };

    class ClientApplication final
    {
    public:

        ClientApplication(const ClientApplication&) = delete;
        ClientApplication(ClientApplication&&) = delete;
        ClientApplication& operator=(const ClientApplication&) = delete;
        ClientApplication& operator=(ClientApplication&&) = delete;

        void PreInitialize()
        {
            Window::GetInstance().Initialize("Blaster* 1.4.6", { 750, 450 });
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

                    GameObjectManager::GetInstance().Register(gameObject);
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

                    std::string componentType(nul + 1, msg.end());

                    auto optionalGameObject = GameObjectManager::GetInstance().Get(goName);

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

            auto crateFuture = ClientRpc::CreateGameObject("Cratee");

            std::thread([crateFuture = std::move(crateFuture)]() mutable
            {
                if (const auto gameObject = crateFuture.get())
                {
                    auto componentFuture = ClientRpc::AddComponent(gameObject->GetName(), std::make_shared<ClientComponent>());

                    ClientRpc::TranslateTo(gameObject->GetName(), { 5, 0, 0 }, 2.0f);
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

            GameObjectManager::GetInstance().Render();

            Window::GetInstance().Present();
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

        static std::once_flag initializationFlag;
        static std::unique_ptr<ClientApplication> instance;

    };

    std::once_flag ClientApplication::initializationFlag;
    std::unique_ptr<ClientApplication> ClientApplication::instance;
}

BOOST_CLASS_EXPORT(Blaster::Client::ClientComponent)
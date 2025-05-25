#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <boost/asio.hpp>
#include "Client/Core/Window.hpp"
#include "Client/Network/ClientNetwork.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Client::Network;

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
            Window::GetInstance().Initialize("Blaster* 1.1.0", { 750, 450 });
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
            ClientNetwork::GetInstance().RegisterReceiver(PacketType::S2C_Chat, [](std::vector<std::uint8_t> msg)
                {
                    const std::string text(reinterpret_cast<char*>(msg.data()), msg.size());

                    const auto delimeter = text.find('§');

                    if (delimeter != std::string::npos)
                    {
                        const std::string sender  = text.substr(0, delimeter);
                        const std::string content = text.substr(delimeter + 1);

                        std::cout << sender << " >> " << content << '\n';
                    }
                });
        }

        bool IsRunning()
        {
            return Window::GetInstance().IsRunning();
        }

        void Update()
        {
            std::string input;

            std::getline(std::cin, input);

            const std::string hello = ClientNetwork::GetInstance().GetStringId() + "§" + input;
            ClientNetwork::GetInstance().Send(PacketType::C2S_Chat, std::span(reinterpret_cast<const std::uint8_t*>(hello.data()), hello.size()));
        }

        void Render()
        {
            Window::Clear();

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
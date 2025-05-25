#pragma once

#include <memory>
#include <mutex>
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
            Window::GetInstance().Initialize("Blaster* 1.0.0", { 750, 450 });
        }

        void Initialize()
        {
            ClientNetwork::GetInstance().Initialize("127.0.0.1", 7777, "Player" + std::to_string(std::rand()));
            ClientNetwork::GetInstance().RegisterReceiver(PacketType::Chat, [](std::vector<std::uint8_t> msg)
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
            const std::string hello = ClientNetwork::GetInstance().GetStringId() + "§Hello world!";
            ClientNetwork::GetInstance().Send(PacketType::Chat, std::span(reinterpret_cast<const std::uint8_t*>(hello.data()), hello.size()));

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        void Render()
        {
            Window::Clear();

            Window::GetInstance().Present();
        }

        void Uninitialize()
        {

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
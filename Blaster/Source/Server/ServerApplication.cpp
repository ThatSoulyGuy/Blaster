#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include "Server/ServerApplication.hpp"

int main()
{
    auto& instance = Blaster::Server::ServerApplication::GetInstance();

    instance.PreInitialize();
    instance.Initialize();

    while (instance.IsRunning())
    {
        instance.Update();
        instance.Render();
    }

    instance.Uninitialize();
}
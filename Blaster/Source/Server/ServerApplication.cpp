#include "Independent/ComponentRegistry.hpp"
#include "Server/ServerApplication.hpp"

int main()
{
    [[maybe_unused]]
    const bool* unused = &Registry::init;

    auto& instance = Blaster::Server::ServerApplication::GetInstance();

    instance.PreInitialize();
    instance.Initialize();

    while (instance.IsRunning())
        instance.Update();

    instance.Uninitialize();
}
#include "Client/ClientApplication.hpp"
#include "Independent/ComponentRegistry.hpp"

int main()
{
    [[maybe_unused]]
    const bool* unused = &Registry::init;

    auto& instance = Blaster::Client::ClientApplication::GetInstance();

    instance.PreInitialize();
    instance.Initialize();

    while (instance.IsRunning())
    {
        instance.Update();
        instance.Render();
    }

    instance.Uninitialize();
}
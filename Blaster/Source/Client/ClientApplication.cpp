#include "Client/ClientApplication.hpp"
#include "Independent/ComponentInclusions.hpp"

int main()
{
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
#include "Independent/TypeRegistrations.hpp"
#include "Independent/ComponentInclusions.hpp"
#include "Server/ServerApplication.hpp"

int main()
{
    auto& instance = Blaster::Server::ServerApplication::GetInstance();

    instance.PreInitialize();
    instance.Initialize();

    while (instance.IsRunning())
        instance.Update();

    instance.Uninitialize();
}
#pragma once

#include "Client/ClientApplication.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/Math/Transform.hpp"
#include "Server/ServerApplication.hpp"

namespace Registry
{
    inline const bool init = []
    {
        ComponentFactory::Register<Transform>();
        ComponentFactory::Register<Blaster::Server::TestComponent>();
        ComponentFactory::Register<Blaster::Client::ClientComponent>();

        return true;
    }();
}
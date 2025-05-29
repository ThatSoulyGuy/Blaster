#pragma once
#include <cstdint>
#include <future>
#include <unordered_map>
#include "Independent/Network/NetworkSerialize.hpp"

namespace Blaster::Independent::Network
{
    enum class RpcType : std::uint16_t
    {
        C2S_CreateGameObject = 100,
        S2C_CreateGameObject,
        C2S_DestroyGameObject,
        S2C_DestroyGameObject,
        C2S_AddComponent,
        S2C_AddComponent,
        C2S_RemoveComponent,
        S2C_RemoveComponent,
        C2S_AddChild,
        S2C_AddChild,
        C2S_RemoveChild,
        S2C_RemoveChild,
        C2S_TranslateTo,
        S2C_TranslateTo
    };

    struct RpcHeader
    {
        std::uint64_t id;
        RpcType type;
    };
}
#pragma once
#include <cstdint>
#include <future>
#include <unordered_map>
#include "Independent/Network/NetworkSerialize.hpp"

namespace Blaster::Independent::Network
{
    enum class RpcType : std::uint16_t
    {
        C2S_CreateGameObject = 1,
        S2C_CreateGameObject = 2,
        C2S_DestroyGameObject = 3,
        S2C_DestroyGameObject = 4,
        C2S_AddComponent = 5,
        S2C_AddComponent = 6,
        C2S_RemoveComponent = 7,
        S2C_RemoveComponent = 8,
        C2S_AddChild = 9,
        S2C_AddChild = 10,
        C2S_RemoveChild = 11,
        S2C_RemoveChild = 12,
        C2S_TranslateTo = 13,
        S2C_TranslateTo = 14
    };

    struct RpcHeader
    {
        std::uint64_t id;
        RpcType type;
    };
}
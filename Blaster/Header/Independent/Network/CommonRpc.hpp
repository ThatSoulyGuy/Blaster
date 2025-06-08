#pragma once
#include <cstdint>
#include <future>
#include <unordered_map>
#include "Independent/Network/NetworkSerialize.hpp"

namespace Blaster::Independent::Network
{
    enum class RpcType
    {
        C2S_CreateGameObject,
        S2C_CreateGameObject,
        C2S_DestroyGameObject,
        S2C_DestroyGameObject,
        C2S_AddComponent,
        S2C_AddComponent,
        C2S_RemoveComponent,
        S2C_RemoveComponent,
        C2S_TranslateTo,
        S2C_TranslateTo
    };

    struct RpcHeader
    {
        std::uint64_t id;
        RpcType type;
    };
}
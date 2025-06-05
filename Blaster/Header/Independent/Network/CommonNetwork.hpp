#pragma once

#include <boost/asio.hpp>
#include <span>
#include <vector>

namespace Blaster::Independent::Network
{
    using TcpProtocol = boost::asio::ip::tcp;
    using ErrorCode = boost::system::error_code;
    using NetworkId = std::uint32_t;

    enum class PacketType : std::uint16_t
    {
        S2C_RequestStringId = 1,
        C2S_StringId = 2,
        S2C_AssignNetworkId = 3,
        S2C_CreateGameObject = 4,
        S2C_DestroyGameObject = 5,
        S2C_AddComponent = 6,
        S2C_RemoveComponent = 7,
        S2C_AddChild = 8,
        S2C_RemoveChild = 9,
        S2C_TranslateTo = 10,
        S2C_Rpc = 11,
        C2S_Rpc = 12
    };

    struct PacketHeader
    {
        PacketType type;

        std::uint32_t size;
        std::uint32_t from;
    };

    inline std::vector<std::uint8_t> CreatePacket(const PacketType type, const NetworkId from, const std::span<const std::uint8_t> payload)
    {
        const PacketHeader header{type, static_cast<std::uint32_t>(payload.size()), from};

        std::vector<std::uint8_t> buffer(sizeof(PacketHeader) + payload.size());

        std::memcpy(buffer.data(), &header, sizeof header);
        std::memcpy(buffer.data() + sizeof header, payload.data(), payload.size());

        return buffer;
    }
}
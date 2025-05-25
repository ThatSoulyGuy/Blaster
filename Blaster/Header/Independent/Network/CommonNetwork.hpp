#pragma once

#include <boost/asio.hpp>
#include <vector>

namespace Blaster::Independent::Network
{
    using TcpProtocol = boost::asio::ip::tcp;
    using ErrorCode = boost::system::error_code;
    using NetworkID = std::uint32_t;

    enum class PacketType : std::uint16_t
    {
        S2C_RequestStringId = 1,
        C2S_StringId = 2,
        S2C_AssignNetworkId = 3,
        C2S_Chat = 10,
        S2C_Chat = 11
    };

    struct PacketHeader
    {
        PacketType type;

        std::uint32_t size;
        std::uint32_t from;
    };

    static_assert(sizeof(PacketHeader) == 12, "Unexpected padding!");

    inline std::vector<std::uint8_t> CreatePacket(const PacketType type, const NetworkID from, const std::span<const std::uint8_t> payload)
    {
        const PacketHeader header{type, static_cast<std::uint32_t>(payload.size()), from};

        std::vector<std::uint8_t> buf(sizeof(PacketHeader) + payload.size());

        std::memcpy(buf.data(), &header, sizeof header);
        std::memcpy(buf.data() + sizeof header, payload.data(), payload.size());

        return buf;
    }
}
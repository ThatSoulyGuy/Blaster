#pragma once

#include "Independent/Network/CommonNetwork.hpp"
#include "Server/Command/CommandParser.hpp"
#include "Server/Command/CommandSender.hpp"

namespace Blaster::Server::Command
{
	struct CommandPacket
	{
        std::string senderName;
        CommandAuthority senderAuthority;
        std::string line;
	};
}

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Server::Command::CommandPacket> : Blaster::Independent::Network::DataConversionBase<DataConversion<Blaster::Server::Command::CommandPacket>, Blaster::Server::Command::CommandPacket>
{
    using Type = Blaster::Server::Command::CommandPacket;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, value.senderName);
        CommonNetwork::WriteTrivial<std::uint8_t>(buffer, static_cast<std::uint8_t>(value.senderAuthority));
        CommonNetwork::EncodeString(buffer, value.line);
    }

    static std::any Decode(const std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;
        Type result;

        result.senderName = CommonNetwork::DecodeString(bytes, offset);
        result.senderAuthority = static_cast<Blaster::Server::Command::CommandAuthority>(CommonNetwork::ReadTrivial<std::uint8_t>(bytes, offset));
        result.line = CommonNetwork::DecodeString(bytes, offset);

        return result;
    }
};
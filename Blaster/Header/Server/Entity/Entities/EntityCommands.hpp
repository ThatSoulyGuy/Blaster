#pragma once

#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Transform3d.hpp"
#include "Independent/Physics/Collider.hpp"

namespace Blaster::Server::Entity::Entities
{
    struct DamageCommand
    {
        std::string path;

        bool isDamage;

        float damage;

        static constexpr std::uint8_t CODE = 72;
    };

    struct RespawnCommand
    {
        std::string path;

        static constexpr std::uint8_t CODE = 73;
    };
}

namespace Blaster::Independent::Network
{
    template <>
    struct DataConversion<Blaster::Server::Entity::Entities::DamageCommand> : DataConversionBase<DataConversion<Blaster::Server::Entity::Entities::DamageCommand>, Blaster::Server::Entity::Entities::DamageCommand>
    {
        using Type = Blaster::Server::Entity::Entities::DamageCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
            CommonNetwork::WriteTrivial(buffer, operation.isDamage);
            CommonNetwork::WriteTrivial(buffer, operation.damage);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type result;

            result.path = CommonNetwork::DecodeString(bytes, offset);
            result.isDamage = CommonNetwork::ReadTrivial<bool>(bytes, offset);
            result.damage = CommonNetwork::ReadTrivial<float>(bytes, offset);

            return result;
        }
    };

    template <>
    struct DataConversion<Blaster::Server::Entity::Entities::RespawnCommand> : DataConversionBase<DataConversion<Blaster::Server::Entity::Entities::RespawnCommand>, Blaster::Server::Entity::Entities::RespawnCommand>
    {
        using Type = Blaster::Server::Entity::Entities::RespawnCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type result;

            result.path = CommonNetwork::DecodeString(bytes, offset);

            return result;
        }
    };
}
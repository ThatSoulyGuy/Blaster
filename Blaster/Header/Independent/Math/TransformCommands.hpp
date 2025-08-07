#pragma once

#include "Independent/Math/Vector.hpp"
#include "Independent/Network/CommonNetwork.hpp"

namespace Blaster::Independent::Math
{
    struct QueryTransformCommand
    {
        std::string path;
        Vector<float, 3> position;

        static constexpr std::uint8_t CODE = 75;
    };

    struct CorrectTransformCommand
    {
        std::string path;
        Vector<float, 3> position;

        static constexpr std::uint8_t CODE = 76;
    };
}

namespace Blaster::Independent::Network
{
    template <>
    struct DataConversion<Blaster::Independent::Math::QueryTransformCommand> : DataConversionBase<DataConversion<Blaster::Independent::Math::QueryTransformCommand>, Blaster::Independent::Math::QueryTransformCommand>
    {
        using Type = Blaster::Independent::Math::QueryTransformCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
            DataConversion<Blaster::Independent::Math::Vector<float, 3>>::Encode(operation.position, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type result;

            result.path = CommonNetwork::DecodeString(bytes, offset);
            result.position = std::any_cast<Blaster::Independent::Math::Vector<float, 3>>(DataConversion<Blaster::Independent::Math::Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Blaster::Independent::Math::Vector<float, 3>>::kWireSize)));

            offset += DataConversion<Blaster::Independent::Math::Vector<float, 3>>::kWireSize;

            return result;
        }
    };

    template <>
    struct DataConversion<Blaster::Independent::Math::CorrectTransformCommand> : DataConversionBase<DataConversion<Blaster::Independent::Math::CorrectTransformCommand>, Blaster::Independent::Math::CorrectTransformCommand>
    {
        using Type = Blaster::Independent::Math::CorrectTransformCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
            DataConversion<Blaster::Independent::Math::Vector<float, 3>>::Encode(operation.position, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type result;

            result.path = CommonNetwork::DecodeString(bytes, offset);
            result.position = std::any_cast<Blaster::Independent::Math::Vector<float, 3>>(DataConversion<Blaster::Independent::Math::Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Blaster::Independent::Math::Vector<float, 3>>::kWireSize)));

            offset += DataConversion<Blaster::Independent::Math::Vector<float, 3>>::kWireSize;

            return result;
        }
    };
}
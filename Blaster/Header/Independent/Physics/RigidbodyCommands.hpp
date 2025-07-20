#pragma once

#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Transform.hpp"
#include "Independent/Physics/Collider.hpp"

namespace Blaster::Independent::Physics
{
    struct ImpulseCommand
    {
        std::string path;

        bool hasPoint;

        Vector<float, 3> impulse;
        Vector<float, 3> point;

        static constexpr std::uint8_t CODE = 42;
    };

    struct SetTransformCommand
    {
        std::string path;

        Vector<float, 3> position;
        Vector<float, 3> rotation;

        static constexpr std::uint8_t CODE = 43;
    };
}

namespace Blaster::Independent::Network
{
    template <>
    struct DataConversion<Blaster::Independent::Physics::ImpulseCommand> : DataConversionBase<DataConversion<Blaster::Independent::Physics::ImpulseCommand>, Blaster::Independent::Physics::ImpulseCommand>
    {
        using Type = Blaster::Independent::Physics::ImpulseCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);
            CommonNetwork::WriteTrivial(buffer, operation.hasPoint);
            DataConversion<Vector<float, 3>>::Encode(operation.impulse, buffer);
            DataConversion<Vector<float, 3>>::Encode(operation.point, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type result;

            result.path = CommonNetwork::DecodeString(bytes, offset);
            result.hasPoint = CommonNetwork::ReadTrivial<bool>(bytes, offset);
            result.impulse = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            offset += DataConversion<Vector<float, 3>>::kWireSize;

            result.point = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            return result;
        }
    };

    template <>
    struct DataConversion<Blaster::Independent::Physics::SetTransformCommand> : DataConversionBase<DataConversion<Blaster::Independent::Physics::SetTransformCommand>, Blaster::Independent::Physics::SetTransformCommand>
    {
        using Type = Blaster::Independent::Physics::SetTransformCommand;

        static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
        {
            CommonNetwork::EncodeString(buffer, operation.path);

            DataConversion<Vector<float, 3>>::Encode(operation.position, buffer);
            DataConversion<Vector<float, 3>>::Encode(operation.rotation, buffer);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, offset);

            out.position = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            offset += DataConversion<Vector<float, 3>>::kWireSize;

            out.rotation = std::any_cast<Vector<float, 3>>(DataConversion<Vector<float, 3>>::Decode(bytes.subspan(offset, DataConversion<Vector<float, 3>>::kWireSize)));

            return out;
        }
    };
}
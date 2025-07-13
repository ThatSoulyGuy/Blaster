#pragma once

#include <cstdint>
#include <string>
#include "Independent/Math/Arithmetic.hpp"
#include "Independent/Utility/TypeRegistrar.hpp"

namespace Blaster::Independent::ECS::Synchronization
{
    struct SnapshotHeader;
    struct Snapshot;

    struct OpCreate;
    struct OpDestroy;
    struct OpAddComponent;
    struct OpRemoveComponent;
    struct OpSetField;
}

namespace Blaster::Independent::Physics
{
    struct ImpulseCommand;
    struct SetTransformCommand;
}

namespace Blaster::Independent::Math
{
    template <Arithmetic, std::size_t N> requires (N > 1)
    class Vector;
}

REGISTER_TYPE(std::int32_t, 37387)
REGISTER_TYPE(std::uint32_t, 25266)
REGISTER_TYPE(std::uint64_t, 42775)
REGISTER_TYPE(float, 42526)
REGISTER_TYPE(double, 14345)
REGISTER_TYPE(std::string, 97573)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::SnapshotHeader, 32546)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::Snapshot, 23264)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::OpCreate, 35635)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::OpDestroy, 22789)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::OpAddComponent, 36578)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::OpRemoveComponent, 13466)
REGISTER_TYPE(Blaster::Independent::ECS::Synchronization::OpSetField, 87953)
REGISTER_TYPE(Blaster::Independent::Physics::ImpulseCommand, 25467)
REGISTER_TYPE(Blaster::Independent::Physics::SetTransformCommand, 17834)

namespace Blaster::Independent::Utility
{
    template <>
    struct TypeIdFromType<Blaster::Independent::Math::Vector<float, 3>> : std::integral_constant<std::size_t, 616402872> { };

    template <>
    struct TypeFromId<616402872>
    {
        using Type = Blaster::Independent::Math::Vector<float, 3>;
    };
}
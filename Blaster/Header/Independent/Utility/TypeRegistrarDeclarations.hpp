#pragma once

#include <string>
#include <boost/preprocessor/punctuation/remove_parens.hpp>
#include "Independent/Math/Arithmetic.hpp"

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
    struct SetVelocityCommand;
    struct CharacterControllerInputCommand;
}

namespace Blaster::Independent::Math
{
    template <Arithmetic, std::size_t N> requires (N > 1)
        class Vector;

    struct QueryTransformCommand;
    struct CorrectTransformCommand;
}

namespace Blaster::Server::Command
{
    struct CommandPacket;
}

namespace Blaster::Server::Entity::Entities
{
    struct DamageCommand;
    struct RespawnCommand;
}

#define BLASTER_FOR_EACH_REGISTERED_TYPE(X) \
X(std::int32_t, 37387) \
X(std::uint32_t, 25266) \
X(float, 42526) \
X(double, 14345) \
X(std::string, 97573) \
X(Blaster::Independent::ECS::Synchronization::SnapshotHeader, 32546) \
X(Blaster::Independent::ECS::Synchronization::Snapshot, 23264) \
X(Blaster::Independent::ECS::Synchronization::OpCreate, 35635) \
X(Blaster::Independent::ECS::Synchronization::OpDestroy, 22789) \
X(Blaster::Independent::ECS::Synchronization::OpAddComponent, 36578) \
X(Blaster::Independent::ECS::Synchronization::OpRemoveComponent, 13466) \
X(Blaster::Independent::ECS::Synchronization::OpSetField, 87953) \
X(Blaster::Independent::Physics::ImpulseCommand, 25467) \
X(Blaster::Independent::Physics::SetTransformCommand, 17834) \
X(Blaster::Independent::Physics::SetVelocityCommand, 92123) \
X(Blaster::Independent::Physics::CharacterControllerInputCommand, 12686) \
X(Blaster::Independent::Math::QueryTransformCommand, 21576) \
X(Blaster::Independent::Math::CorrectTransformCommand, 11399) \
X(Blaster::Server::Command::CommandPacket, 12321) \
X(Blaster::Server::Entity::Entities::DamageCommand, 62289) \
X(Blaster::Server::Entity::Entities::RespawnCommand, 82181) \
X((Blaster::Independent::Math::Vector<float, 3>), 616402872)

namespace Blaster::Independent::Utility
{
    template<typename T> struct TypeIdFromType;
    template<std::size_t Id> struct TypeFromId;

    #define BLASTER_DECLARE_SPEC(TYPE, ID) \
    template<> struct TypeIdFromType<BOOST_PP_REMOVE_PARENS(TYPE)> : std::integral_constant<std::size_t, ID> {}; \
    template<> struct TypeFromId<ID> { using Type = BOOST_PP_REMOVE_PARENS(TYPE); };

    BLASTER_FOR_EACH_REGISTERED_TYPE(BLASTER_DECLARE_SPEC)

    #undef BLASTER_DECLARE_SPEC
}
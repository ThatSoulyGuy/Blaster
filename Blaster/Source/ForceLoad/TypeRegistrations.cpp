#pragma once

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

using namespace Blaster::Independent::Utility;

namespace
{
    template <typename T, std::size_t Id>
    BLASTER_USED inline const bool RegisterRuntimeNameOnce = []()
    {
        const std::string demangled = TypeIdFromType<T>::DemangleName(typeid(T).name());

        Add(Id, demangled);

        return true;
    }();

    #define BLASTER_DEFINE_RUNTIME(TYPE, ID) BLASTER_USED const bool BOOST_PP_CAT(r, __COUNTER__) = RegisterRuntimeNameOnce<BOOST_PP_REMOVE_PARENS(TYPE), ID>;

    BLASTER_FOR_EACH_REGISTERED_TYPE(BLASTER_DEFINE_RUNTIME)

    #undef BLASTER_DEFINE_RUNTIME
}
#include <string>
#include "Independent/ECS/Synchronization/CommonSynchronization.hpp"
#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Physics/PhysicsCommands.hpp"
#include "Server/Command/CommandNetworking.hpp"
#include "Server/Entity/Entities/EntityCommands.hpp"

using namespace Blaster::Independent::Network;

namespace
{
    struct ForceLoadRegistrars
    {
        ForceLoadRegistrars()
        {
            DataConversion<std::int32_t>::EnsureRegistered();
            DataConversion<std::uint32_t>::EnsureRegistered();
            DataConversion<float>::EnsureRegistered();
            DataConversion<double>::EnsureRegistered();
            DataConversion<std::string>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::SnapshotHeader>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::Snapshot>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::OpCreate>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::OpDestroy>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::OpAddComponent>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::OpRemoveComponent>::EnsureRegistered();
            DataConversion<Blaster::Independent::ECS::Synchronization::OpSetField>::EnsureRegistered();
            DataConversion<Blaster::Independent::Physics::ImpulseCommand>::EnsureRegistered();
            DataConversion<Blaster::Independent::Physics::SetTransformCommand>::EnsureRegistered();
            DataConversion<Blaster::Independent::Physics::SetVelocityCommand>::EnsureRegistered();
            DataConversion<Blaster::Independent::Physics::CharacterControllerInputCommand>::EnsureRegistered();
            DataConversion<Blaster::Independent::Math::QueryTransformCommand>::EnsureRegistered();
            DataConversion<Blaster::Independent::Math::CorrectTransformCommand>::EnsureRegistered();
            DataConversion<Blaster::Server::Command::CommandPacket>::EnsureRegistered();
            DataConversion<Blaster::Server::Entity::Entities::DamageCommand>::EnsureRegistered();
            DataConversion<Blaster::Server::Entity::Entities::RespawnCommand>::EnsureRegistered();
            DataConversion<Blaster::Independent::Math::Vector<float, 3>>::EnsureRegistered();
        }
    };

    static ForceLoadRegistrars sAnchors{};
}

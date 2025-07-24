#pragma once

#include <btBulletDynamicsCommon.h>
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"

using namespace Blaster::Independent::ECS;

namespace Blaster::Independent::Physics
{
    class PhysicsBody : public Component
    {

    public:
        
        virtual ~PhysicsBody() = default;

        virtual void SyncToBullet() { }
        virtual void SyncFromBullet() { }

        virtual btCollisionObject* GetCollisionObject() const = 0;

    protected:

        bool IsLocallyControlled() const
        {
#ifdef IS_SERVER
            return true;
#else
            return GetGameObject()->IsLocallyControlled();
#endif
        }

        virtual bool IsAuthoritative() const noexcept = 0;

        template<typename Command>
        void QueueToServer(PacketType type, Command&& command) const
        {
#ifndef IS_SERVER
            if (IsLocallyControlled())
                Client::Network::ClientNetwork::GetInstance().Send(type, std::forward<Command>(command));
#endif
        }


    private:

        DESCRIBE_AND_REGISTER(PhysicsBody, (Component), (), (), ())
        
    };
}
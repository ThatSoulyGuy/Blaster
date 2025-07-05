#pragma once

#include <btBulletDynamicsCommon.h>
#include <memory>

#include "Independent/Utility/Time.hpp"

using namespace Blaster::Independent::Utility;

namespace Blaster::Independent::Collider
{
    class PhysicsWorld final
    {
        
    public:

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        void Update() const
        {
            world->stepSimulation(Time::GetInstance().GetDeltaTime(), 10, 1.f / 120.f);
        }

        [[nodiscard]]
        btDiscreteDynamicsWorld* GetWorld() const
        {
            return world.get();
        }

        void RegisterRigidBody(btRigidBody* rb) const
        {
            world->addRigidBody(rb);
        }

        void UnregisterRigidBody(btRigidBody* rb) const
        {
            world->removeRigidBody(rb);
        }

        static PhysicsWorld& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<PhysicsWorld>(new PhysicsWorld());
            });

            return *instance;
        }

    private:

        PhysicsWorld()
        {
            broadphase = std::make_unique<btDbvtBroadphase>();
            collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
            dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfig.get());
            solver = std::make_unique<btSequentialImpulseConstraintSolver>();
            world = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), broadphase.get(), solver.get(), collisionConfig.get());
            world->setGravity({ 0, -9.81f, 0 });
        }

        std::unique_ptr<btBroadphaseInterface> broadphase;
        std::unique_ptr<btDefaultCollisionConfiguration> collisionConfig;
        std::unique_ptr<btCollisionDispatcher> dispatcher;
        std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
        std::unique_ptr<btDiscreteDynamicsWorld> world;

        static std::once_flag initializationFlag;
        static std::unique_ptr<PhysicsWorld> instance;

    };

    std::once_flag PhysicsWorld::initializationFlag;
    std::unique_ptr<PhysicsWorld> PhysicsWorld::instance;
}

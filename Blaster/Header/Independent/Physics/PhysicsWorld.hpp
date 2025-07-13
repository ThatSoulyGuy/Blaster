#pragma once

#include <memory>
#include <btBulletDynamicsCommon.h>
#include "Independent/Utility/Time.hpp"

using namespace Blaster::Independent::Utility;

namespace Blaster::Independent::Physics
{
    class PhysicsWorld final
    {

    public:

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        void Initialize()
        {
            collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
            dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfiguration.get());
            broadphase = std::make_unique<btDbvtBroadphase>();
            solver = std::make_unique<btSequentialImpulseConstraintSolver>();

            world = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get());
            world->setGravity(btVector3(0.0f, -9.8f, 0.0f));
        }

        void Update()
        {
            constexpr float fixedStep = 1.0f / 120.0f;
            constexpr int maxSubSteps = 4;

            world->stepSimulation(Time::GetInstance().GetDeltaTime(), maxSubSteps, fixedStep);
        }

        void AddBody(btRigidBody* body)
        {
            world->addRigidBody(body);
        }

        void RemoveBody(btRigidBody* body)
        {
            world->removeRigidBody(body);
        }

        [[nodiscard]]
        btDiscreteDynamicsWorld* GetHandle() const
        {
            return world.get();
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

        PhysicsWorld() = default;

        std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
        std::unique_ptr<btCollisionDispatcher> dispatcher;
        std::unique_ptr<btBroadphaseInterface> broadphase;
        std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
        std::unique_ptr<btDiscreteDynamicsWorld> world;

        static std::once_flag initializationFlag;
        static std::unique_ptr<PhysicsWorld> instance;

    };

    std::once_flag PhysicsWorld::initializationFlag;
    std::unique_ptr<PhysicsWorld> PhysicsWorld::instance;
}
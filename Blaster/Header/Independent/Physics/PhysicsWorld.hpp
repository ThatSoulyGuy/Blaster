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
            collisionConfiguration = new btDefaultCollisionConfiguration();
            dispatcher = new btCollisionDispatcher(collisionConfiguration);
            broadphase = new btDbvtBroadphase();
            solver = new btSequentialImpulseConstraintSolver();

            world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
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
            if (!world)
                return;

            world->addRigidBody(body);
        }

        void RemoveBody(btRigidBody* body)
        {
            if (!world)
                return;

            world->removeRigidBody(body);
        }

        [[nodiscard]]
        btDiscreteDynamicsWorld* GetHandle() const
        {
            return world;
        }

        void Uninitialize()
        {
            delete world;
            delete collisionConfiguration;
            delete dispatcher;
            delete broadphase;
            delete solver;
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

        btDefaultCollisionConfiguration* collisionConfiguration;
        btCollisionDispatcher* dispatcher;
        btBroadphaseInterface* broadphase;
        btSequentialImpulseConstraintSolver* solver;
        btDiscreteDynamicsWorld* world;

        static std::once_flag initializationFlag;
        static std::unique_ptr<PhysicsWorld> instance;

    };

    std::once_flag PhysicsWorld::initializationFlag;
    std::unique_ptr<PhysicsWorld> PhysicsWorld::instance;
}
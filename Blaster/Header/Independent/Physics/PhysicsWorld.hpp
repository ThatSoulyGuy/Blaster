#pragma once

#include <memory>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
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

            broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());

            solver = new btSequentialImpulseConstraintSolver();

            world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
            world->setGravity(btVector3(0.0f, -9.81f, 0.0f));
        }

        void Update()
        {
            constexpr double kFixedStep = 1.0 / 120.0;
            constexpr double kMaxFrame = 0.25;

            static double accumulator = 0.0;
            const double frameDt = std::min(static_cast<double>(Time::GetInstance().GetDeltaTime()), kMaxFrame);

            accumulator += frameDt;

            while (accumulator >= kFixedStep)
            {
                world->stepSimulation(static_cast<btScalar>(kFixedStep), 0, static_cast<btScalar>(kFixedStep));
                accumulator -= kFixedStep;
            }
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
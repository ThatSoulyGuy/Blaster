#pragma once

#include <memory>
#include "Independent/Utility/Time.hpp"
#include "Independent/Utility/BulletSterilized.hpp"

#ifndef IS_SERVER
#include "Client/Render/Camera.hpp"
#include "Independent/Test/DebugDrawer.hpp"
#endif

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

#ifndef IS_SERVER
            drawer = new Blaster::Independent::Test::DebugDrawer();
            world->setDebugDrawer(drawer);
#endif
        }

#ifndef IS_SERVER
        void Render(std::shared_ptr<Blaster::Client::Render::Camera> camera)
        {
            if (drawDebug)
            {
                world->debugDrawWorld();
                drawer->flush(camera->GetProjectionMatrix() * camera->GetViewMatrix());
            }
        }
#endif

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

#ifndef IS_SERVER
        void ToggleDrawDebug()
        {
            drawDebug = !drawDebug;
        }

        bool IsDrawingDebug() const
        {
            return drawDebug;
        }
#endif
        
        void Uninitialize()
        {
            delete world;
            delete collisionConfiguration;
            delete dispatcher;
            delete broadphase;
            delete solver;

#ifndef IS_SERVER
            delete drawer;
#endif
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

#ifndef IS_SERVER
        Blaster::Independent::Test::DebugDrawer* drawer;
        bool drawDebug;
#endif

        static std::once_flag initializationFlag;
        static std::unique_ptr<PhysicsWorld> instance;

    };
}
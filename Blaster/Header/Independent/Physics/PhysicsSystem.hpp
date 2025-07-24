#pragma once

#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Physics/PhysicsBody.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"

namespace Blaster::Independent::Physics
{
    class PhysicsSystem final
    {

    public:

        PhysicsSystem(const PhysicsSystem&) = delete;
        PhysicsSystem(PhysicsSystem&&) = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;
        PhysicsSystem& operator=(PhysicsSystem&&) = delete;

        void Update()
        {
            ForEachBody([](auto& body) { body.SyncToBullet(); });

            constexpr float fixedStep = 1.f / 120.f;
            constexpr int maxSubStep = 4;

            auto* world = PhysicsWorld::GetInstance().GetHandle();

            world->stepSimulation(Time::GetInstance().GetDeltaTime(), maxSubStep, fixedStep);

            ForEachBody([](auto& body) { body.SyncFromBullet(); });
        }

        static PhysicsSystem& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
                {
                    instance = std::unique_ptr<PhysicsSystem>(new PhysicsSystem());
                });

            return *instance;
        }

    private:

        PhysicsSystem() = default;

        template <typename Function>
        static void ForEachBody(Function&& function)
        {
            for (const auto& gameObject : ECS::GameObjectManager::GetInstance().GetAll())
            {
                for (const auto& [type, comp] : gameObject->GetComponentMap())
                {
                    if (auto* body = dynamic_cast<PhysicsBody*>(comp.get()))
                        function(*body);
                }
            }
        }

        static std::once_flag initializationFlag;
        static std::unique_ptr<PhysicsSystem> instance;

    };

    std::once_flag PhysicsSystem::initializationFlag;
    std::unique_ptr<PhysicsSystem> PhysicsSystem::instance;
}

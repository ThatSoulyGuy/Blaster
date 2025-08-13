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
            static double acc = 0.0;
            constexpr double fixed = 1.0 / 120.0;
            acc += std::clamp<double>(Time::GetInstance().GetDeltaTime(), 0.0, 0.1);

            ForEachBody([](auto& body) { body.SyncToBullet(); });

            auto* world = PhysicsWorld::GetInstance().GetHandle();
            int steps = 0;

            while (acc >= fixed && steps < 8)
            {
                world->stepSimulation(fixed, 0, fixed);

                acc -= fixed;

                ++steps;
            }

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
                for (const auto& component : gameObject->GetComponentMap() | std::views::values)
                {
                    if (auto* body = dynamic_cast<PhysicsBody*>(component.get()))
                    {
                        if (body)
                            function(*body);
                    }
                }
            }
        }

        static std::once_flag initializationFlag;
        static std::unique_ptr<PhysicsSystem> instance;

    };

    std::once_flag PhysicsSystem::initializationFlag;
    std::unique_ptr<PhysicsSystem> PhysicsSystem::instance;
}

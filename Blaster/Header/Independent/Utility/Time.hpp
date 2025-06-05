#pragma once

#include <chrono>
#include <mutex>

namespace Blaster::Independent::Utility
{
    class Time final
    {

    public:

        Time(const Time&) = delete;
        Time(Time&&) = delete;
        Time& operator=(const Time&) = delete;
        Time& operator=(Time&&) = delete;

        void Update()
        {
            const auto now = std::chrono::high_resolution_clock::now();

            if (!initialized)
            {
                lastFrameTime = now;
                deltaTime = 0.0f;

                initialized = true;

                return;
            }

            const std::chrono::duration<float> frameDuration = now - lastFrameTime;

            deltaTime = frameDuration.count();

            lastFrameTime = now;
        }

        float GetDeltaTime() const
        {
            return deltaTime;
        }

        static Time& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<Time>(new Time());
            });

            return *instance;
        }

    private:

        Time() = default;

        bool initialized = false;

        std::chrono::high_resolution_clock::time_point lastFrameTime;

        float deltaTime = 0.0f;

        static std::once_flag initializationFlag;
        static std::unique_ptr<Time> instance;

    };

    std::once_flag Time::initializationFlag;
    std::unique_ptr<Time> Time::instance;
}
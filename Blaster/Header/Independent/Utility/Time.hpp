#pragma once

#include <chrono>
#include <mutex>

#undef GetCurrentTime

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
            using clock = std::chrono::steady_clock;

            const auto now = clock::now();

            if (!initialized)
            {
                last = now;
                elapsed = 0.0f;
                delta = 0.0f;

                initialized = true;

                return;
            }

            delta = std::chrono::duration<float>(now - last).count();
            elapsed += delta;
            last = now;
        }

        float GetDeltaTime() const
        {
            return delta;
        }

        float GetCurrentTime() const
        {
            return elapsed;
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

        float elapsed = 0.0f;
        float delta = 0.0f;

        std::chrono::steady_clock::time_point last;

        static std::once_flag initializationFlag;
        static std::unique_ptr<Time> instance;

    };

    std::once_flag Time::initializationFlag;
    std::unique_ptr<Time> Time::instance;
}
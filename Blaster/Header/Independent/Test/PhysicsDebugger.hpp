#pragma once

namespace Blaster::Independent::Test
{

}

#ifdef _WIN32

#define NOMINMAX

#include <windows.h>
#include <iostream>
#include <fstream>
#include <mutex>
#include <map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Transform3d.hpp"
#include "Independent/Physics/PhysicsCommands.hpp"

#undef min
#undef max

using namespace Blaster::Independent::Physics;

namespace Blaster::Independent::Test
{

    class PhysicsDebugger
    {

    public:

        static void Initialize()
        {
            std::lock_guard lk(mutex);

            if (!consoleAllocated)
            {
                AllocConsole();

                freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
                freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);

                consoleAllocated = true;
            }

#ifdef IS_SERVER
            out.open("PhysicsDebugger_Server.log", std::ios::out);
#else
            out.open("PhysicsDebugger_Client.log", std::ios::out);
#endif
            out << "=== Physics Debugger START " << TimeStamp() << " ===\n" << "Time      | Rigidbody   | Pos (x,y,z)        | Rot (x,y,z)        | DeltaRot\n";
        }

        static void Uninitialize()
        {
            std::lock_guard lk(mutex);

            out << "=== Physics Debugger STOP  " << TimeStamp() << " ===\n";
            out.close();

            if (consoleAllocated)
            {
                FreeConsole();

                consoleAllocated = false;
            }
        }

        static void RegisterRigidbody(btRigidBody* rb)
        {
            std::lock_guard lk(mutex);
            lastRot[rb] = { 0, 0, 0 };
        }

        static void LogTransform(btRigidBody* rb, const Independent::Math::Transform3d& xf)
        {
            std::lock_guard lk(mutex);

            auto pos = xf.GetWorldPosition();
            auto rot = xf.GetWorldRotation();

            auto& last = lastRot[rb];

            Independent::Math::Vector<float, 3> delta
            {
                std::abs(rot.x() - last.x()),
                std::abs(rot.y() - last.y()),
                std::abs(rot.z() - last.z())
            };

            float maxDelta = std::max({ delta.x(), delta.y(), delta.z() });

            out << TimeStamp() << " | "
                << std::setw(10) << rb << " | "
                << std::fixed << std::setprecision(2)
                << "(" << pos.x() << "," << pos.y() << "," << pos.z() << ") | "
                << "(" << rot.x() << "," << rot.y() << "," << rot.z() << ") | "
                << "(" << delta.x() << "," << delta.y() << "," << delta.z() << ")";

            if (maxDelta > spikeThreshold)
                out << "  <*** ROT SPIKE ***>";

            out << "\n";
            last = rot;
        }

        static void LogImpulsePacket(const ImpulseCommand& cmd)
        {
            std::lock_guard lk(mutex);

            out << TimeStamp() << " [NET] Impulse -> path=" << cmd.path
                << " impulse=("
                << cmd.impulse.x() << "," << cmd.impulse.y() << "," << cmd.impulse.z()
                << ")\n";
        }

        static void LogSetTransformPacket(const SetTransformCommand& cmd)
        {
            std::lock_guard lk(mutex);
            out << TimeStamp() << " [NET] SetTransform -> path=" << cmd.path
                << " pos=("
                << cmd.position.x() << "," << cmd.position.y() << "," << cmd.position.z()
                << ") rot=("
                << cmd.rotation.x() << "," << cmd.rotation.y() << "," << cmd.rotation.z()
                << ")\n";
        }

    private:

        static std::string TimeStamp()
        {
            using namespace std::chrono;

            auto now = system_clock::now();
            auto t = system_clock::to_time_t(now);
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            std::ostringstream ss;

            ss << std::put_time(std::localtime(&t), "%H:%M:%S")
                << "." << std::setfill('0') << std::setw(3) << ms.count();

            return ss.str();
        }

        static inline bool consoleAllocated = false;
        static inline std::mutex mutex;
        static inline std::ofstream out;
        static inline std::map<btRigidBody*, Independent::Math::Vector<float, 3>> lastRot;
        static inline float spikeThreshold = 30.0f;
    };

}

#endif
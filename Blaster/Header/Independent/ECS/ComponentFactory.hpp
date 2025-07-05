#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <boost/serialization/shared_ptr.hpp>
#include "Independent/ECS/Component.hpp"

#if defined(__GNUG__)
#include <cxxabi.h>
#elif defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

#undef min
#undef max
#undef ERROR
#endif

namespace Blaster::Independent::ECS
{
    class ComponentFactory
    {

    public:

        using Creator = std::function<std::shared_ptr<Component>()>;

        template<typename T>
        static void Register()
        {
            const std::string key = Demangle(typeid(T).name());

            std::lock_guard guard(GetMutex());

            GetRegistry()[key] = []()
            {
                return std::static_pointer_cast<Component>(std::shared_ptr<T>(new T()));
            };
        }

        static std::shared_ptr<Component> Instantiate(const std::string& typeName)
        {
            std::lock_guard guard(GetMutex());

            auto& registry = GetRegistry();
            const auto iterator = registry.find(typeName);

            if (iterator == registry.end())
                return nullptr;

            return iterator->second();
        }

    private:

        static std::unordered_map<std::string, Creator>& GetRegistry()
        {
            static std::unordered_map<std::string, Creator> registry;

            return registry;
        }

        static std::mutex& GetMutex()
        {
            static std::mutex mutex;

            return mutex;
        }

        static std::string Demangle(const char* raw)
        {
#if defined(__GNUG__)
            int status = 0;

            std::unique_ptr<char, void(*)(void*)> p { abi::__cxa_demangle(raw, nullptr, nullptr, &status), std::free };

            return status == 0 ? std::string{ p.get() } : raw;
#elif defined(_MSC_VER)
            char buf[1024];

            if (UnDecorateSymbolName(raw, buf, sizeof(buf), UNDNAME_NAME_ONLY))
                return buf;

            return raw;
#else
            return raw;
#endif
        }
    };
}
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <boost/serialization/access.hpp>
#include <boost/serialization/export.hpp>
#include <boost/preprocessor.hpp>
#include "Independent/ECS/MergeSupport.hpp"

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

#define OPERATOR_CHECK_DETAIL(r, data, i, elem) \
    BOOST_PP_IF(i, && ,) elem == data.elem

#define OPERATOR_CHECK(...) \
    BOOST_PP_SEQ_FOR_EACH_I(OPERATOR_CHECK_DETAIL, other, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

namespace Blaster::Client::Render
{
    class Camera;
}

namespace Blaster::Independent::ECS
{ 
    inline std::string Demangle(const char* raw)
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

    namespace Synchronization
    {
        class ReceiverSynchronization;
    }

    class GameObject;

    class Component
    {

    public:

        virtual ~Component() = default;

        [[nodiscard]]
        bool WasAdded() const noexcept
        {
            return wasAdded;
        }

        void ClearWasAdded()
        {
            wasAdded = false;
        }

        [[nodiscard]]
        bool WasRemoved() const noexcept
        {
            return wasRemoved;
        }
        
        void ClearWasRemoved()
        {
            wasRemoved = false;
        }

        void ClearTransientFlags() noexcept
        {
            wasAdded = false;
            wasRemoved = false;
        }

        virtual void Initialize() {}
        virtual void Update() {}
        virtual void Render(const std::shared_ptr<Client::Render::Camera>&) {}

        [[nodiscard]]
        std::string GetTypeName() const
        {
            return CachedName(typeid(*this));
        }

        [[nodiscard]]
        std::shared_ptr<GameObject> GetGameObject() const
        {
            return gameObject.lock();
        }

    private:

        friend class Blaster::Independent::ECS::GameObject;
        friend class Blaster::Independent::ECS::Synchronization::ReceiverSynchronization;

        static const std::string& CachedName(const std::type_info& info)
        {
            static std::unordered_map<std::size_t, std::string> cache;
            const std::size_t key = info.hash_code();

            auto iterator = cache.find(key);

            if (iterator == cache.end())
                iterator = cache.emplace(key, Demangle(info.name())).first;

            return iterator->second;
        }

        std::weak_ptr<GameObject> gameObject;

        bool wasAdded = false;
        bool wasRemoved = false;

        friend class boost::serialization::access;

        template <class Archive>
        void serialize(Archive&, const unsigned) {}

        BOOST_DESCRIBE_CLASS(Component, (), (), (), ())
    };
}

BOOST_CLASS_EXPORT(Blaster::Independent::ECS::Component)
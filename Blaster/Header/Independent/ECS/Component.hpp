#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <boost/serialization/access.hpp>
#include <boost/serialization/export.hpp>
#include <boost/preprocessor.hpp>
#include "Independent/ECS/MergeSupport.hpp"

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp.lib")
#else
#include <cxxabi.h>
#include <cstdlib>
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
    namespace Synchronization
    {
        class ReceiverSynchronization;
    }

    [[nodiscard]]
    static std::string DemangleName(const char* encodedName)
    {
#if defined(_MSC_VER)
        char buffer[1024] = {};

        if(UnDecorateSymbolName(encodedName, buffer, static_cast<DWORD>(std::size(buffer)), UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_MEMBER_TYPE | UNDNAME_NO_THISTYPE))
            return buffer;

        throw std::runtime_error("Failed to demangle name");
#else
        int status = 0;

        const std::unique_ptr<char, void(*)(void*)> result{ abi::__cxa_demangle(encodedName, nullptr, nullptr, &status), std::free };

        return status == 0 ? result.get() : throw std::runtime_error("Failed to demangle name");;
#endif
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

        void SetShouldSynchronize(bool shouldSynchronize)
        {
            this->shouldSynchronize = shouldSynchronize;
        }

        [[nodiscard]]
        bool ShouldSynchronize() const noexcept
        {
            return shouldSynchronize;
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

        static const std::string& CachedName(const std::type_info& typeInformation)
        {
            static std::unordered_map<std::size_t, std::string> cache;
            const std::size_t key = typeInformation.hash_code();

            auto iterator = cache.find(key);

            if (iterator == cache.end())
                iterator = cache.emplace(key, DemangleName(typeInformation.name())).first;

            return iterator->second;
        }

        std::weak_ptr<GameObject> gameObject;

        bool shouldSynchronize = true;

        bool wasAdded = false;
        bool wasRemoved = false;

        friend class boost::serialization::access;

        template <class Archive>
        void serialize(Archive&, const unsigned) {}

        BOOST_DESCRIBE_CLASS(Component, (), (), (), ())
    };
}

BOOST_CLASS_EXPORT(Blaster::Independent::ECS::Component)
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <boost/serialization/access.hpp>
#include <boost/serialization/export.hpp>
#include <boost/preprocessor.hpp>
#include "Independent/ECS/MergeSupport.hpp"

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
            return typeid(*this).name();
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
                iterator = cache.emplace(key, typeInformation.name()).first;

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
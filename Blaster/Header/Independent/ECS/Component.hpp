#pragma once

#include <memory>
#include <string>
#include <boost/serialization/access.hpp>
#include "boost/serialization/export.hpp"

namespace Blaster::Client::Render
{
    class Camera;
}

namespace Blaster::Independent::ECS
{
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

        [[nodiscard]]
        bool WasRemoved() const noexcept
        {
            return wasRemoved;
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
        virtual std::string GetTypeName() const
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

        std::weak_ptr<GameObject> gameObject;

        bool wasAdded = false;
        bool wasRemoved = false;

        friend class boost::serialization::access;

        template <class Archive>
        void serialize(Archive&, const unsigned) {}
    };
}

BOOST_CLASS_EXPORT(Blaster::Independent::ECS::Component)
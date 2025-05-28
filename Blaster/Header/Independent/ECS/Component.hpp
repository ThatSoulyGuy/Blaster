#pragma once

#include <memory>
#include <string>
#include <boost/serialization/access.hpp>

#include "boost/serialization/export.hpp"

namespace Blaster::Independent::ECS
{
    class GameObject;

    class Component
    {

    public:

        virtual ~Component() { }

        virtual void Initialize() { }
        virtual void Update() { }
        virtual void Render() { }

        virtual std::string GetTypeName() const = 0;

        std::shared_ptr<GameObject> GetGameObject() const
        {
            return gameObject.lock();
        }

    private:

        friend class Blaster::Independent::ECS::GameObject;

        std::weak_ptr<GameObject> gameObject;

        friend class boost::serialization::access;

        template <class Archive>
        void serialize(Archive&, const unsigned) { }

    };
}

BOOST_CLASS_EXPORT(Blaster::Independent::ECS::Component)
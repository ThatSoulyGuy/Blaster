#pragma once

#include "Client/Render/Skeleton.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/Component.hpp"

using namespace Blaster::Independent::ECS;

namespace Blaster::Client::Render
{
    class Animator final : public Component
    {

    public:

        static std::shared_ptr<Animator> Create(Skeleton* skeleton)
        {
            std::shared_ptr<Animator> animator(new Animator());

            animator->skeleton = skeleton;

            return animator;
        }

    private:

        Animator() = default;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        }

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        Skeleton* skeleton;

        DESCRIBE_AND_REGISTER(Animator, (Component), (), (), ())

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Animator)
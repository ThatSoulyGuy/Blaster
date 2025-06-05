#pragma once

#include "Client/Core/Window.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Math/Matrix.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Independent::ECS;

namespace Blaster::Client::Render
{
    class Camera final : public Component
    {

    public:

        [[nodiscard]]
        Matrix<float, 4, 4> GetProjectionMatrix() const
        {
            return Matrix<float, 4, 4>::Perspective(fieldOfView * (static_cast<float>(std::numbers::pi) / 180), static_cast<float>(Window::GetInstance().GetDimensions().x()) / static_cast<float>(Window::GetInstance().GetDimensions().y()), nearPlane, farPlane);
        }

        [[nodiscard]]
        Matrix<float, 4, 4> GetViewMatrix() const
        {
            return Matrix<float, 4, 4>::LookAt(GetGameObject()->GetTransform()->GetWorldPosition(), GetGameObject()->GetTransform()->GetWorldPosition() + GetGameObject()->GetTransform()->GetForward(), { 0.0f, 1.0f, 0.0f });
        }

        [[nodiscard]]
        std::string GetTypeName() const override
        {
            return typeid(Camera).name();
        }

        static std::shared_ptr<Camera> Create(const float fieldOfView, const float nearPlane, const float farPlane)
        {
            std::shared_ptr<Camera> result(new Camera());

            result->fieldOfView = fieldOfView;
            result->nearPlane = nearPlane;
            result->farPlane = farPlane;

            return result;
        }

    private:

        Camera() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & boost::serialization::make_nvp("fieldOfView", fieldOfView);
            archive & boost::serialization::make_nvp("nearPlane", nearPlane);
            archive & boost::serialization::make_nvp("farPlane", farPlane);
        }

        float fieldOfView = 0;
        float nearPlane = 0;
        float farPlane = 0;

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Camera)
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

        void SetFieldOfView(float fieldOfView)
        {
            this->fieldOfView = fieldOfView;
        }

        void SetNearFar(float nearPlane, float farPlane)
        {
            this->nearPlane = nearPlane;
            this->farPlane = farPlane;
        }

        void SetAspectOverride(std::optional<float> aspectOverride)
        {
            this->aspectOverride = aspectOverride;
        }

        void SetUpHint(std::optional<Vector<float, 3>> upHint)
        {
            this->upHint = std::move(upHint);
        }

        void SetOrthographic(bool isOrthographic)
        {
            this->isOrthographic = isOrthographic;
        }

        [[nodiscard]]
        Matrix<float, 4, 4> GetProjectionMatrix() const
        {
            if (!isOrthographic)
            {
                const float degToRad = static_cast<float>(std::numbers::pi) / 180.0f;

                float aspect = aspectOverride.has_value() ? aspectOverride.value() : static_cast<float>(Window::GetInstance().GetDimensions().x()) / static_cast<float>(Window::GetInstance().GetDimensions().y());

                return Matrix<float, 4, 4>::Perspective(fieldOfView * degToRad, aspect, nearPlane, farPlane);
            }
            else
                return Matrix<float, 4, 4>::Orthographic(0.0f, Window::GetInstance().GetDimensions().x(), Window::GetInstance().GetDimensions().y(), 0.0f, nearPlane, farPlane);
        }

        [[nodiscard]]
        Matrix<float, 4, 4> GetViewMatrix() const
        {
            const auto transform = GetGameObject()->GetTransform3d();
            const auto eye = transform->GetWorldPosition();

            auto forward = transform->GetForward();

            if (Vector<float, 3>::LengthSquared(forward) <= 1e-8f)
                forward = { 0.0f, 0.0f, -1.0f };
            else
                forward = Vector<float, 3>::Normalize(forward);

            Vector<float, 3> up = upHint.value_or(Vector<float, 3>{ 0.0f, 1.0f, 0.0f });

            auto normalizedUp = Vector<float, 3>::Normalize(up);
            float color = std::abs(Vector<float, 3>::Dot(forward, normalizedUp));

            if (color > 0.999f)
                up = (std::abs(forward.z()) < 0.99f) ? Vector<float, 3>{ 0.0f, 0.0f, 1.0f } : Vector<float, 3>{ 1.0f, 0.0f, 0.0f };

            return Matrix<float, 4, 4>::LookAt(eye, eye + forward, up);
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

        bool isOrthographic = false;

        std::optional<float> aspectOverride{};
        std::optional<Vector<float, 3>> upHint{};

        BOOST_DESCRIBE_CLASS(Blaster::Client::Render::Camera, (Blaster::Independent::ECS::Component), (), (), (fieldOfView, nearPlane, farPlane))

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Camera, 29484)
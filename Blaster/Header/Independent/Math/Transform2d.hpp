#pragma once

#include <numbers>
#include <optional>
#include <vector>
#include <chrono>
#include <functional>
#include "Client/Core/Window.hpp"
#include "Independent/Math/Vector.hpp"
#include "Independent/Math/Matrix.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"

using namespace Blaster::Client::Core;

namespace Blaster::Independent::Math
{
    class Transform2d final : public Component
    {

    public:

        void Initialize() override
        {
            SetShouldSynchronize(false);
        }

        void SetAnchors(const Vector<float, 2>& anchorMin, const Vector<float, 2>& anchorMax)
        {
            this->anchorMin = anchorMin;
            this->anchorMax = anchorMax;
        }

        void SetOffsets(const Vector<float, 2>& offsetMin, const Vector<float, 2>& offsetMax)
        {
            this->offsetMin = offsetMin;
            this->offsetMax = offsetMax;
        }

        void SetPivot(const Vector<float, 2>& pivot)
        {
            this->pivot = pivot;
        }

        void SetScale(const Vector<float, 2>& scale)
        {
            this->scale = scale;
        }

        void SetRotation(float rotation)
        {
            this->rotation = std::fmod(rotation, 360.f);

            if (this->rotation < 0)
                this->rotation += 360.0f;
        }

        [[nodiscard]]
        std::optional<std::weak_ptr<Transform2d>> GetParent() const
        {
            return parent;
        }

        void SetParent(std::shared_ptr<Transform2d> newParent)
        {
            if (!newParent)
                parent = std::nullopt;
            else
                parent = std::make_optional<std::weak_ptr<Transform2d>>(newParent);
        }

        std::pair<Vector<float, 2>, Vector<float, 2>> GetWorldRect() const
        {
            if (parent)
            {
                auto parentRect = parent.value().lock()->GetWorldRect();

                Vector<float, 2> parentSize = parentRect.second - parentRect.first;

                auto [min, max] = ResolveRect(*this, parentSize);

                return { min + parentRect.first, max + parentRect.first };
            }
            else
            {
#ifdef IS_SERVER
                throw std::runtime_error("Cannot use 'Blaster::Client::Core::Window' on server!");
#endif

                Vector<float, 2> canvasSize = { float(Window::GetInstance().GetDimensions().x()), float(Window::GetInstance().GetDimensions().y()) };

                return ResolveRect(*this, canvasSize);
            }
        }

        Matrix<float, 4, 4> GetModelMatrix() const
        {
            auto [min, max] = GetWorldRect();

            Vector<float, 2> position = min + pivot * (max - min);

            Matrix<float, 4, 4> T = Matrix<float, 4, 4>::Translation({ position.x(), position.y(), 0.f });
            Matrix<float, 4, 4> R = Matrix<float, 4, 4>::RotationZ(rotation * std::numbers::pi_v<float> / 180.f);
            Matrix<float, 4, 4> S = Matrix<float, 4, 4>::Scale({ scale.x(), scale.y(), 1.f });

            Vector<float, 2> windowDimensions = { float(Window::GetInstance().GetDimensions().x()), float(Window::GetInstance().GetDimensions().y()) };

            Matrix<float, 4, 4> NDC = Matrix<float, 4, 4>::Scale({ 2.f / windowDimensions.x(), 2.f / windowDimensions.y(), 1.f }) * Matrix<float, 4, 4>::Translation({ -windowDimensions.x() * 0.5f, -windowDimensions.y() * 0.5f, 0.f });

            Matrix<float, 4, 4> local = T * R * S;

            Matrix<float, 4, 4> world = parent ? parent.value().lock()->GetModelMatrix() * local : local;

            return NDC * world;
        }

        static std::shared_ptr<Transform2d> Create(const Vector<float, 2>& anchorMin, const Vector<float, 2>& anchorMax, const Vector<float, 2>& offsetMin, const Vector<float, 2>& offsetMax, const Vector<float, 2>& pivot, const Vector<float, 2>& scale, float rotation)
        {
            std::shared_ptr<Transform2d> result(new Transform2d());
            
            result->anchorMin = anchorMin;
            result->anchorMax = anchorMax;
            result->offsetMin = offsetMin;
            result->offsetMax = offsetMax;
            result->pivot = pivot;
            result->scale = scale;
            result->rotation = rotation;

            return result;
        }

    private:

        Transform2d() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <class Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Component);

            archive & BOOST_SERIALIZATION_NVP(anchorMin);
            archive & BOOST_SERIALIZATION_NVP(anchorMax);
            archive & BOOST_SERIALIZATION_NVP(offsetMin);
            archive & BOOST_SERIALIZATION_NVP(offsetMax);
            archive & BOOST_SERIALIZATION_NVP(pivot);
            archive & BOOST_SERIALIZATION_NVP(scale);
            archive & BOOST_SERIALIZATION_NVP(rotation);
        }

        std::pair<Vector<float, 2>, Vector<float, 2>> ResolveRect(const Transform2d& transform, const Vector<float, 2>& parentSize) const
        {
            Vector<float, 2> min = transform.anchorMin * parentSize + transform.offsetMin;
            Vector<float, 2> max = transform.anchorMax * parentSize + transform.offsetMax;

            return { min, max };
        }

        Vector<float, 2> anchorMin{ 0.0f, 0.0f };
        Vector<float, 2> anchorMax{ 0.0f, 0.0f };
        Vector<float, 2> offsetMin{ 0.0f, 0.0f };
        Vector<float, 2> offsetMax{ 0.0f, 0.0f };

        Vector<float, 2> pivot{ 0.5f, 0.5f };
        Vector<float, 2> scale{ 1.0f, 1.0f };

        float rotation{ 0.0f };

        std::optional<std::weak_ptr<Transform2d>> parent = std::nullopt;

        DESCRIBE_AND_REGISTER(Transform2d, (Component), (), (), (anchorMin, anchorMax, offsetMin, offsetMax, pivot, scale))
    };
}

REGISTER_COMPONENT(Blaster::Independent::Math::Transform2d, 19830)
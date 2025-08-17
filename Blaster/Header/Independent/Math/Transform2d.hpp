#pragma once

#include <numbers>
#include <optional>
#include <vector>
#include <chrono>
#include <functional>
#include "Client/Core/Window.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/Math/Vector.hpp"
#include "Independent/Math/Matrix.hpp"
#include "Independent/ComponentRegistry.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Independent::ECS;

namespace Blaster::Independent::Math
{
    template <typename T> requires std::is_enum_v<T>
    constexpr T operator|(T lhs, T rhs)
    {
        return static_cast<T>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }

    template <typename T> requires std::is_enum_v<T>
    constexpr T operator&(T lhs, T rhs)
    {
        return static_cast<T>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
    }

    template <typename T>
    concept BitType = requires(T a, T b)
    {
        { a & b } -> std::same_as<T>;
        { a | b } -> std::same_as<T>;
    } && std::is_enum_v<T>;

    class Transform2d final : public Component
    {

    public:

        enum class Anchor : std::uint8_t
        {
            NONE = 0,
            LEFT = 1 << 0,
            RIGHT = 1 << 1,
            CENTER_X = 1 << 2,
            TOP = 1 << 3,
            BOTTOM = 1 << 4,
            CENTER_Y = 1 << 5,
            CENTER = (1 << 2) | (1 << 5)
        };

        enum class Stretch : std::uint8_t
        {
            NONE = 0,
            LEFT = 1 << 0,
            RIGHT = 1 << 1,
            TOP = 1 << 2,
            BOTTOM = 1 << 3,
            CENTER = 1 << 4,
            MIDDLE = 1 << 5 
        };

        void Initialize() override
        {
            SetShouldSynchronize(false);

            UpdateLayout(GetReferenceSize());
        }

        void Update() override
        {
            UpdateLayout(GetReferenceSize());
        }

        [[nodiscard]]
        Vector<float, 2> GetPosition() const
        {
            return position;
        }

        void SetPosition(const Vector<float, 2>& position)
        {
            this->position = position;
        }

        [[nodiscard]]
        float GetRotation() const
        {
            return rotation;
        }

        void SetRotation(float rotation)
        {
            this->rotation = std::fmod(rotation, 360.f);

            if (this->rotation < 0)
                this->rotation += 360.0f;
        }

        [[nodiscard]]
        Vector<float, 2> GetDimensions() const
        {
            return dimensions;
        }

        void SetDimensions(const Vector<float, 2>& dimensions)
        {
            this->dimensions = dimensions;
        }

        [[nodiscard]]
        Anchor GetAnchors() const
        {
            return anchors;
        }

        void SetAnchors(const Anchor& anchors)
        {
            this->anchors = anchors;
        }
        
        [[nodiscard]]
        Stretch GetStretch() const 
        {
            return stretch;
        }

        void SetStretch(const Stretch& stretch)
        {
            this->stretch = stretch;
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

        Matrix<float, 4, 4> GetModelMatrix() const
        {
            Matrix<float, 4, 4> t = Matrix<float, 4, 4>::Translation({ transformedPosition.x(), transformedPosition.y(), 0 });
            Matrix<float, 4, 4> r = Matrix<float, 4, 4>::RotationZ(rotation * std::numbers::pi_v<float> / 180.0f);
            Matrix<float, 4, 4> s = Matrix<float, 4, 4>::Scale({ dimensions.x(), dimensions.y(), 1 });

            Matrix<float, 4, 4> local = t * r * s;

            if (!parent || parent.value().expired())
                return local;

            const auto parentPointer = parent.value().lock();

            const Vector<float, 2> invParentSize{ 1.0f / parentPointer->dimensions.x(), 1.0f / parentPointer->dimensions.y() };
            Matrix<float, 4, 4> unscaleParent = Matrix<float, 4, 4>::Scale({ invParentSize.x(), invParentSize.y(), 1 });

            return parentPointer->GetModelMatrix() * unscaleParent * local;
        }

        std::pair<Vector<float, 2>, Vector<float, 2>> GetWorldRect() const
        {
            const Matrix<float, 4, 4> M = GetModelMatrix();

            const std::array<Vector<float, 2>, 4> unit =
            {
                Vector<float, 2>{0.0f, 0.0f},
                Vector<float, 2>{1.0f, 0.0f},
                Vector<float, 2>{1.0f, 1.0f},
                Vector<float, 2>{0.0f, 1.0f}
            };

            Vector<float, 2> mn{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            Vector<float, 2> mx{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

            for (const auto& p : unit)
            {
                const Vector<float, 4> v{ p.x(), p.y(), 0.0f, 1.0f };
                const Vector<float, 4> w = M * v;

                mn[0] = std::min(mn[0], w[0]);  mn[1] = std::min(mn[1], w[1]);
                mx[0] = std::max(mx[0], w[0]);  mx[1] = std::max(mx[1], w[1]);
            }

            return { mn, mx };
        }

        static std::shared_ptr<Transform2d> Create(const Vector<float, 2>& position, float rotation, const Vector<float, 2>& scale)
        {
            std::shared_ptr<Transform2d> result(new Transform2d());
            
            result->position = position;
            result->dimensions = scale;
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

            archive & BOOST_SERIALIZATION_NVP(position);
            archive & BOOST_SERIALIZATION_NVP(rotation);
            archive & BOOST_SERIALIZATION_NVP(dimensions);

            std::uint8_t anchorBits = std::uint8_t(anchors), stretchBits = std::uint8_t(stretch);

            archive & BOOST_SERIALIZATION_NVP(anchorBits);
            archive & BOOST_SERIALIZATION_NVP(stretchBits);

            if constexpr (Archive::is_loading::value)
            {
                anchors = Anchor(anchorBits);
                stretch = Stretch(stretchBits);
            }
        }

        std::pair<Vector<float, 2>, Vector<float, 2>> ResolveRect(const Transform2d& transform, const Vector<float, 2>& parentSize) const
        {
            const float rotationRadians = transform.rotation * std::numbers::pi_v<float> / 180.0f;
            const float cosine = std::cos(rotationRadians);
            const float sine = std::sin(rotationRadians);

            const std::array<Vector<float, 2>, 4> localCorners
            {
                Vector<float, 2>{ 0.0f, 0.0f },
                Vector<float, 2>{ parentSize.x(), 0.0f },
                Vector<float, 2>{ parentSize.x(), parentSize.y() },
                Vector<float, 2>{ 0.0f, parentSize.y() }
            };

            Vector<float, 2> minimumBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            Vector<float, 2> maximumBounds{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

            for (const auto& localCorner : localCorners)
            {
                const Vector<float, 2> scaledCorner{ localCorner.x() * transform.dimensions.x(), localCorner.y() * transform.dimensions.y() };
                const Vector<float, 2> rotatedCorner{ scaledCorner.x() * cosine - scaledCorner.y() * sine, scaledCorner.x() * sine + scaledCorner.y() * cosine };
                const Vector<float, 2> worldCorner{ rotatedCorner.x() + transform.transformedPosition.x(), rotatedCorner.y() + transform.transformedPosition.y() };

                minimumBounds[0] = std::min(minimumBounds[0], worldCorner[0]);
                minimumBounds[1] = std::min(minimumBounds[1], worldCorner[1]);
                maximumBounds[0] = std::max(maximumBounds[0], worldCorner[0]);
                maximumBounds[1] = std::max(maximumBounds[1], worldCorner[1]);
            }

            return { minimumBounds, maximumBounds };
        }

        template <BitType T>
        static bool HasFlag(T flags, T bit)
        {
            return (std::uint8_t(flags) & std::uint8_t(bit)) != 0;
        }

        Vector<float, 2> GetReferenceSize() const
        {
            return parent && !parent.value().expired() ? parent.value().lock()->dimensions : Vector<float, 2>{ float(Window::GetInstance().GetDimensions().x()), float(Window::GetInstance().GetDimensions().y()) };
        }

        void UpdateLayout(const Vector<float, 2>& ref)
        {
            ResolveStretch(ref);
            ResolveAnchor(ref);
        }

        void ResolveStretch(const Vector<float, 2>& ref)
        {
            Vector<float, 2> newSize = dimensions;

            const bool stretchLeft = HasFlag(stretch, Stretch::LEFT) || HasFlag(stretch, Stretch::CENTER);
            const bool stretchRight = HasFlag(stretch, Stretch::RIGHT) || HasFlag(stretch, Stretch::CENTER);

            if (stretchLeft && stretchRight)
                newSize.x() = ref.x() - position.x() * 2.0f;
            else if (stretchLeft)
                newSize.x() = ref.x() * 0.5f - position.x();
            else if (stretchRight)
                newSize.x() = ref.x() * 0.5f - position.x();

            const bool stretchTop = HasFlag(stretch, Stretch::TOP) || HasFlag(stretch, Stretch::MIDDLE);
            const bool stretchBottom = HasFlag(stretch, Stretch::BOTTOM) || HasFlag(stretch, Stretch::MIDDLE);

            if (stretchTop && stretchBottom)
                newSize.y() = ref.y() - position.y() * 2.0f;
            else if (stretchTop)
                newSize.y() = ref.y() * 0.5f - position.y();
            else if (stretchBottom)
                newSize.y() = ref.y() * 0.5f - position.y();

            dimensions = { std::max(1.0f, newSize.x()), std::max(1.0f, newSize.y()) };
        }

        void ResolveAnchor(const Vector<float, 2>& ref)
        {
            const bool stretchedX = HasFlag(stretch, Stretch::LEFT) && (HasFlag(stretch, Stretch::RIGHT) || HasFlag(stretch, Stretch::CENTER));
            const bool stretchedY = HasFlag(stretch, Stretch::TOP) && (HasFlag(stretch, Stretch::BOTTOM) || HasFlag(stretch, Stretch::MIDDLE));

            float anchorX = 0.0f;

            if (stretchedX)
                anchorX = 0.0f;
            else if (HasFlag(anchors, Anchor::LEFT))
                anchorX = 0.0f;
            else if (HasFlag(anchors, Anchor::RIGHT))
                anchorX = ref.x() - dimensions.x();
            else if (HasFlag(anchors, Anchor::CENTER_X))
                anchorX = ref.x() * 0.5f - dimensions.x() * 0.5f;

            float anchorY = 0.0f;

            if (stretchedY)
                anchorY = 0.0f;
            else if (HasFlag(anchors, Anchor::TOP))
                anchorY = 0.0f;
            else if (HasFlag(anchors, Anchor::BOTTOM))
                anchorY = ref.y() - dimensions.y();
            else if (HasFlag(anchors, Anchor::CENTER_Y))
                anchorY = ref.y() * 0.5f - dimensions.y() * 0.5f;

            transformedPosition = { anchorX + position.x(), anchorY + position.y() };
        }

        Anchor anchors{ Anchor::CENTER };
        Stretch stretch{ Stretch::NONE };

        Vector<float, 2> position{ 0.0f, 0.0f };
        Vector<float, 2> transformedPosition{ 0.0f, 0.0f };

        float rotation{ 0.0f };
        Vector<float, 2> dimensions{ 16.0f, 16.0f };

        std::optional<std::weak_ptr<Transform2d>> parent = std::nullopt;

        DESCRIBE_AND_REGISTER(Transform2d, (Component), (), (), (position, rotation, dimensions))
    };
}

REGISTER_COMPONENT(Blaster::Independent::Math::Transform2d, 19830)
#pragma once

#include <memory>
#include <optional>
#include <numbers>
#include <functional>
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/export.hpp>
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/Math/Matrix.hpp"
#include "Independent/Math/Vector.hpp"

using namespace Blaster::Independent::ECS;

namespace Blaster::Independent::Math
{
    class Transform final : public Component
    {

    public:

        void Translate(const Vector<float, 3>& translation)
        {
            localPosition += translation;

            for (auto& function : onPositionUpdated)
                function(localPosition);
        }

        void Rotate(const Vector<float, 3>& rotationDeg)
        {
            localRotation += rotationDeg;

            for (int i = 0; i < 3; ++i)
            {
                localRotation[i] = std::fmod(localRotation[i], 360.0f);

                if (localRotation[i] < 0.0f)
                    localRotation[i] += 360.0f;
            }

            for (auto& function : onRotationUpdated)
                function(localRotation);
        }

        void Scale(const Vector<float, 3>& scale)
        {
            localScale += scale;

            for (auto& function : onScaleUpdated)
                function(localScale);
        }

        [[nodiscard]]
        Vector<float, 3> GetLocalPosition() const
        {
            return localPosition;
        }

        void SetLocalPosition(const Vector<float, 3>& value, const bool update = true)
        {
            localPosition = value;

            if (!update)
                return;

            for (auto& function : onPositionUpdated)
                function(localPosition);
        }

        [[nodiscard]]
        Vector<float, 3> GetLocalRotation() const
        {
            return localRotation;
        }

        void SetLocalRotation(const Vector<float, 3>& value, const bool update = true)
        {
            localRotation = value;

            for (int i = 0; i < 3; ++i)
            {
                localRotation[i] = std::fmod(localRotation[i], 360.0f);

                if (localRotation[i] < 0.0f)
                    localRotation[i] += 360.0f;
            }

            if (!update)
                return;

            for (auto& function : onRotationUpdated)
                function(localRotation);
        }

        [[nodiscard]]
        Vector<float, 3> GetLocalScale() const
        {
            return localScale;
        }

        void SetLocalScale(const Vector<float, 3>& value, bool update = true)
        {
            localScale = value;

            if (!update)
                return;

            for (auto& function : onScaleUpdated)
                function(localScale);
        }

        [[nodiscard]]
        Vector<float, 3> GetWorldPosition() const
        {
            auto M = GetModelMatrix();

            return { M[3][0], M[3][1], M[3][2] };
        }

        [[nodiscard]]
        Vector<float, 3> GetWorldScale() const
        {
            auto M = GetModelMatrix();

            auto length = [](float x, float y, float z)
                {
                    return std::sqrt(x * x + y * y + z * z);
                };

            return
            {
                length(M[0][0], M[0][1], M[0][2]),
                length(M[1][0], M[1][1], M[1][2]),
                length(M[2][0], M[2][1], M[2][2])
            };
        }

        [[nodiscard]]
        Vector<float, 3> GetForward() const
        {
            auto M = GetModelMatrix();

            Vector<float, 3> f = { M[2][0], M[2][1], M[2][2] };

            return Vector<float, 3>::Normalize(f);
        }

        [[nodiscard]]
        Vector<float, 3> GetRight() const
        {
            auto M = GetModelMatrix();

            Vector<float, 3> r = { M[0][0], M[0][1], M[0][2] };

            return Vector<float, 3>::Normalize(r);
        }

        [[nodiscard]]
        Vector<float, 3> GetUp() const
        {
            auto M = GetModelMatrix();

            Vector<float, 3> u = { M[1][0], M[1][1], M[1][2] };

            return Vector<float, 3>::Normalize(u);
        }

        [[nodiscard]]
        Vector<float, 3> GetWorldRotation() const
        {
            auto M = GetModelMatrix();

            Vector<float, 3> sc = GetWorldScale();

            if (std::fabs(sc.x()) > 1e-6f)
                M[0][0] /= sc.x(); M[0][1] /= sc.x(); M[0][2] /= sc.x();

            if (std::fabs(sc.y()) > 1e-6f)
                M[1][0] /= sc.y(); M[1][1] /= sc.y(); M[1][2] /= sc.y();

            if (std::fabs(sc.z()) > 1e-6f)
                M[2][0] /= sc.z(); M[2][1] /= sc.z(); M[2][2] /= sc.z();


            float pitch, yaw, roll;

            if (std::fabs(M[0][0]) < 1e-6f && std::fabs(M[1][0]) < 1e-6f)
            {
                pitch = std::atan2(-M[2][0], M[2][2]);
                yaw = 0.0f;
                roll = std::atan2(-M[1][2], M[1][1]);
            }
            else
            {
                yaw = std::atan2(M[1][0], M[0][0]);
                pitch = std::atan2(-M[2][0], std::sqrt(M[2][1] * M[2][1] + M[2][2] * M[2][2]));
                roll = std::atan2(M[2][1], M[2][2]);
            }

            constexpr float rad2deg = 180.0f / std::numbers::pi_v<float>;

            return Vector<float, 3>{ pitch* rad2deg, yaw* rad2deg, roll* rad2deg };
        }

        void AddOnPositionChangedCallback(const std::function<void(Vector<float, 3>)>& function)
        {
            onPositionUpdated.push_back(function);
        }

        void AddOnRotationChangedCallback(const std::function<void(Vector<float, 3>)>& function)
        {
            onRotationUpdated.push_back(function);
        }

        void AddOnScaleChangedCallback(const std::function<void(Vector<float, 3>)>& function)
        {
            onScaleUpdated.push_back(function);
        }

        [[nodiscard]]
        std::optional<std::weak_ptr<Transform>> GetParent() const
        {
            return parent;
        }

        void SetParent(std::shared_ptr<Transform> newParent)
        {
            if (!newParent)
                parent = std::nullopt;
            else
                parent = std::make_optional<std::weak_ptr<Transform>>(newParent);
        }

        [[nodiscard]]
        Matrix<float, 4, 4> GetModelMatrix() const
        {
            const auto T = Matrix<float, 4, 4>::Translation(localPosition);
            const auto RX = Matrix<float, 4, 4>::RotationX(localRotation.x() * (std::numbers::pi_v<float> / 180.0f));
            const auto RY = Matrix<float, 4, 4>::RotationY(localRotation.y() * (std::numbers::pi_v<float> / 180.0f));
            const auto RZ = Matrix<float, 4, 4>::RotationZ(localRotation.z() * (std::numbers::pi_v<float> / 180.0f));
            const auto S = Matrix<float, 4, 4>::Scale(localScale);

            const Matrix<float, 4, 4> localMatrix = T * RZ * RY * RX * S;

            if (parent.has_value())
            {
                if (const auto parentPtr = parent.value().lock())
                    return parentPtr->GetModelMatrix() * localMatrix;
            }

            return localMatrix;
        }

        [[nodiscard]]
        std::string GetTypeName() const override
        {
            return typeid(Transform).name();
        }

        static std::shared_ptr<Transform> Create(const Vector<float, 3>& position, const Vector<float, 3>& rotation, const Vector<float, 3>& scale)
        {
            std::shared_ptr<Transform> result(new Transform());

            result->localPosition = position;
            result->localRotation = rotation;
            result->localScale = scale;

            return result;
        }

    private:

        Transform() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <class Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & boost::serialization::make_nvp("localPosition", localPosition);
            archive & boost::serialization::make_nvp("localRotation", localRotation);
            archive & boost::serialization::make_nvp("localScale", localScale);
        }

        std::optional<std::weak_ptr<Transform>> parent;

        std::vector<std::function<void(Vector<float, 3>)>> onPositionUpdated;
        std::vector<std::function<void(Vector<float, 3>)>> onRotationUpdated;
        std::vector<std::function<void(Vector<float, 3>)>> onScaleUpdated;

        Vector<float, 3> localPosition = { 0.0f, 0.0f, 0.0f };
        Vector<float, 3> localRotation = { 0.0f, 0.0f, 0.0f };
        Vector<float, 3> localScale = { 1.0f, 1.0f, 1.0f };
    };
}

REGISTER_COMPONENT(Blaster::Independent::Math::Transform)
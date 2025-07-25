#pragma once

#include <unordered_map>
#include <memory>
#include <chrono>
#include "Independent/Math/Transform3d.hpp"
#include "Independent/Utility/Time.hpp"

namespace Blaster::Independent::ECS
{
    class GameObject;
}

namespace Blaster::Independent::ECS::Synchronization
{
    class TranslationBuffer final
    {
    public:

        TranslationBuffer(const TranslationBuffer&) = delete;
        TranslationBuffer(TranslationBuffer&&) = delete;
        TranslationBuffer& operator=(const TranslationBuffer&) = delete;
        TranslationBuffer& operator=(TranslationBuffer&&) = delete;

        void Enqueue(const std::shared_ptr<Blaster::Independent::Math::Transform3d>& transform, const Blaster::Independent::Math::Vector<float, 3>& position, const Blaster::Independent::Math::Vector<float, 3>& rotation, const Blaster::Independent::Math::Vector<float, 3>& scale)
        {
            using Transform3d = Blaster::Independent::Math::Transform3d;

            Entry& entry = entryMap[transform.get()];

            entry.transform = transform;
            entry.startingPosition = transform->GetLocalPosition();
            entry.startingRotation = transform->GetLocalRotation();
            entry.startingScale = transform->GetLocalScale();
            entry.targetPosition = position;
            entry.targetRotation = rotation;
            entry.targetScale = scale;
            entry.progress = 0.0f;
        }

        void Update()
        {
            namespace Math = Blaster::Independent::Math;

            const float snapInterval = 0.10f;
            auto iterator = entryMap.begin();

            while (iterator != entryMap.end())
            {
                Entry& entry = iterator->second;
                std::shared_ptr<Math::Transform3d> transform = entry.transform.lock();

                if (!transform)
                {
                    iterator = entryMap.erase(iterator);
                    continue;
                }

                entry.progress += Time::GetInstance().GetDeltaTime() / snapInterval;

                const float t = std::clamp(entry.progress, 0.0f, 1.0f);

                const auto lerp = [t](const Math::Vector<float, 3>& a, const Math::Vector<float, 3>& b) { return a + (b - a) * t; };

                transform->SetLocalPosition(lerp(entry.startingPosition, entry.targetPosition), false);
                transform->SetLocalRotation(lerp(entry.startingRotation, entry.targetRotation), false);
                transform->SetLocalScale(lerp(entry.startingScale, entry.targetScale), false);

                if (t >= 1.0f)
                    iterator = entryMap.erase(iterator);
                else
                    ++iterator;
            }
        }

        static TranslationBuffer& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<TranslationBuffer>(new TranslationBuffer());
            });

            return *instance;
        }

    private:

        TranslationBuffer() = default;

        struct Entry
        {
            std::weak_ptr<Blaster::Independent::Math::Transform3d> transform;

            Blaster::Independent::Math::Vector<float, 3> startingPosition, startingRotation, startingScale;
            Blaster::Independent::Math::Vector<float, 3> targetPosition, targetRotation, targetScale;

            float progress = 0.0f;
        };

        std::unordered_map<const void*, Entry> entryMap;

        static std::once_flag initializationFlag;
        static std::unique_ptr<TranslationBuffer> instance;

    };
    
    std::once_flag TranslationBuffer::initializationFlag;
    std::unique_ptr<TranslationBuffer> TranslationBuffer::instance;
}

#pragma once

#include <deque>
#include <unordered_map>

#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Math/Vector.hpp"
#include "Independent/Utility/Time.hpp"

using namespace Blaster::Independent::Math;
using namespace Blaster::Independent::Utility;

namespace Blaster::Client::Network
{
    struct KeyFrame
    {
        Vector<float, 3> position;
        float timeStamp;
    };

    class TranslationBuffer final
    {

    public:

        TranslationBuffer(const TranslationBuffer&) = delete;
        TranslationBuffer(TranslationBuffer&&) = delete;
        TranslationBuffer& operator=(const TranslationBuffer&) = delete;
        TranslationBuffer& operator=(TranslationBuffer&&) = delete;

        void Push(const std::string& path, const Vector<float,3> target, const float secondsFromNow)
        {
            const float tNow = Time::GetInstance().GetCurrentTime();

            auto& deque = buffers[path];

            deque.push_back({ target, tNow + secondsFromNow });

            while (deque.size() > 4)
                deque.pop_front();
        }

        void Update()
        {
            const float renderTime = Time::GetInstance().GetCurrentTime() - backlog;

            for (auto iterator = buffers.begin(); iterator != buffers.end();)
            {
                auto& deque = iterator->second;

                if (deque.empty())
                {
                    iterator = buffers.erase(iterator);
                    continue;
                }

                const auto gameObject = Locate(iterator->first);

                if (!gameObject)
                {
                    ++iterator;
                    continue;
                }

                while (deque.size() > 1 && renderTime > deque[1].timeStamp)
                    deque.pop_front();

                const auto& [aPosition, aTimeStamp] = deque.front();

                if (deque.size() == 1 || renderTime <= aTimeStamp)
                {
                    gameObject->GetTransform()->SetLocalPosition(aPosition);

                    ++iterator;

                    continue;
                }

                const auto& [bPosition, bTimeStamp] = deque[1];
                const float span = bTimeStamp - aTimeStamp;
                const float t = span > 1e-4f ? (renderTime - aTimeStamp) / span : 1.0f;

                const auto finalPosition = Vector<float,3>::Lerp(aPosition, bPosition, std::clamp(t, 0.0f, 1.0f));
                gameObject->GetTransform()->SetLocalPosition(finalPosition);

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

        static std::shared_ptr<GameObject> Locate(const std::string& path)
        {
            auto toView = [](auto&& sub)
            {
                return std::string_view(&*sub.begin(), std::ranges::distance(sub));
            };

            auto parts = path | std::views::split('.') | std::views::transform(toView);

            const auto iterator = parts.begin();

            if (iterator == parts.end())
                return nullptr;

            auto current = GameObjectManager::GetInstance().Get(std::string{*iterator});

            if (!current)
                return nullptr;

            for (auto segment : parts | std::views::drop(1))
            {
                current = (*current)->GetChild(std::string{segment});

                if (!current)
                    return nullptr;
            }

            return *current;
        }

        std::unordered_map<std::string, std::deque<KeyFrame>> buffers;

        static constexpr float backlog = 0.20f;

        static std::once_flag initializationFlag;
        static std::unique_ptr<TranslationBuffer> instance;

    };

    std::once_flag TranslationBuffer::initializationFlag;
    std::unique_ptr<TranslationBuffer> TranslationBuffer::instance;
}
#pragma once

#include "Client/Render/Skeleton.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/Utility/Time.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Client::Render
{
    enum class WrapMode { ONCE, LOOP };
    enum class BlendMode { OVERWRITE, ADDITIVE };

    struct ActiveNet
    {
        std::string clipName;

        float time = 0.f;
        float speed = 1.f;
        float weight = 1.f;
        float fadeVel = 0.f;

        WrapMode wrap = WrapMode::LOOP;
        BlendMode blend = BlendMode::OVERWRITE;

        bool operator==(const ActiveNet& other) const
        {
            return OPERATOR_CHECK(clipName, time, speed, weight, fadeVel, wrap, blend);
        }

        bool operator!=(const ActiveNet& other) const
        {
            return !(*this == other);
        }

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & BOOST_SERIALIZATION_NVP(clipName);
            archive & BOOST_SERIALIZATION_NVP(time);
            archive & BOOST_SERIALIZATION_NVP(speed);
            archive & BOOST_SERIALIZATION_NVP(weight);
            archive & BOOST_SERIALIZATION_NVP(fadeVel);
            archive & BOOST_SERIALIZATION_NVP(wrap);
            archive & BOOST_SERIALIZATION_NVP(blend);
        }
    };

    struct Key
    {
        float time;
        Vector<float, 3> translation;
        Vector<float, 3> scale;
        Vector<float, 4> rotation;
    };

    struct Channel
    {
        uint32_t boneId;
        std::vector<Key> keys;
    };

    struct AnimationClip
    {
        std::string name;
        float durationSeconds{ 0.0f };
        float ticksPerSecond{ 25.0f };
        std::vector<Channel> channels;
    };

    class Animator final : public Component
    {

    public:

        Animator(const Animator&) = delete;
        Animator(Animator&&) = delete;
        Animator& operator=(const Animator&) = delete;
        Animator& operator=(Animator&&) = delete;

        void AddClip(AnimationClip clip)
        {
            clips[clip.name] = std::move(clip);

            ResolvePending();
        }

        bool HasClip(const std::string& name) const
        {
            return clips.contains(name);
        }

        void SetSkeleton(Skeleton* skeleton)
        {
            this->skeleton = skeleton;
        }

        void Play(const std::string& name, float fadeSeconds = 0.0f, float speed = 1.0f, WrapMode wrap = WrapMode::LOOP, BlendMode mode = BlendMode::OVERWRITE)
        {
            auto iterator = clips.find(name);

            if (iterator == clips.end())
                return;

            Active instance;

            instance.clip = &iterator->second;
            instance.speed = speed;
            instance.time = 0.0f;
            instance.wrap = wrap;
            instance.blend = mode;
            instance.weight = fadeSeconds <= 0.0f ? 1.0f : 0.0f;
            instance.fadeVelocity = fadeSeconds <= 0.0f ? 0.0f : 1.0f / fadeSeconds;

            if (fadeSeconds > 0.0f)
            {
                for (auto& active : activeList)
                    active.fadeVelocity = -1.0f / fadeSeconds;
            }

            activeList.push_back(instance);

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(Animator));
        }

        bool IsPlaying(const std::string& name)
        {
            if (!clips.contains(name))
                return false;

            for (const auto& active : activeList)
            {
                if (active.clip->name == name)
                    return true;
            }

            return false;
        }

        void Stop(const std::string& name, float fadeSeconds = 0.0f)
        {
            for (auto& active : activeList)
            {
                if (active.clip && active.clip->name == name)
                    active.fadeVelocity = fadeSeconds <= 0.0f ? -1.0f : -1.0f / fadeSeconds;
            }

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(Animator));
        }

        void StopAll(float fadeSeconds = 0.0f)
        {
            for (auto& active : activeList)
                active.fadeVelocity = fadeSeconds <= 0.0f ? -1.0f : -1.0f / fadeSeconds;

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(Animator));
        }

        void SetSpeed(const std::string& name, float speed)
        {
            for (auto& active : activeList)
            {
                if (active.clip && active.clip->name == name)
                    active.speed = speed;
            }

            Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(GetGameObject(), typeid(Animator));
        }

        void Update() override
        {
            const float dt = Time::GetInstance().GetDeltaTime();

            for (auto& active : activeList)
            {
                active.time += active.speed * dt;
                active.weight = std::clamp(active.weight + active.fadeVelocity * dt, 0.0f, 1.0f);

                if (active.clip && active.wrap == WrapMode::LOOP)
                {
                    while (active.time > active.clip->durationSeconds)
                        active.time -= active.clip->durationSeconds;
                }
            }

            activeList.erase(std::remove_if(activeList.begin(), activeList.end(), [](const Active& a) { return a.weight <= 0.0f; }), activeList.end());

            if (activeList.empty())
                return;

            const size_t boneCount = skeleton->bones.size();

            translations.assign(boneCount, Vector<float, 3>{0, 0, 0});
            scales.assign(boneCount, Vector<float, 3>{1, 1, 1});
            rotations.assign(boneCount, Vector<float, 4>{0, 0, 0, 1});
            filled.assign(boneCount, false);

            for (const auto& active : activeList)
            {
                const float w = active.weight;

                if (w <= 0.0f)
                    continue;

                for (const auto& chan : active.clip->channels)
                {
                    const Key samp = SampleChannel(chan, active.time, active.clip->durationSeconds, active.wrap);
                    const uint32_t id = chan.boneId;

                    if (active.blend == BlendMode::ADDITIVE)
                    {
                        translations[id] += samp.translation * w;
                        scales[id] += (samp.scale - Vector<float, 3>{1, 1, 1})* w;
                        rotations[id] = Slerp(rotations[id], samp.rotation, w);
                    }
                    else
                    {
                        if (!filled[id])
                        {
                            translations[id] = samp.translation;
                            scales[id] = samp.scale;
                            rotations[id] = samp.rotation;
                            filled[id] = true;
                        }
                        else
                        {
                            translations[id] = Lerp(translations[id], samp.translation, w);
                            scales[id] = Lerp(scales[id], samp.scale, w);
                            rotations[id] = Slerp(rotations[id], samp.rotation, w);
                        }
                    }
                }
            }

            for (size_t i = 0; i < boneCount; ++i)
            {
                Matrix<float, 4, 4> local;

                if (filled[i])
                    local = Matrix<float, 4, 4>::Translation(translations[i]) * QuaternionToMatrix(rotations[i]) * Matrix<float, 4, 4>::Scale(scales[i]);
                else
                    local = skeleton->bones[i].localBind;

                const uint32_t parentId = skeleton->bones[i].parent;

                if (parentId != std::numeric_limits<uint32_t>::max())
                    skeleton->bones[i].globalAnimated =
                    skeleton->bones[parentId].globalAnimated * local;
                else
                    skeleton->bones[i].globalAnimated = local;
            }
        }

        void OnAfterMerge() override
        {
            ResolvePending();
        }

        static std::shared_ptr<Animator> Create(Skeleton* skeleton)
        {
            std::shared_ptr<Animator> result(new Animator());

            result->skeleton = skeleton;

            return result;
        }

    private:

        Animator() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <class Archive>
        void save(Archive& archive, const unsigned) const
        {
            archive & boost::serialization::base_object<Component>(*this);

            std::vector<ActiveNet> activeNetwork;

            activeNetwork.reserve(activeList.size());

            for (const auto& active : activeList)
            {
                if (!active.clip)
                    continue;

                activeNetwork.push_back({ active.clip->name, active.time, active.speed, active.weight, active.fadeVelocity, active.wrap, active.blend });
            }

            activeNetwork.insert(activeNetwork.end(), pendingNetworkList.begin(), pendingNetworkList.end());

            archive & BOOST_SERIALIZATION_NVP(activeNetwork);
        }

        template <class Archive>
        void load(Archive& archive, const unsigned)
        {
            std::vector<ActiveNet> activeNetworkList;

            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(activeNetworkList);

            activeList.clear();

            for (const auto& activeNet : activeNetworkList)
            {
                auto iterator = clips.find(activeNet.clipName);

                if (iterator == clips.end())
                {
                    pendingNetworkList.push_back(activeNet);
                    continue;
                }

                Active active;

                active.clip = &iterator->second;
                active.time = activeNet.time;
                active.speed = activeNet.speed;
                active.weight = activeNet.weight;
                active.fadeVelocity = activeNet.fadeVel;
                active.wrap = activeNet.wrap;
                active.blend = activeNet.blend;

                activeList.push_back(active);
            }
        }

        BOOST_SERIALIZATION_SPLIT_MEMBER()

        void ResolvePending()
        {
            if (pendingNetworkList.empty())
                return;

            for (auto iterator = pendingNetworkList.begin(); iterator != pendingNetworkList.end(); )
            {
                auto clipIt = clips.find(iterator->clipName);

                if (clipIt == clips.end())
                { 
                    ++iterator;
                    continue;
                }

                Active active;

                active.clip = &clipIt->second;
                active.time = iterator->time;
                active.speed = iterator->speed;
                active.weight = iterator->weight;
                active.fadeVelocity = iterator->fadeVel;
                active.wrap = iterator->wrap;
                active.blend = iterator->blend;

                activeList.push_back(active);
                iterator = pendingNetworkList.erase(iterator);
            }
        }

        struct Active
        {
            const AnimationClip* clip{ nullptr };

            float time{ 0.0f };
            float speed{ 1.0f };
            float weight{ 1.0f };
            float fadeVelocity{ 0.0f };

            WrapMode wrap{ WrapMode::LOOP };
            BlendMode blend{ BlendMode::OVERWRITE };

            bool operator==(const Active& other) const
            {
                return OPERATOR_CHECK(clip, time, speed, weight, fadeVelocity, wrap, blend);
            }

            bool operator!=(const Active& other) const
            {
                return !(*this == other);
            }
        };

        static Vector<float, 4> NormalizeQuat(Vector<float, 4> q)
        {
            const float len = std::sqrt(q.x() * q.x() + q.y() * q.y() + q.z() * q.z() + q.w() * q.w());

            return q / len;
        }

        static Vector<float, 4> Slerp(Vector<float, 4> a, Vector<float, 4> b, float t)
        {
            float dot = a.x() * b.x() + a.y() * b.y() + a.z() * b.z() + a.w() * b.w();

            if (dot < 0.0f)
            {
                b = b * -1.0f;
                dot = -dot;
            }

            const float kEps = 1e-5f;

            if (dot > 1.0f - kEps) 
                return NormalizeQuat(a * (1.0f - t) + b * t);

            const float theta = std::acos(dot);
            const float sinT = std::sin(theta);
            const float w1 = std::sin((1.0f - t) * theta) / sinT;
            const float w2 = std::sin(t * theta) / sinT;

            return NormalizeQuat(a * w1 + b * w2);
        }

        static Vector<float, 3> Lerp(Vector<float, 3> a, Vector<float, 3> b, float t)
        {
            return a + (b - a) * t;
        }

        static Vector<float, 3> Hermite(const Key& k0, const Key& k1, float t)
        {
            return Lerp(k0.translation, k1.translation, t);
        }

        static Vector<float, 4> RotInterp(const Key& k0, const Key& k1, float t)
        {
            return Slerp(k0.rotation, k1.rotation, t);
        }

        static Key SampleChannel(const Channel& chan, float time, float duration, WrapMode wrap)
        {
            if (chan.keys.empty()) return Key{};

            if (time <= chan.keys.front().time)
                return chan.keys.front();

            if (time >= chan.keys.back().time)
            {
                if (wrap == WrapMode::LOOP)
                    time = std::fmod(time, duration);
                else
                    return chan.keys.back();
            }

            auto it = std::upper_bound(chan.keys.begin(), chan.keys.end(), time, [](float t, const Key& k) { return t < k.time; });

            const Key& k1 = *it;
            const Key& k0 = *(it - 1);

            const float span = k1.time - k0.time;
            const float alpha = span > 0.0f ? (time - k0.time) / span : 0.0f;

            Key out;

            out.translation = Hermite(k0, k1, alpha);
            out.scale = Lerp(k0.scale, k1.scale, alpha);
            out.rotation = RotInterp(k0, k1, alpha);

            return out;
        }

        static Matrix<float, 4, 4> QuaternionToMatrix(const Vector<float, 4>& q)
        {
            const float x = q.x(), y = q.y(), z = q.z(), w = q.w();
            const float xx = x * x, yy = y * y, zz = z * z, xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;

            return Matrix<float, 4, 4>
            {
                { 1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0},
                { 2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx),0 },
                { 2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy),0 },
                { 0, 0, 0, 1 }
            };
        }

        Skeleton* skeleton{ nullptr };

        std::unordered_map<std::string, AnimationClip> clips;
        std::vector<Active> activeList;
        std::vector<ActiveNet> pendingNetworkList;

        std::vector<Vector<float, 3>> translations;
        std::vector<Vector<float, 3>> scales;
        std::vector<Vector<float, 4>> rotations;
        std::vector<bool> filled;

        DESCRIBE_AND_REGISTER(Animator, (Component), (), (), (activeList, pendingNetworkList))
    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Animator, 32949)
#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
#include <random>
#include <AL/al.h>
#include <AL/alc.h>
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Utility/AssetPath.hpp"
#include "Independent/Utility/Time.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Utility;

namespace Blaster::Client::Sound
{
	class SoundClip final : public Component
	{

	public:

        struct DistanceModel
        {
            float referenceDistance = 2.0f;
            float maxDistance = 50.0f;
            float rolloff = 1.0f;

            template <typename Archive>
            void serialize(Archive& archive, const unsigned)
            {
                archive & BOOST_SERIALIZATION_NVP(referenceDistance);
                archive & BOOST_SERIALIZATION_NVP(maxDistance);
                archive & BOOST_SERIALIZATION_NVP(rolloff);
            }
        };

        ~SoundClip()
        {
#ifndef IS_SERVER
            for (auto& source : sourceList)
            {
                if (source)
                {
                    alSourceStop(source);
                    alDeleteSources(1, &source);

                    source = 0;
                }
            }

            for (auto& buffer : bufferList)
            {
                if (buffer)
                {
                    alDeleteBuffers(1, &buffer);

                    buffer = 0;
                }
            }
#endif

            generated = false;
        }

		SoundClip(const SoundClip&) = delete;
		SoundClip(SoundClip&&) = delete;
		SoundClip& operator=(const SoundClip&) = delete;
		SoundClip& operator=(SoundClip&&) = delete;

        void Play()
        {
            if (!EnsureReady())
                return;

            if (sourceList.empty())
                return;

            static thread_local std::mt19937 device{ std::random_device{}() };

            std::uniform_int_distribution<int> distribution(0, static_cast<int>(sourceList.size()) - 1);

            const ALuint source = sourceList[distribution(device)];

            alSourceRewind(source);
            alSourcePlay(source);
        }

        void Pause()
        {
            for (const auto& source : sourceList)
            {
                if (source)
                    alSourcePause(source);
            }
        }

        void Stop()
        {
            for (const auto& source : sourceList)
            {
                if (source)
                    alSourceStop(source);
            }
        }

        bool IsPlaying() const
        {
            for (const auto& source : sourceList)
            {
                if (!source)
                    return false;
            
                ALint sourceState = 0;

                alGetSourcei(source, AL_SOURCE_STATE, &sourceState);

                return sourceState == AL_PLAYING;
            }
        }

        [[nodiscard]]
        float GetGain() const
        {
            return gain;
        }

        void SetGain(float gain)
        {
            this->gain = gain;

            for (const auto& source : sourceList)
            {
                if (source)
                    alSourcef(source, AL_GAIN, this->gain);
            }
        }

        [[nodiscard]]
        float GetPitch() const
        {
            return pitch;
        }

        void SetPitch(float pitch)
        {
            this->pitch = pitch;

            for (const auto& source : sourceList)
            {
                if (source)
                    alSourcef(source, AL_PITCH, this->pitch);
            }
        }

        [[nodiscard]]
        float GetLooping() const
        {
            return loop;
        }

        void SetLooping(bool loop)
        {
            this->loop = loop;

            for (const auto& source : sourceList)
            {
                if (source)
                    alSourcei(source, AL_LOOPING, this->loop ? AL_TRUE : AL_FALSE);
            }
        }

        [[nodiscard]]
        float GetSpatial() const
        {
            return spatial;
        }

        void SetSpatial(bool spatial)
        {
            this->spatial = spatial;

            for (const auto& source : sourceList)
            {
                if (source)
                    alSourcei(source, AL_SOURCE_RELATIVE, this->spatial ? AL_FALSE : AL_TRUE);
            }
        }

        [[nodiscard]]
        DistanceModel GetDistanceModel() const
        {
            return distanceModel;
        }

        void SetDistanceModel(DistanceModel distanceModel)
        {
            this->distanceModel = distanceModel;

            for (const auto& source : sourceList)
            {
                if (source)
                {
                    alSourcef(source, AL_REFERENCE_DISTANCE, distanceModel.referenceDistance);
                    alSourcef(source, AL_MAX_DISTANCE, distanceModel.maxDistance);
                    alSourcef(source, AL_ROLLOFF_FACTOR, distanceModel.rolloff);
                }
            }
        }

        [[nodiscard]]
        std::vector<AssetPath> GetPathList() const
        {
            return pathList;
        }

        void Generate()
        {
#ifndef IS_SERVER
            if (generated)
                return;
            if (!OpenALBootstrap::Ensure())
            {
                std::cerr << "[SoundClip] OpenAL device/context init failed.\n";
                return;
            }
            if (pathList.empty())
            {
                std::cerr << "[SoundClip] No paths provided.\n";
                return;
            }

            bufferList.clear();
            sourceList.clear();
            sampleRateList.clear();

            for (const auto& path : pathList)
            {
                ALuint buffer = 0;
                ALsizei rate = 0;

                if (!LoadWavToBuffer(path.GetFullPath(), buffer, rate))
                {
                    std::cerr << "[SoundClip] Failed to load WAV: " << path.GetFullPath() << "\n";
                    continue;
                }

                ALuint source = 0;

                alGenSources(1, &source);
                alSourcei(source, AL_BUFFER, buffer);

                alSourcef(source, AL_GAIN, gain);
                alSourcef(source, AL_PITCH, pitch);
                alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
                alSourcei(source, AL_SOURCE_RELATIVE, spatial ? AL_FALSE : AL_TRUE);
                alSourcef(source, AL_REFERENCE_DISTANCE, distanceModel.referenceDistance);
                alSourcef(source, AL_MAX_DISTANCE, distanceModel.maxDistance);
                alSourcef(source, AL_ROLLOFF_FACTOR, distanceModel.rolloff);

                bufferList.push_back(buffer);
                sourceList.push_back(source);
                sampleRateList.push_back(rate);
            }

            if (sourceList.empty())
                return;

            UpdateSourceTransform(true);

            generated = true;

            if (autoplay)
                Play();
#endif
        }

        void Update() override
        {
            bool sourceActive = false;

            for (const auto& source : sourceList)
            {
                if (source)
                    sourceActive = true;
            }

            if (!sourceActive)
                return;

            UpdateSourceTransform(false);
        }

        static std::shared_ptr<SoundClip> Create(const std::vector<AssetPath>& pathList, bool loop = false, bool spatial = true, bool autoplay = false, float gain = 1.0f, float pitch = 1.0f, DistanceModel distanceModel = { 2.0f, 50.0f, 1.0f })
        {
            std::shared_ptr<SoundClip> result(new SoundClip());

            result->pathList = pathList;
            result->loop = loop;
            result->spatial = spatial;
            result->autoplay = autoplay;
            result->gain = gain;
            result->pitch = pitch;
            result->distanceModel = distanceModel;

            return result;
        }

	private:

		SoundClip() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(pathList);
            archive & BOOST_SERIALIZATION_NVP(loop);
            archive & BOOST_SERIALIZATION_NVP(spatial);
            archive & BOOST_SERIALIZATION_NVP(autoplay);
            archive & BOOST_SERIALIZATION_NVP(gain);
            archive & BOOST_SERIALIZATION_NVP(pitch);
            archive & BOOST_SERIALIZATION_NVP(distanceModel);
        }

        struct OpenALBootstrap
        {
            static inline ALCdevice* device = nullptr;
            static inline ALCcontext* context = nullptr;

            static bool Ensure()
            {
                if (context)
                    return true;

                device = alcOpenDevice(nullptr);

                if (!device)
                    return false;

                context = alcCreateContext(device, nullptr);

                if (!context)
                    return false;

                if (!alcMakeContextCurrent(context))
                    return false;

                alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
                return true;
            }

            static void Shutdown()
            {
                if (context)
                {
                    alcMakeContextCurrent(nullptr);
                    alcDestroyContext(context);
                    
                    context = nullptr;
                }

                if (device)
                {
                    alcCloseDevice(device);
                    device = nullptr;
                }
            }
        };

        static void ALPrintError(const char* where)
        {
            if (const ALenum error = alGetError(); error != AL_NO_ERROR)
                std::cerr << "[OpenAL] Error 0x" << std::hex << error << " at " << where << std::dec << "\n";
        }

        bool EnsureReady()
        {
#ifndef IS_SERVER
            if (!generated)
                Generate();

            return !sourceList.empty();
#else
            return false;
#endif
        }

        static bool LoadWavToBuffer(const std::string& filePath, ALuint& outBuffer, ALsizei& outRate)
        {
            std::ifstream file(filePath, std::ios::binary);

            if (!file) return
                false;

            auto readU32 = [&](uint32_t& v) { file.read(reinterpret_cast<char*>(&v), 4); };
            auto readU16 = [&](uint16_t& v) { file.read(reinterpret_cast<char*>(&v), 2); };
            auto readTag = [&](char tag[4]) { file.read(tag, 4); };

            char riff[4];
            readTag(riff);
            uint32_t riffSize = 0;

            readU32(riffSize);
            char wave[4];
            readTag(wave);

            if (std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE")
                return false;

            uint16_t audioFormat = 0;
            uint16_t numChannels = 0;
            uint32_t sampleRate = 0;
            uint16_t bitsPerSample = 0;

            std::vector<char> dataChunk;

            while (file && (!audioFormat || dataChunk.empty()))
            {
                char id[4];
                readTag(id);

                uint32_t size = 0;
                readU32(size);

                const std::string sid(id, 4);

                if (sid == "fmt ")
                {
                    readU16(audioFormat);
                    readU16(numChannels);
                    readU32(sampleRate);

                    uint32_t byteRate = 0; readU32(byteRate);
                    uint16_t blockAlign = 0; readU16(blockAlign);

                    readU16(bitsPerSample);

                    if (size > 16)
                        file.seekg(static_cast<std::streamoff>(size - 16), std::ios::cur);
                }
                else if (sid == "data")
                {
                    dataChunk.resize(size);

                    if (size)
                        file.read(dataChunk.data(), size);
                }
                else
                    file.seekg(static_cast<std::streamoff>(size), std::ios::cur);
            }

            if (!audioFormat || dataChunk.empty() || (audioFormat != 1))
                return false;

            ALenum format = 0;

            if (numChannels == 1 && bitsPerSample == 8)
                format = AL_FORMAT_MONO8;
            else if (numChannels == 1 && bitsPerSample == 16)
                format = AL_FORMAT_MONO16;
            else if (numChannels == 2 && bitsPerSample == 8)
                format = AL_FORMAT_STEREO8;
            else if (numChannels == 2 && bitsPerSample == 16)
                format = AL_FORMAT_STEREO16;
            else
            {
                std::cerr << "[SoundClip] Unsupported WAV format: " << numChannels << "ch, " << bitsPerSample << "bpp\n";
                return false;
            }

            alGenBuffers(1, &outBuffer);
            alBufferData(outBuffer, format, dataChunk.data(), static_cast<ALsizei>(dataChunk.size()), static_cast<ALsizei>(sampleRate));

            outRate = static_cast<ALsizei>(sampleRate);

            ALPrintError("alBufferData");

            return true;
        }

        void UpdateSourceTransform(bool forceInit)
        {
            const auto worldPosition = GetGameObject()->GetTransform3d()->GetWorldPosition();

            for (const auto& source : sourceList)
            {
                if (!source)
                    return;

                alSource3f(source, AL_POSITION, worldPosition.x(), worldPosition.y(), worldPosition.z());

                const float deltaTime = Blaster::Independent::Utility::Time::GetInstance().GetDeltaTime();

                if (!forceInit && deltaTime > 0.00001f)
                {
                    const float vx = (worldPosition.x() - lastPosition.x()) / deltaTime;
                    const float vy = (worldPosition.y() - lastPosition.y()) / deltaTime;
                    const float vz = (worldPosition.z() - lastPosition.z()) / deltaTime;

                    alSource3f(source, AL_VELOCITY, vx, vy, vz);
                }
                else
                    alSource3f(source, AL_VELOCITY, 0.f, 0.f, 0.f);
            }

            lastPosition = worldPosition;
        }

		std::vector<AssetPath> pathList;

        std::vector<ALuint> bufferList;
        std::vector<ALuint> sourceList;
        std::vector<ALsizei> sampleRateList;

        bool generated = false;
        bool loop = false;
        bool spatial = true;
        bool autoplay = false;
        float gain = 1.0f;
        float pitch = 1.0f;

        DistanceModel distanceModel;

        Vector<float, 3> lastPosition;

        DESCRIBE_AND_REGISTER(SoundClip, (Component), (), (), (pathList, loop, spatial, autoplay, gain, pitch, distanceModel))
	};
}

REGISTER_COMPONENT(Blaster::Client::Sound::SoundClip, 92819)
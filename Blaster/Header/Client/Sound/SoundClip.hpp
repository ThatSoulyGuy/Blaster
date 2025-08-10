#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
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

        ~SoundClip()
        {
#ifndef IS_SERVER
            if (source)
            {
                alSourceStop(source);
                alDeleteSources(1, &source);

                source = 0;
            }

            if (buffer)
            {
                alDeleteBuffers(1, &buffer);

                buffer = 0;
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

            alSourcePlay(source);
        }

        void Pause()
        {
            if (source)
                alSourcePause(source);
        }

        void Stop()
        {
            if (source)
                alSourceStop(source);
        }

        bool IsPlaying() const
        {
            if (!source)
                return false;

            ALint s = 0;

            alGetSourcei(source, AL_SOURCE_STATE, &s);

            return s == AL_PLAYING;
        }

        void SetGain(float g)
        {
            gain = g;

            if (source)
                alSourcef(source, AL_GAIN, gain);
        }

        void SetPitch(float p)
        {
            pitch = p;

            if (source)
                alSourcef(source, AL_PITCH, pitch);
        }

        void SetLooping(bool l)
        {
            loop = l;

            if (source)
                alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
        }

        void SetSpatial(bool s)
        {
            spatial = s;

            if (source)
                alSourcei(source, AL_SOURCE_RELATIVE, spatial ? AL_FALSE : AL_TRUE);
        }

        void SetDistanceModel(float refDistance, float maxDistance, float rolloff)
        {
            refDistance = refDistance;
            maxDistance = maxDistance;
            rolloff = rolloff;

            if (source)
            {
                alSourcef(source, AL_REFERENCE_DISTANCE, refDistance);
                alSourcef(source, AL_MAX_DISTANCE, maxDistance);
                alSourcef(source, AL_ROLLOFF_FACTOR, rolloff);
            }
        }

        AssetPath GetPath() const
        {
            return path;
        }

        void Generate()
        {
#ifndef IS_SERVER
            if (generated)
                return;

            generated = true;

            if (!OpenALBootstrap::Ensure())
            {
                std::cerr << "[SoundClip] OpenAL device/context init failed.\n";
                return;
            }

            if (!LoadWavToBuffer(path.GetFullPath(), buffer, sampleRate))
            {
                std::cerr << "[SoundClip] Failed to load WAV: " << path.GetFullPath() << "\n";
                return;
            }

            alGenSources(1, &source);
            alSourcei(source, AL_BUFFER, buffer);

            alSourcef(source, AL_GAIN, gain);
            alSourcef(source, AL_PITCH, pitch);
            alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
            alSourcei(source, AL_SOURCE_RELATIVE, spatial ? AL_FALSE : AL_TRUE);
            alSourcef(source, AL_REFERENCE_DISTANCE, refDistance);
            alSourcef(source, AL_MAX_DISTANCE, maxDistance);
            alSourcef(source, AL_ROLLOFF_FACTOR, rolloff);

            UpdateSourceTransform(true);

            if (autoplay)
                alSourcePlay(source);
#endif
        }

        void Update() override
        {
            if (!source)
                return;

            UpdateSourceTransform(false);
        }

        static std::shared_ptr<SoundClip> Create(const AssetPath& path, bool loop = false, bool spatial = true, bool autoplay = false, float gain = 1.0f, float pitch = 1.0f, float refDist = 2.0f, float maxDist = 50.0f, float rolloff = 1.0f)
        {
            std::shared_ptr<SoundClip> result(new SoundClip());

            result->path = path;
            result->loop = loop;
            result->spatial = spatial;
            result->autoplay = autoplay;
            result->gain = gain;
            result->pitch = pitch;
            result->refDistance = refDist;
            result->maxDistance = maxDist;
            result->rolloff = rolloff;

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

            archive & BOOST_SERIALIZATION_NVP(path);
            archive & BOOST_SERIALIZATION_NVP(loop);
            archive & BOOST_SERIALIZATION_NVP(spatial);
            archive & BOOST_SERIALIZATION_NVP(autoplay);
            archive & BOOST_SERIALIZATION_NVP(gain);
            archive & BOOST_SERIALIZATION_NVP(pitch);
            archive & BOOST_SERIALIZATION_NVP(refDistance);
            archive & BOOST_SERIALIZATION_NVP(maxDistance);
            archive & BOOST_SERIALIZATION_NVP(rolloff);
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
            const ALenum err = alGetError();

            if (err != AL_NO_ERROR)
                std::cerr << "[OpenAL] Error 0x" << std::hex << err << " at " << where << std::dec << "\n";
        }

        bool EnsureReady()
        {
            if (!generated)
                Generate();

            return source != 0;
        }

        static bool LoadWavToBuffer(const std::string& file, ALuint& outBuffer, ALsizei& outRate)
        {
            std::ifstream f(file, std::ios::binary);

            if (!f) return
                false;

            auto readU32 = [&](uint32_t& v) { f.read(reinterpret_cast<char*>(&v), 4); };
            auto readU16 = [&](uint16_t& v) { f.read(reinterpret_cast<char*>(&v), 2); };
            auto readTag = [&](char tag[4]) { f.read(tag, 4); };

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

            while (f && (!audioFormat || dataChunk.empty()))
            {
                char id[4]; readTag(id);
                uint32_t size = 0; readU32(size);
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
                        f.seekg(static_cast<std::streamoff>(size - 16), std::ios::cur);
                }
                else if (sid == "data")
                {
                    dataChunk.resize(size);

                    if (size)
                        f.read(dataChunk.data(), size);
                }
                else
                    f.seekg(static_cast<std::streamoff>(size), std::ios::cur);
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
            if (!source)
                return;

            const auto p = GetGameObject()->GetTransform3d()->GetWorldPosition();

            alSource3f(source, AL_POSITION, p.x(), p.y(), p.z());

            const float dt = Blaster::Independent::Utility::Time::GetInstance().GetDeltaTime();
            if (!forceInit && dt > 0.00001f)
            {
                const float vx = (p.x() - lastPosX) / dt;
                const float vy = (p.y() - lastPosY) / dt;
                const float vz = (p.z() - lastPosZ) / dt;
                alSource3f(source, AL_VELOCITY, vx, vy, vz);
            }
            else
            {
                alSource3f(source, AL_VELOCITY, 0.f, 0.f, 0.f);
            }

            lastPosX = p.x(); lastPosY = p.y(); lastPosZ = p.z();
        }

		AssetPath path;

        ALuint buffer = 0;
        ALuint source = 0;
        ALsizei sampleRate = 0;

        bool generated = false;
        bool loop = false;
        bool spatial = true;
        bool autoplay = false;
        float gain = 1.0f;
        float pitch = 1.0f;

        float refDistance = 2.0f;
        float maxDistance = 50.0f;
        float rolloff = 1.0f;

        float lastPosX = 0.f, lastPosY = 0.f, lastPosZ = 0.f;

        DESCRIBE_AND_REGISTER(SoundClip, (Component), (), (), (path, loop, spatial, autoplay, gain, pitch, refDistance, maxDistance, rolloff))
	};
}

REGISTER_COMPONENT(Blaster::Client::Sound::SoundClip, 92819)
#pragma once

#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <span>
#include <vector>
#include "Independent/Utility/TypeRegistrar.hpp"

namespace Blaster::Independent::Network
{
    using TcpProtocol = boost::asio::ip::tcp;
    using ErrorCode = boost::system::error_code;
    using NetworkId = std::uint32_t;

    enum class PacketType : std::uint16_t
    {
        S2C_RequestStringId = 1,
        C2S_StringId = 2,
        S2C_AssignNetworkId = 3,
        S2C_Snapshot = 4,
        C2S_Snapshot = 5,
        S2C_TranslateTo = 6,
        C2S_Rigidbody_Impulse = 7,
        C2S_Rigidbody_SetVelocity = 8,
        C2S_Rigidbody_SetTransform = 9,
        C2S_CharacterController_Input
    };

    struct PacketHeader
    {
        PacketType type;

        std::uint32_t size;
        std::uint32_t from;
        std::uint64_t sequence;
    };

    using DecodeFunction = std::any(*)(std::span<const std::uint8_t>);

    class ConversionRegistry
    {

    public:


        static void Register(std::uint64_t typeHash, DecodeFunction function)
        {
            GetMap()[typeHash] = function;
        }

        static std::any Decode(std::uint64_t typeHash, std::span<const std::uint8_t> bytes)
        {
            const auto iterator = GetMap().find(typeHash);
                
            return iterator != GetMap().end() ? iterator->second(bytes) : std::any();
        }
        
    private:

        static std::unordered_map<std::uint64_t, DecodeFunction>& GetMap()
        {
            static std::unordered_map<std::uint64_t, DecodeFunction> map;
                
            return map;
        }
    };

    template <typename Derived, typename Type>
    struct DataConversionBase
    {
        inline static constexpr std::uint64_t TypeHash = Blaster::Independent::Utility::TypeRegistrar::GetTypeId<Type>();

    private:

        static std::any Bridge(std::span<const std::uint8_t> bytes)
        {
            return Derived::Decode(bytes);
        }

#ifndef _MSC_VER
        inline static const bool registered [[gnu::used]] = []()
        {
            ConversionRegistry::Register(TypeHash, &Bridge);
            return true;
        }();
#else
        inline static const bool registered = []()
        {
            ConversionRegistry::Register(TypeHash, &Bridge);
            return true;
        }();
#endif
    };

    template <typename T>
    struct DataConversion;

    template <typename... Args>
    concept DataConvertible = (requires(Args&&... values, std::vector<std::uint8_t>& buffer)
    {
        (DataConversion<std::decay_t<Args>>::Encode(values, buffer), ...);
    });

    class CommonNetwork final
    {

    public:

        CommonNetwork(const CommonNetwork&) = delete;
        CommonNetwork(CommonNetwork&&) = delete;
        CommonNetwork& operator=(const CommonNetwork&) = delete;
        CommonNetwork& operator=(CommonNetwork&&) = delete;

        template <typename... Args> requires DataConvertible<Args...>
        static std::span<std::uint8_t> AssembleData(Args&&... arguments)
        {
            static thread_local std::vector<std::uint8_t> internalBuffer;

            internalBuffer.clear();
            AppendElement(internalBuffer, std::forward<Args>(arguments)...);

            return { internalBuffer.data(), internalBuffer.size() };
        }

        static std::vector<std::any> DisassembleData(std::span<std::uint8_t> data)
        {
            std::vector<std::any> result;
            std::size_t offset = 0;

            while (offset + sizeof(std::uint64_t) * 2 <= data.size())
            {
                const auto typeHash = ReadTrivial<std::uint64_t>(data, offset);
                const auto byteCount = ReadTrivial<std::uint64_t>(data, offset);

                assert(offset + byteCount <= data.size());

                const std::span<const std::uint8_t> bytes(data.data() + offset, static_cast<std::size_t>(byteCount));

                offset += byteCount;
                result.emplace_back(ConversionRegistry::Decode(typeHash, bytes));
            }

            return result;
        }

        template <typename... Args> requires DataConvertible<Args...>
        static std::vector<std::uint8_t> BuildPacket(const PacketType type, const NetworkId from, Args&&... args)
        {
            static std::atomic<std::uint64_t> sequenceGenerator = 0;
            const std::uint64_t seq = ++sequenceGenerator;

            std::span<const std::uint8_t> payload = AssembleData(std::forward<Args>(args)...);

            const PacketHeader header{ type, static_cast<std::uint32_t>(payload.size()), from, seq };

            std::vector<std::uint8_t> buffer(sizeof header + payload.size());

            std::memcpy(buffer.data(), &header, sizeof header);
            std::memcpy(buffer.data() + sizeof header, payload.data(), payload.size());

            return buffer;
        }

        static void WriteRaw(std::vector<std::uint8_t>& buffer, const void* data, std::size_t byteCount)
        {
            const auto* source = static_cast<const std::uint8_t*>(data);
            buffer.insert(buffer.end(), source, source + byteCount);
        }

        template <typename Trivial>
        static void WriteTrivial(std::vector<std::uint8_t>& buffer, const Trivial& value)
        {
            static_assert(std::is_trivially_copyable_v<Trivial>);

            WriteRaw(buffer, &value, sizeof(Trivial));
        }

        template <typename Trivial>
        static Trivial ReadTrivial(std::span<const std::uint8_t> bytes, std::size_t& offset)
        {
            assert(offset + sizeof(Trivial) <= bytes.size());

            Trivial result;

            std::memcpy(&result, bytes.data() + offset, sizeof(Trivial));

            offset += sizeof(Trivial);

            return result;
        }

        static void ReadRaw(std::span<const std::uint8_t> source, std::size_t& offset, void* destination, std::size_t byteCount)
        {
            assert(offset + byteCount <= source.size());
            std::memcpy(destination, source.data() + offset, byteCount);

            offset += byteCount;
        }

        static void EncodeString(std::vector<std::uint8_t>& buf, const std::string& s)
        {
            const auto len = static_cast<std::uint32_t>(s.size());

            CommonNetwork::WriteTrivial(buf, len);
            CommonNetwork::WriteRaw(buf, s.data(), len);
        }

        static std::string DecodeString(std::span<const std::uint8_t> src, std::size_t& offset)
        {
            const auto len = CommonNetwork::ReadTrivial<std::uint32_t>(src, offset);

            std::string out(reinterpret_cast<const char*>(src.data() + offset), len);
            offset += len;

            return out;
        }

        static void EncodeBlob(std::vector<std::uint8_t>& buf, const std::vector<std::uint8_t>& blob)
        {
            const auto len = static_cast<std::uint32_t>(blob.size());

            CommonNetwork::WriteTrivial(buf, len);
            CommonNetwork::WriteRaw(buf, blob.data(), len);
        }

        static std::vector<std::uint8_t> DecodeBlob(std::span<const std::uint8_t> src, std::size_t& offset)
        {
            const auto len = CommonNetwork::ReadTrivial<std::uint32_t>(src, offset);

            std::vector<std::uint8_t> out(len);
            CommonNetwork::ReadRaw(src, offset, out.data(), len);

            return out;
        }

        template <typename PtrT>
        static std::vector<std::uint8_t> SerializePointerToBlob(const PtrT& ptr)
        {
            std::ostringstream stream;
            boost::archive::text_oarchive archive(stream);

            archive << ptr;

            const std::string& txt = stream.str();

            return { txt.begin(), txt.end() };
        }

    private:

        CommonNetwork() = default;

        static void AppendElement(const std::vector<std::uint8_t>& buffer)
        {
            (void)buffer;
        }

        template <typename First, typename... Rest>
        static void AppendElement(std::vector<std::uint8_t>& buffer, First&& firstValue, Rest&&... remainingValues)
        {
            using Decayed = std::decay_t<First>;

            const std::uint64_t typeHash = DataConversion<Decayed>::TypeHash;
            const std::size_t headerIndex = buffer.size();

            buffer.resize(buffer.size() + sizeof(std::uint64_t) * 2);

            const std::size_t dataStart = buffer.size();

            DataConversion<Decayed>::Encode(firstValue, buffer);

            const auto byteCount = static_cast<std::uint64_t>(buffer.size() - dataStart);

            std::memcpy(buffer.data() + headerIndex, &typeHash, sizeof(std::uint64_t));
            std::memcpy(buffer.data() + headerIndex + sizeof(std::uint64_t), &byteCount, sizeof(std::uint64_t));

            AppendElement(buffer, std::forward<Rest>(remainingValues)...);
        }

    };
}

template <>
struct Blaster::Independent::Network::DataConversion<std::int32_t> : Blaster::Independent::Network::DataConversionBase<DataConversion<std::int32_t>, std::int32_t>
{
    using Type = std::int32_t;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::WriteTrivial(buffer, value);
    }

    static std::any Decode(const std::span<const std::uint8_t> bytes)
    {
        assert(bytes.size() == sizeof(Type));

        Type value;
        std::memcpy(&value, bytes.data(), sizeof(Type));

        return value;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<std::uint32_t> : Blaster::Independent::Network::DataConversionBase<DataConversion<std::uint32_t>, std::uint32_t>
{
    using Type = std::uint32_t;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::WriteTrivial(buffer, value);
    }

    static std::any Decode(const std::span<const std::uint8_t> bytes)
    {
        assert(bytes.size() == sizeof(Type));

        Type value;
        std::memcpy(&value, bytes.data(), sizeof(Type));

        return value;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<float> : Blaster::Independent::Network::DataConversionBase<DataConversion<float>, float>
{
    using Type = float;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::WriteTrivial(buffer, value);
    }

    static std::any Decode(const std::span<const std::uint8_t> bytes)
    {
        assert(bytes.size() == sizeof(Type));
        Type value;

        std::memcpy(&value, bytes.data(), sizeof(Type));

        return value;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<double> : Blaster::Independent::Network::DataConversionBase<DataConversion<double>, double>
{
    using Type = double;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::WriteTrivial(buffer, value);
    }

    static std::any Decode(const std::span<const std::uint8_t> bytes)
    {
        assert(bytes.size() == sizeof(Type));

        Type value;
        std::memcpy(&value, bytes.data(), sizeof(Type));

        return value;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<std::string> : Blaster::Independent::Network::DataConversionBase<DataConversion<std::string>, std::string>
{
    using Type = std::string;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        const auto length = static_cast<std::uint64_t>(value.size());

        CommonNetwork::WriteTrivial(buffer, length);
        CommonNetwork::WriteRaw(buffer, value.data(), length);
    }

    static std::any Decode(const std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;
        const auto length = CommonNetwork::ReadTrivial<std::uint64_t>(bytes, offset);

        assert(offset + length <= bytes.size());

        std::string result(reinterpret_cast<const char*>(bytes.data() + offset), static_cast<std::size_t>(length));

        return result;
    }
};
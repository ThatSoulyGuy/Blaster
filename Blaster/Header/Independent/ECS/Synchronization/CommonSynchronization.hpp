#pragma once

#include "Independent/Network/CommonNetwork.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Independent::ECS::Synchronization
{
	enum class OpCode : std::uint8_t
	{
		Create = 1,
		Destroy = 2,
		AddComponent = 3,
		RemoveComponent = 4,
		SetField = 5
	};

    struct OpCreate
    {
        static constexpr OpCode Code = OpCode::Create;

        std::string path;
        std::string className;
    };

    struct OpDestroy
    {
        static constexpr OpCode Code = OpCode::Destroy;

        std::string path;
    };

    struct OpAddComponent
    {
        static constexpr OpCode Code = OpCode::AddComponent;

        std::string path;
        std::string componentType;
        std::vector<std::uint8_t> blob;
    };

    struct OpRemoveComponent
    {
        static constexpr OpCode Code = OpCode::RemoveComponent;

        std::string path;
        std::string componentType;
    };

    struct OpSetField
    {
        static constexpr OpCode Code = OpCode::SetField;

        std::string path;
        std::string componentType;
        std::string field;
        std::vector<std::uint8_t> blob;
    };

#if defined(_MSC_VER)
#pragma pack(push, 1)
    struct SnapshotHeader
    {
        std::uint64_t sequence;
        std::uint32_t operationCount;
    };
#pragma pack(pop)
#elif defined(__clang__) || defined(__GNUC__)
    struct __attribute__((packed)) SnapshotHeader
    {
        std::uint64_t sequence;
        std::uint32_t opCount;
    };
#else
#error "Unknown compiler – please add packing directives"
#endif

    static_assert(sizeof(SnapshotHeader) == 12, "SnapshotHeader must be 12 bytes");

    struct Snapshot
    {
        SnapshotHeader header;
        std::vector<std::uint8_t> operationBlob;
    };
}

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::SnapshotHeader> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::SnapshotHeader>, Blaster::Independent::ECS::Synchronization::SnapshotHeader>
{
    using Type = Blaster::Independent::ECS::Synchronization::SnapshotHeader;

    static void Encode(const Type& value, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::WriteTrivial(buffer, value.sequence);
        CommonNetwork::WriteTrivial(buffer, value.operationCount);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type header = {};

        header.sequence = CommonNetwork::ReadTrivial<std::uint64_t>(bytes, offset);
        header.operationCount = CommonNetwork::ReadTrivial<std::uint32_t>(bytes, offset);

        return header;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::Snapshot> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::Snapshot>, Blaster::Independent::ECS::Synchronization::Snapshot>
{
    using Type = Blaster::Independent::ECS::Synchronization::Snapshot;

    static void Encode(const Type& type, std::vector<std::uint8_t>& buffer)
    {
        Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::SnapshotHeader>::Encode(type.header, buffer);

        CommonNetwork::WriteRaw(buffer, type.operationBlob.data(), type.operationBlob.size());
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Blaster::Independent::ECS::Synchronization::Snapshot result;

        result.header = std::any_cast<Blaster::Independent::ECS::Synchronization::SnapshotHeader>(Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::SnapshotHeader>::Decode(bytes.subspan(offset, sizeof(Blaster::Independent::ECS::Synchronization::SnapshotHeader))));
        offset += sizeof(Blaster::Independent::ECS::Synchronization::SnapshotHeader);

        std::vector<std::uint8_t> tmp(bytes.begin() + offset, bytes.end());
        result.operationBlob.swap(tmp);

        return result;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpCreate> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpCreate>, Blaster::Independent::ECS::Synchronization::OpCreate>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpCreate;

    static void Encode(const Type& type, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, type.path);
        CommonNetwork::EncodeString(buffer, type.className);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type out;

        out.path = CommonNetwork::DecodeString(bytes, offset);
        out.className = CommonNetwork::DecodeString(bytes, offset);

        return out;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpDestroy> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpDestroy>, Blaster::Independent::ECS::Synchronization::OpDestroy>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpDestroy;

    static void Encode(const Type& type, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, type.path);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        return Type{ CommonNetwork::DecodeString(bytes, offset) };
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpAddComponent> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpAddComponent>, Blaster::Independent::ECS::Synchronization::OpAddComponent>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpAddComponent;

    static void Encode(const Type& type, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, type.path);
        CommonNetwork::EncodeString(buffer, type.componentType);

        const std::uint32_t length = static_cast<std::uint32_t>(type.blob.size());

        CommonNetwork::WriteTrivial(buffer, length);
        CommonNetwork::WriteRaw(buffer, type.blob.data(), length);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type result;

        result.path = CommonNetwork::DecodeString(bytes, offset);
        result.componentType = CommonNetwork::DecodeString(bytes, offset);

        const std::uint32_t length = CommonNetwork::ReadTrivial<std::uint32_t>(bytes, offset);

        result.blob.assign(bytes.begin() + offset, bytes.begin() + offset + length);
        offset += length;

        return result;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpRemoveComponent> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpRemoveComponent>, Blaster::Independent::ECS::Synchronization::OpRemoveComponent>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpRemoveComponent;

    static void Encode(const Type& type, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, type.path);
        CommonNetwork::EncodeString(buffer, type.componentType);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type result;

        result.path = CommonNetwork::DecodeString(bytes, offset);
        result.componentType = CommonNetwork::DecodeString(bytes, offset);

        return result;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpSetField> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpSetField>, Blaster::Independent::ECS::Synchronization::OpSetField>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpSetField;

    static void Encode(const Type& type, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, type.path);
        CommonNetwork::EncodeString(buffer, type.componentType);
        CommonNetwork::EncodeString(buffer, type.field);
        CommonNetwork::EncodeBlob(buffer, type.blob);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type result;

        result.path = CommonNetwork::DecodeString(bytes, offset);
        result.componentType = CommonNetwork::DecodeString(bytes, offset);
        result.field = CommonNetwork::DecodeString(bytes, offset);
        result.blob = CommonNetwork::DecodeBlob(bytes, offset);

        return result;
    }
};
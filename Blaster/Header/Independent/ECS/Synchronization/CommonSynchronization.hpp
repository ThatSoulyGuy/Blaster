#pragma once

#include "Independent/Network/CommonNetwork.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Independent::ECS
{
    class IGameObjectSynchronization;
}

namespace Blaster::Independent::ECS::Synchronization
{
    struct DirtyRequest
    {
        std::weak_ptr<Blaster::Independent::ECS::IGameObjectSynchronization> go;
        std::optional<std::type_index> component;
    };

    thread_local std::vector<DirtyRequest> gDeferredDirty;

    inline std::atomic_uint32_t gSnapshotApplyDepth{ 0 };

    enum class Route : std::uint8_t
    {
        ServerBroadcast = 0,
        ToServerOnly = 1,
        RelayOnce = 2,
    };

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

        std::optional<NetworkId> owner;
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
        int componentType;
        std::vector<std::uint8_t> blob;
    };

    struct OpRemoveComponent
    {
        static constexpr OpCode Code = OpCode::RemoveComponent;

        std::string path;
        int componentType;
    };

    struct OpSetField
    {
        static constexpr OpCode Code = OpCode::SetField;

        std::string path;
        int componentType;
        std::string field;
        std::vector<std::uint8_t> blob;
    };

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
    struct SnapshotHeader
    {
        std::uint64_t sequence;
        std::uint32_t operationCount;
        std::uint64_t ack;
        Route route;
        Blaster::Independent::Network::NetworkId origin; 
    }
#if defined(_MSC_VER)
    ;
#pragma pack(pop)
#elif defined(__clang__) || defined(__GNUC__)
    __attribute__((packed));
#else
#error "Unknown compiler - please add packing directives"
#endif

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
        CommonNetwork::WriteTrivial(buffer, value.ack);
        CommonNetwork::WriteTrivial(buffer, value.route);
        CommonNetwork::WriteTrivial(buffer, value.origin);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type header = {};

        header.sequence = CommonNetwork::ReadTrivial<std::uint64_t>(bytes, offset);
        header.operationCount = CommonNetwork::ReadTrivial<std::uint32_t>(bytes, offset);
        header.ack = CommonNetwork::ReadTrivial<std::uint64_t>(bytes, offset);
        header.route = (Blaster::Independent::ECS::Synchronization::Route) CommonNetwork::ReadTrivial<std::uint8_t>(bytes, offset);
        header.origin = CommonNetwork::ReadTrivial<std::uint32_t>(bytes, offset);

        return header;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::Snapshot> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::Snapshot>, Blaster::Independent::ECS::Synchronization::Snapshot>
{
    using Type = Blaster::Independent::ECS::Synchronization::Snapshot;

    static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
    {
        Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::SnapshotHeader>::Encode(operation.header, buffer);

        CommonNetwork::WriteRaw(buffer, operation.operationBlob.data(), operation.operationBlob.size());
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

    static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, operation.path);
        CommonNetwork::EncodeString(buffer, operation.className);

        const bool hasOwner = operation.owner.has_value();

        CommonNetwork::WriteTrivial(buffer, static_cast<std::uint8_t>(hasOwner));

        if (hasOwner)
            CommonNetwork::WriteTrivial(buffer, operation.owner.value());
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type out;

        out.path = CommonNetwork::DecodeString(bytes, offset);
        out.className = CommonNetwork::DecodeString(bytes, offset);

        const bool hasOwner = CommonNetwork::ReadTrivial<std::uint8_t>(bytes, offset);

        if (hasOwner)
            out.owner = CommonNetwork::ReadTrivial<NetworkId>(bytes, offset);

        return out;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpDestroy> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpDestroy>, Blaster::Independent::ECS::Synchronization::OpDestroy>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpDestroy;

    static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, operation.path);
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

    static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, operation.path);
        CommonNetwork::WriteTrivial(buffer, operation.componentType);

        const std::uint32_t length = static_cast<std::uint32_t>(operation.blob.size());

        CommonNetwork::WriteTrivial(buffer, length);
        CommonNetwork::WriteRaw(buffer, operation.blob.data(), length);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type result;

        result.path = CommonNetwork::DecodeString(bytes, offset);
        result.componentType = CommonNetwork::ReadTrivial<int>(bytes, offset);

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

    static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, operation.path);
        CommonNetwork::WriteTrivial(buffer, operation.componentType);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type result;

        result.path = CommonNetwork::DecodeString(bytes, offset);
        result.componentType = CommonNetwork::ReadTrivial<int>(bytes, offset);

        return result;
    }
};

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpSetField> : Blaster::Independent::Network::DataConversionBase<Blaster::Independent::Network::DataConversion<Blaster::Independent::ECS::Synchronization::OpSetField>, Blaster::Independent::ECS::Synchronization::OpSetField>
{
    using Type = Blaster::Independent::ECS::Synchronization::OpSetField;

    static void Encode(const Type& operation, std::vector<std::uint8_t>& buffer)
    {
        CommonNetwork::EncodeString(buffer, operation.path);
        CommonNetwork::WriteTrivial(buffer, operation.componentType);
        CommonNetwork::EncodeString(buffer, operation.field);
        CommonNetwork::EncodeBlob(buffer, operation.blob);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;

        Type result;

        result.path = CommonNetwork::DecodeString(bytes, offset);
        result.componentType = CommonNetwork::ReadTrivial<int>(bytes, offset);
        result.field = CommonNetwork::DecodeString(bytes, offset);
        result.blob = CommonNetwork::DecodeBlob(bytes, offset);

        return result;
    }
};
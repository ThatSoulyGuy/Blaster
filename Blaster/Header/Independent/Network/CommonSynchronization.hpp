#pragma once

#include "Independent/Network/CommonNetwork.hpp"

namespace Blaster::Independent::Network
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
        std::string compType;
        std::vector<std::uint8_t> blob;
    };

    struct OpRemoveComponent
    {
        static constexpr OpCode Code = OpCode::RemoveComponent;

        std::string path;
        std::string compType;
    };

    struct OpSetField
    {
        static constexpr OpCode Code = OpCode::SetField;

        std::string path;
        std::string compType;
        std::string field;
        std::vector<std::uint8_t> blob;
    };

#if defined(_MSC_VER)
#pragma pack(push, 1)
    struct SnapshotHeader
    {
        std::uint64_t sequence;
        std::uint32_t opCount;
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
        std::vector<std::uint8_t> opBlob;
    };

    template <>
    struct DataConversion<SnapshotHeader> : DataConversionBase<DataConversion<SnapshotHeader>, SnapshotHeader>
    {
        using Type = SnapshotHeader;

        static void Encode(const Type& value, std::vector<std::uint8_t>& buf)
        {
            CommonNetwork::WriteTrivial(buf, value.sequence);
            CommonNetwork::WriteTrivial(buf, value.opCount);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t off = 0;

            Type header;

            header.sequence = CommonNetwork::ReadTrivial<std::uint64_t>(bytes, off);
            header.opCount = CommonNetwork::ReadTrivial<std::uint32_t>(bytes, off);

            return header;
        }
    };

    template <>
    struct DataConversion<Snapshot> : DataConversionBase<DataConversion<Snapshot>, Snapshot>
    {
        using Type = Snapshot;

        static void Encode(const Type& v, std::vector<std::uint8_t>& buf)
        {
            DataConversion<SnapshotHeader>::Encode(v.header, buf);

            CommonNetwork::WriteRaw(buf, v.opBlob.data(), v.opBlob.size());
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Snapshot out;

            out.header = std::any_cast<SnapshotHeader>(DataConversion<SnapshotHeader>::Decode(bytes.subspan(offset, sizeof(SnapshotHeader))));
            offset += sizeof(SnapshotHeader);

            std::vector<std::uint8_t> tmp(bytes.begin() + offset, bytes.end());
            out.opBlob.swap(tmp);

            return out;
        }
    };

    template <>
    struct DataConversion<OpCreate> : DataConversionBase<DataConversion<OpCreate>, OpCreate>
    {
        using Type = OpCreate;

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
    struct DataConversion<OpDestroy> : DataConversionBase<DataConversion<OpDestroy>, OpDestroy>
    {
        using Type = OpDestroy;

        static void Encode(const Type& v, std::vector<std::uint8_t>& buf)
        {
            CommonNetwork::EncodeString(buf, v.path);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t off = 0;
            
            return Type{ CommonNetwork::DecodeString(bytes, off) };
        }
    };

    template <>
    struct DataConversion<OpAddComponent> : DataConversionBase<DataConversion<OpAddComponent>, OpAddComponent>
    {
        using Type = OpAddComponent;

        static void Encode(const Type& v, std::vector<std::uint8_t>& buf)
        {
            CommonNetwork::EncodeString(buf, v.path);
            CommonNetwork::EncodeString(buf, v.compType);

            const std::uint32_t len = static_cast<std::uint32_t>(v.blob.size());

            CommonNetwork::WriteTrivial(buf, len);
            CommonNetwork::WriteRaw(buf, v.blob.data(), len);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, offset);
            out.compType = CommonNetwork::DecodeString(bytes, offset);

            const std::uint32_t len = CommonNetwork::ReadTrivial<std::uint32_t>(bytes, offset);

            out.blob.assign(bytes.begin() + offset, bytes.begin() + offset + len);
            offset += len;

            return out;
        }
    };

    template <>
    struct DataConversion<OpRemoveComponent> : DataConversionBase<DataConversion<OpRemoveComponent>, OpRemoveComponent>
    {
        using Type = OpRemoveComponent;

        static void Encode(const Type& v, std::vector<std::uint8_t>& buf)
        {
            CommonNetwork::EncodeString(buf, v.path);
            CommonNetwork::EncodeString(buf, v.compType);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t off = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, off);
            out.compType = CommonNetwork::DecodeString(bytes, off);

            return out;
        }
    };

    template <>
    struct DataConversion<OpSetField> : DataConversionBase<DataConversion<OpSetField>, OpSetField>
    {
        using Type = OpSetField;

        static void Encode(const Type& v, std::vector<std::uint8_t>& buf)
        {
            CommonNetwork::EncodeString(buf, v.path);
            CommonNetwork::EncodeString(buf, v.compType);
            CommonNetwork::EncodeString(buf, v.field);
            CommonNetwork::EncodeBlob(buf, v.blob);
        }

        static std::any Decode(std::span<const std::uint8_t> bytes)
        {
            std::size_t offset = 0;

            Type out;

            out.path = CommonNetwork::DecodeString(bytes, offset);
            out.compType = CommonNetwork::DecodeString(bytes, offset);
            out.field = CommonNetwork::DecodeString(bytes, offset);
            out.blob = CommonNetwork::DecodeBlob(bytes, offset);

            return out;
        }
    };
}
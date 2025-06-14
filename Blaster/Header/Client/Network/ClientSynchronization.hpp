#pragma once

#include <unordered_set>
#include <boost/archive/text_iarchive.hpp>
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/CommonSynchronization.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Network;

namespace Blaster::Client::Network
{
    template <typename Value>
    void DeserializeInto(std::shared_ptr<Value>& value, const std::vector<std::uint8_t>& blob)
    {
        std::string txt(blob.begin(), blob.end());
        std::istringstream ss(txt);
        boost::archive::text_iarchive ia(ss);

        ia >> value;
    }

    class ClientSynchronization final
    {

    public:

        ClientSynchronization(const ClientSynchronization&) = delete;
        ClientSynchronization(ClientSynchronization&&) = delete;
        ClientSynchronization& operator=(const ClientSynchronization&) = delete;
        ClientSynchronization& operator=(ClientSynchronization&&) = delete;

        static void HandleSnapshotPayload(std::vector<std::uint8_t> payload)
        {
            std::span<std::uint8_t> span(payload.data(), payload.size());

            std::vector<std::any> anyVec = CommonNetwork::DisassembleData(span);

            if (anyVec.empty())
                return;

            const Snapshot snap = std::any_cast<Snapshot>(anyVec.front());

            if (snap.header.sequence == 0)
                GameObjectManager::GetInstance().Clear();

            std::span<const std::uint8_t> blob(snap.opBlob.data(), snap.opBlob.size());

            std::size_t offset = 0;

            for (std::uint32_t i = 0; i < snap.header.opCount; ++i)
            {
                if (offset + 5 > blob.size())
                    break;

                const OpCode code = static_cast<OpCode>(blob[offset]);
                offset += sizeof(std::uint8_t);

                const std::uint32_t length = CommonNetwork::ReadTrivial<std::uint32_t>(blob, offset);

                if (length == 0)
                {
                    std::cerr << "Zero-length op payload, code '" << static_cast<int>(code) << "' in ClientSynchronization::HandleSnapshotPayload!";
                    continue;
                }

                if (offset + length > blob.size())
                    break;

                const std::span<const std::uint8_t> slice(blob.data() + offset, length);

                offset += length;

                ApplyOperation(code, slice);
            }
        }

    private:

        ClientSynchronization() = default;

        static void ApplyOperation(OpCode code, std::span<const std::uint8_t> slice)
        {
            switch (code)
            {

            case OpCode::Create:
                HandleCreate(slice);
                break;

            case OpCode::Destroy:
                HandleDestroy(slice);
                break;

            case OpCode::AddComponent:
                HandleAddComponent(slice);
                break;

            case OpCode::RemoveComponent:
                HandleRemoveComponent(slice);
                break;

            case OpCode::SetField:
                HandleSetField(slice);
                break;

            default:
                std::cerr << "Invalid opCode '" << (int)code << "' at ClientSynchronization::ApplyOperation." << std::endl;
                break;
            }
        }

        static void HandleCreate(std::span<const std::uint8_t> slice)
        {
            OpCreate operation = std::any_cast<OpCreate>(DataConversion<OpCreate>::Decode(slice));

            const std::string& path = operation.path;

            std::string parentPath;
            std::string objectName;

            std::size_t dotPosition = path.find_last_of('.');

            if (dotPosition == std::string::npos)
            {
                parentPath = ".";
                objectName = path;
            }
            else
            {
                parentPath = path.substr(0, dotPosition);
                objectName = path.substr(dotPosition + 1);
            }

            auto gameObject = GameObject::Create(objectName);

            GameObjectManager::GetInstance().Register(gameObject, parentPath);
        }

        static void HandleDestroy(std::span<const std::uint8_t> slice)
        {
            OpDestroy operation = std::any_cast<OpDestroy>(DataConversion<OpDestroy>::Decode(slice));

            GameObjectManager::GetInstance().Unregister(operation.path);
        }

        static void HandleAddComponent(std::span<const std::uint8_t> slice)
        {
            OpAddComponent operation = std::any_cast<OpAddComponent>(DataConversion<OpAddComponent>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional.has_value())
                return;

            std::shared_ptr<Component> component = ComponentFactory::Instantiate(operation.compType);

            if (component == nullptr)
                return;

            DeserializeInto(component, operation.blob);

            if (gameObjectOptional.value()->HasComponentDynamic(operation.compType))
                gameObjectOptional.value()->RemoveComponentDynamic(operation.compType);

            gameObjectOptional.value()->AddComponentDynamic(component);
        }

        static void HandleRemoveComponent(std::span<const std::uint8_t> slice)
        {
            OpRemoveComponent operation = std::any_cast<OpRemoveComponent>(DataConversion<OpRemoveComponent>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional.has_value())
                return;

            gameObjectOptional.value()->RemoveComponentDynamic(operation.compType);
        }

        static void HandleSetField(std::span<const std::uint8_t> slice)
        {
            OpSetField operation = std::any_cast<OpSetField>(DataConversion<OpSetField>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional.has_value())
                return;

            auto componentOptional = gameObjectOptional.value()->GetComponentDynamic(operation.compType);

            if (!componentOptional.has_value())
                return;

            DeserializeInto(componentOptional.value(), operation.blob);
        }
    };
}
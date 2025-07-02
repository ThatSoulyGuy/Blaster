#pragma once

#include <unordered_set>
#include <queue>
#include <sstream>
#include <string_view>
#include <boost/archive/text_iarchive.hpp>
#include <boost/mp11.hpp>
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/ECS/Synchronization/SyncTracker.hpp"
#include "Independent/ECS/GameObjectManager.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Network;

namespace Blaster::Independent::ECS::Synchronization
{
    class ReceiverSynchronization final
    { 

    public:

        ReceiverSynchronization(const ReceiverSynchronization&) = delete;
        ReceiverSynchronization(ReceiverSynchronization&&) = delete;
        ReceiverSynchronization& operator=(const ReceiverSynchronization&) = delete;
        ReceiverSynchronization& operator=(ReceiverSynchronization&&) = delete;

        static void HandleSnapshotPayload(std::vector<std::uint8_t> payload)
        {
            std::span<std::uint8_t> packet(payload.data(), payload.size());
            std::vector<std::any> anyList = CommonNetwork::DisassembleData(packet);

            if (anyList.empty())
                return;

            Snapshot snapshot = std::any_cast<Snapshot>(anyList.front());
             
            if (snapshot.header.sequence <= SyncTracker::GetLastIncoming(snapshot.header.origin))
                return;

#ifndef IS_SERVER
            if (snapshot.header.origin == Blaster::Client::Network::ClientNetwork::GetInstance().GetNetworkId())
                return;
#endif
            
            std::cout << "Received packet from source '" << snapshot.header.origin << "' with route '" << (int)snapshot.header.route << "' with ack '" << snapshot.header.ack << "'!" << std::endl;

            ApplySnapshot(snapshot);

            SyncTracker::MarkDelivered(snapshot.header.origin, snapshot.header.sequence);
            SyncTracker::MarkAck(snapshot.header.origin, snapshot.header.ack);

#ifdef IS_SERVER
            if (snapshot.header.route == Route::RelayOnce)
            {
                snapshot.header.route = Route::ServerBroadcast;

                std::cout << "Sent packet containing '" << snapshot.header.operationCount << "' operation(s) to all clients EXCEPT '" << snapshot.header.origin << "'!" << std::endl;

                for (NetworkId id : Blaster::Server::Network::ServerNetwork::GetInstance().GetConnectedClients())
                {
                    if (id != snapshot.header.origin)
                        Blaster::Server::Network::ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Snapshot, snapshot);
                }
            }
#endif
        }

    private:

        ReceiverSynchronization() = default;

        static void ApplySnapshot(const Snapshot& snapshot)
        {
            struct Guard
            {
                Guard() { gSnapshotApplyDepth.fetch_add(1); }
                ~Guard() { gSnapshotApplyDepth.fetch_sub(1); }
            } guard;

            std::span<const std::uint8_t> blob(snapshot.operationBlob.data(), snapshot.operationBlob.size());

            std::size_t offset = 0;

            for (std::uint32_t i = 0; i < snapshot.header.operationCount; ++i)
            {
                if (offset + 5 > blob.size())
                    break;

                OpCode code = static_cast<OpCode>(blob[offset]);

                offset += sizeof(std::uint8_t);

                std::uint32_t length = CommonNetwork::ReadTrivial<std::uint32_t>(blob, offset);

                if (length == 0 || offset + length > blob.size())
                    break;

                std::span<const std::uint8_t> slice(blob.data() + offset, length);

                offset += length;

                ApplyOperation(code, slice, (snapshot.header.origin != 0));
            }
        }

        static void ApplyOperation(OpCode code, std::span<const std::uint8_t> slice, bool fromClient)
        {
            switch (code)
            {

            case OpCode::Create:
                HandleCreate(slice, fromClient);
                break;

            case OpCode::Destroy:
                HandleDestroy(slice, fromClient);
                break;

            case OpCode::AddComponent:
                HandleAddComponent(slice, fromClient);
                break;

            case OpCode::RemoveComponent:
                HandleRemoveComponent(slice, fromClient);
                break;

            case OpCode::SetField:
                HandleSetField(slice, fromClient);
                break;

            default:
                std::cerr << "Invalid opCode '" << (int)code << "' at ReceiverSynchronization::ApplyOperation." << std::endl;
                break;
            }

            std::cout << "Applied opCode '" << (int)code << "', fromClient was '" << fromClient << "'!" << std::endl;
        }

        static void HandleCreate(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpCreate operation = std::any_cast<OpCreate>(DataConversion<OpCreate>::Decode(slice));

            const std::string& path = operation.path;

            if (GameObjectManager::GetInstance().Has(path)) //TODO: Very hacky, fix this
                return;

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
            
            auto gameObject = GameObject::Create(objectName, false, operation.owner);
            
            gameObject->ClearJustCreated();

#ifndef IS_SERVER
            GameObjectManager::GetInstance().Register(gameObject, parentPath, fromClient);
#else
            GameObjectManager::GetInstance().Register(gameObject, parentPath, !fromClient);
#endif
        }

        static void HandleDestroy(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpDestroy operation = std::any_cast<OpDestroy>(DataConversion<OpDestroy>::Decode(slice));

            GameObjectManager::GetInstance().Unregister(operation.path);
        }

        static void HandleAddComponent(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpAddComponent operation = std::any_cast<OpAddComponent>(DataConversion<OpAddComponent>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional)
                return;

            if (gameObjectOptional.value()->HasComponentDynamic(operation.componentType))
            {
                auto& existing = *gameObjectOptional.value()->UnsafeFindComponentPointer(operation.componentType);

                if (typeid(*existing) == typeid(Transform))
                {
                    std::shared_ptr<Component> fresh = ComponentFactory::Instantiate(operation.componentType);

                    DeserializeInto(fresh, operation.blob);

                    std::shared_ptr<Transform> incoming = std::static_pointer_cast<Transform>(fresh);

                    std::static_pointer_cast<Transform>(existing)->SetLocalPosition(incoming->GetLocalPosition(), false);
                    std::static_pointer_cast<Transform>(existing)->SetLocalRotation(incoming->GetLocalRotation(), false);
                    std::static_pointer_cast<Transform>(existing)->SetLocalScale(incoming->GetLocalScale(), false);
                    
                    existing->ClearWasAdded();
                    SenderSynchronization::RememberHash(existing);
                }

                return;
            }

            std::shared_ptr<Component> fresh = ComponentFactory::Instantiate(operation.componentType);

            if (!fresh)
                return;

            DeserializeInto(fresh, operation.blob);

            fresh->ClearWasAdded();

            SenderSynchronization::RememberHash(fresh);

#ifndef IS_SERVER
            gameObjectOptional.value()->AddComponentDynamic(std::move(fresh), fromClient);
#else
            gameObjectOptional.value()->AddComponentDynamic(std::move(fresh), !fromClient);
#endif
        }

        static void HandleRemoveComponent(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpRemoveComponent operation = std::any_cast<OpRemoveComponent>(DataConversion<OpRemoveComponent>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional.has_value())
                return;

            if (gameObjectOptional.value()->HasComponentDynamic(operation.componentType))
                SenderSynchronization::ForgetHash(*gameObjectOptional.value()->GetComponentDynamic(operation.componentType));

#ifndef IS_SERVER
            gameObjectOptional.value()->RemoveComponentDynamic(operation.componentType, fromClient);
#else
            gameObjectOptional.value()->RemoveComponentDynamic(operation.componentType, !fromClient);
#endif
        }

        static void HandleSetField(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpSetField operation = std::any_cast<OpSetField>(DataConversion<OpSetField>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional.has_value())
                return;

            auto* componentOptional = gameObjectOptional.value()->UnsafeFindComponentPointer(operation.componentType);

            if (!componentOptional)
                return;

            DeserializeIntoMerge(*componentOptional, operation.blob);

            SenderSynchronization::RememberHash(*componentOptional);
        }

        template <typename T>
        static void DeserializeInto(std::shared_ptr<T>& destination, const std::vector<std::uint8_t>& blob)
        {
            std::string text(blob.begin(), blob.end());
            std::istringstream stream(text);

            boost::archive::text_iarchive archive(stream);

            std::shared_ptr<T> temporary;

            archive >> temporary;

            if (!temporary)
            {
                std::cerr << "Corrupt payload\n";
                return;
            }

            temporary->gameObject = destination->GetGameObject();

            destination = std::move(temporary);
        }

        template <typename T>
        static void DeserializeIntoMerge(std::shared_ptr<T>& destination, std::span<const std::uint8_t> blob)
        {
            std::string_view view{ reinterpret_cast<const char*>(blob.data()), blob.size() };

            std::istringstream stream{ std::string(view) };

            boost::archive::text_iarchive archive(stream);

            std::shared_ptr<T> incoming;

            archive >> incoming;

            if (!incoming)
                return;

            MergeComponents(destination, incoming);
        }
    };
}
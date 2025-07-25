#pragma once

#include <unordered_set>
#include <queue>
#include <sstream>
#include <string_view>
#include <boost/archive/text_iarchive.hpp>
#include <boost/mp11.hpp>
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/ECS/Synchronization/SyncTracker.hpp"
#include "Independent/ECS/Synchronization/TranslationBuffer.hpp"
#include "Independent/ECS/GameObjectManager.hpp"

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Network;

namespace Blaster::Independent::ECS::Synchronization
{
    struct SnapshotApplyGuard
    {
        SnapshotApplyGuard()
        {
            gSnapshotApplyDepth.fetch_add(1);
        }

        ~SnapshotApplyGuard()
        {
            gSnapshotApplyDepth.fetch_sub(1);

            if (gSnapshotApplyDepth.load(std::memory_order_relaxed) == 0)
            {
                for (auto& request : gDeferredDirty)
                {
                    if (auto gameObject = request.go.lock())
                    {
                        if (request.component.has_value())
                            SenderSynchronization::GetInstance().MarkDirty(std::static_pointer_cast<GameObject>(gameObject), request.component.value());
                        else
                            SenderSynchronization::GetInstance().MarkDirty(std::static_pointer_cast<GameObject>(gameObject));
                    }
                }

                gDeferredDirty.clear();
            }
        }
    };

    class ReceiverSynchronization final
    { 

    public:

        ReceiverSynchronization(const ReceiverSynchronization&) = delete;
        ReceiverSynchronization(ReceiverSynchronization&&) = delete;
        ReceiverSynchronization& operator=(const ReceiverSynchronization&) = delete;
        ReceiverSynchronization& operator=(ReceiverSynchronization&&) = delete;

        void HandleSnapshotPayload(std::vector<std::uint8_t> payload)
        {
            std::span<std::uint8_t> packet(payload.data(), payload.size());
            std::vector<std::any> anyList = CommonNetwork::DisassembleData(packet);

            if (anyList.empty())
                return;

            Snapshot snapshot = std::any_cast<Snapshot>(anyList.front());
             
            if (snapshot.header.sequence <= SyncTracker::GetInstance().GetLastIncoming(snapshot.header.origin))
                return;

#ifndef IS_SERVER
            if (snapshot.header.origin == Blaster::Client::Network::ClientNetwork::GetInstance().GetNetworkId())
                return;
#endif
            
            std::cout << "Received packet from source '" << snapshot.header.origin << "' with route '" << (int)snapshot.header.route << "' with ack '" << snapshot.header.ack << "'!" << std::endl;

            ApplySnapshot(snapshot);

            SyncTracker::GetInstance().MarkDelivered(snapshot.header.origin, snapshot.header.sequence);
            SyncTracker::GetInstance().MarkAck(snapshot.header.origin, snapshot.header.ack);

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

        static ReceiverSynchronization& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<ReceiverSynchronization>(new ReceiverSynchronization());
            });

            return *instance;
        }

    private:

        ReceiverSynchronization() = default;

        void ApplySnapshot(const Snapshot& snapshot)
        {
            SnapshotApplyGuard guard;

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

        void ApplyOperation(OpCode code, std::span<const std::uint8_t> slice, bool fromClient)
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

        void HandleCreate(std::span<const std::uint8_t> slice, bool fromClient)
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

        void HandleDestroy(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpDestroy operation = std::any_cast<OpDestroy>(DataConversion<OpDestroy>::Decode(slice));

            GameObjectManager::GetInstance().Unregister(operation.path);
        }

        void HandleAddComponent(const std::span<const std::uint8_t> slice, bool fromClient)
        {
            auto [path, componentType, blob] = std::any_cast<OpAddComponent>(DataConversion<OpAddComponent>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(path);

            if (!gameObjectOptional)
                return;

            if (gameObjectOptional.value()->HasComponentDynamic(TypeRegistrar::GetRuntimeName(componentType)))
            {
                auto& existing = *gameObjectOptional.value()->UnsafeFindComponentPointer(TypeRegistrar::GetRuntimeName(componentType));

                if (typeid(*existing) == typeid(Transform3d))
                {
                    std::shared_ptr<Component> fresh = ComponentFactory::Instantiate(componentType);

                    DeserializeInto(fresh, blob);

                    std::shared_ptr<Transform3d> incoming = std::static_pointer_cast<Transform3d>(fresh);

                    std::static_pointer_cast<Transform3d>(existing)->SetLocalPosition(incoming->GetLocalPosition(), false);
                    std::static_pointer_cast<Transform3d>(existing)->SetLocalRotation(incoming->GetLocalRotation(), false);
                    std::static_pointer_cast<Transform3d>(existing)->SetLocalScale(incoming->GetLocalScale(), false);

                    existing->ClearWasAdded();
                    SenderSynchronization::GetInstance().RememberHash(existing);
                }
                else
                {
                    DeserializeIntoMerge(existing, blob);
                    SenderSynchronization::GetInstance().RememberHash(existing);
                }

                return;
            }

            std::shared_ptr<Component> fresh = ComponentFactory::Instantiate(componentType);

            if (!fresh)
                return;

            DeserializeInto(fresh, blob);

            fresh->ClearWasAdded();

            SenderSynchronization::GetInstance().RememberHash(fresh);

#ifndef IS_SERVER
            gameObjectOptional.value()->AddComponentDynamic(std::move(fresh), fromClient);
#else
            gameObjectOptional.value()->AddComponentDynamic(std::move(fresh), !fromClient);
#endif
        }

        void HandleRemoveComponent(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpRemoveComponent operation = std::any_cast<OpRemoveComponent>(DataConversion<OpRemoveComponent>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (!gameObjectOptional.has_value())
                return;

            if (gameObjectOptional.value()->HasComponentDynamic(TypeRegistrar::GetRuntimeName(operation.componentType)))
                SenderSynchronization::GetInstance().ForgetHash(*gameObjectOptional.value()->GetComponentDynamic(TypeRegistrar::GetRuntimeName(operation.componentType)));

#ifndef IS_SERVER
            gameObjectOptional.value()->RemoveComponentDynamic(TypeRegistrar::GetRuntimeName(operation.componentType), fromClient);
#else
            gameObjectOptional.value()->RemoveComponentDynamic(TypeRegistrar::GetRuntimeName(operation.componentType), !fromClient);
#endif
        }

        void HandleSetField(std::span<const std::uint8_t> slice, bool fromClient)
        {
            OpSetField operation = std::any_cast<OpSetField>(DataConversion<OpSetField>::Decode(slice));

            auto gameObjectOptional = GameObjectManager::GetInstance().Get(operation.path);

            if (gameObjectOptional && gameObjectOptional.value()->IsLocallyControlled())
                return;
            
#ifndef IS_SERVER
            if (operation.componentType == TypeRegistrar::GetTypeId<Blaster::Independent::Math::Transform3d>())
            {
                std::shared_ptr<Component> fresh = ComponentFactory::Instantiate(operation.componentType);

                DeserializeInto(fresh, operation.blob);

                std::shared_ptr<Transform3d> temporary = std::static_pointer_cast<Transform3d>(fresh);

                if (gameObjectOptional)
                    TranslationBuffer::GetInstance().Enqueue(std::static_pointer_cast<Transform3d>(*gameObjectOptional.value()->UnsafeFindComponentPointer(TypeRegistrar::GetRuntimeName(operation.componentType))), temporary->GetLocalPosition(), temporary->GetLocalRotation(), temporary->GetLocalScale());
                
                return;
            }
#endif

            if (!gameObjectOptional.has_value())
                return;

            auto* componentOptional = gameObjectOptional.value()->UnsafeFindComponentPointer(TypeRegistrar::GetRuntimeName(operation.componentType));

            if (!componentOptional)
                return;

            DeserializeIntoMerge(*componentOptional, operation.blob);

            SenderSynchronization::GetInstance().RememberHash(*componentOptional);
        }

        template <typename T>
        void DeserializeInto(std::shared_ptr<T>& destination, const std::vector<std::uint8_t>& blob)
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
        void DeserializeIntoMerge(std::shared_ptr<T>& destination, std::span<const std::uint8_t> blob)
        {
            std::string_view view{ reinterpret_cast<const char*>(blob.data()), blob.size() };

            std::istringstream stream{ std::string(view) };

            boost::archive::text_iarchive archive(stream);

            std::shared_ptr<T> incoming;

            archive >> incoming;

            if (!incoming)
                return;

            MergeSupport::MergeComponents(destination, incoming);
        }

        static std::once_flag initializationFlag;
        static std::unique_ptr<ReceiverSynchronization> instance;

    };

    std::once_flag ReceiverSynchronization::initializationFlag;
    std::unique_ptr<ReceiverSynchronization> ReceiverSynchronization::instance;
}
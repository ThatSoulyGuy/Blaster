#pragma once

#include <unordered_set>
#include <queue>
#include <string_view>
#include <boost/archive/text_oarchive.hpp>
#include "Independent/ECS/Synchronization/CommonSynchronization.hpp"
#include "Independent/ECS/Synchronization/SyncTracker.hpp"
#include "Independent/ECS/IGameObjectSynchronization.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"

#ifdef IS_SERVER
#include "Server/Network/ServerNetwork.hpp" 
#else
#include "Client/Network/ClientNetwork.hpp"
#endif

namespace Blaster::Independent::ECS
{
    class GameObject;
}

using namespace Blaster::Independent::Network;
using namespace Blaster::Independent::Thread;

namespace Blaster::Independent::ECS::Synchronization
{
    struct DirtyCompKey
    {
        std::weak_ptr<IGameObjectSynchronization> gameObject;
        std::type_index componentType;
    };

    struct DirtyCompHash
    {
        std::size_t operator()(const DirtyCompKey& k) const noexcept
        {
            auto sp = k.gameObject.lock();

            std::size_t h1 = std::hash<void*>{}(sp.get());
            std::size_t h2 = k.componentType.hash_code();

            return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
        }
    };

    struct DirtyCompEqual
    {
        bool operator()(const DirtyCompKey& a, const DirtyCompKey& b) const noexcept
        {
            return a.componentType == b.componentType && a.gameObject.lock().get() == b.gameObject.lock().get();
        }
    };

    class SenderSynchronization final
    {

    public:

        SenderSynchronization(const SenderSynchronization&) = delete;
        SenderSynchronization(SenderSynchronization&&) = delete;
        SenderSynchronization& operator=(const SenderSynchronization&) = delete;
        SenderSynchronization& operator=(SenderSynchronization&&) = delete;

        static void MarkDirty(const std::shared_ptr<GameObject>& gameObject)
        {
            if (gSnapshotApplyDepth.load(std::memory_order_relaxed) != 0)
                return;

#ifndef IS_SERVER
            auto sync = std::static_pointer_cast<IGameObjectSynchronization>(gameObject);

            if (sync->GetOwningClient().has_value() && sync->GetOwningClient().value() != Blaster::Client::Network::ClientNetwork::GetInstance().GetNetworkId())
                return;
#endif

            if (std::static_pointer_cast<IGameObjectSynchronization>(gameObject)->IsLocal())
                return;

            {
                std::unique_lock guard(dirtyMutex);
                dirtyGameObjectSet.insert(std::static_pointer_cast<IGameObjectSynchronization>(gameObject));
            }

            WakeFlusher();
        }

        static void MarkDirty(const std::shared_ptr<GameObject>& gameObject, const std::type_index& component)
        {
            if (gSnapshotApplyDepth.load(std::memory_order_relaxed) != 0)
                return;

#ifndef IS_SERVER
            auto sync = std::static_pointer_cast<IGameObjectSynchronization>(gameObject);

            if (sync->GetOwningClient().has_value() && sync->GetOwningClient().value() != Blaster::Client::Network::ClientNetwork::GetInstance().GetNetworkId())
                return;
#endif

            if (std::static_pointer_cast<IGameObjectSynchronization>(gameObject)->IsLocal())
                return;

            {
                std::unique_lock guard(dirtyMutex);
                dirtyComponentSet.emplace(DirtyCompKey{ std::static_pointer_cast<IGameObjectSynchronization>(gameObject), component });
            }

            WakeFlusher();
        }

        static void FlushDirty()
        {
            if (!flushRequested.exchange(false, std::memory_order_acq_rel))
                return;

            Snapshot templateSnapshot{};

            templateSnapshot.header.operationCount = 0;

#ifdef IS_SERVER
            templateSnapshot.header.route = Route::ServerBroadcast;
            templateSnapshot.header.origin = 0;
#else
            templateSnapshot.header.route = Route::RelayOnce;
            templateSnapshot.header.origin = Blaster::Client::Network::ClientNetwork::GetInstance().GetNetworkId();
#endif

            {
                std::unique_lock guard(dirtyMutex);

                for (auto const& node : dirtyGameObjectSet)
                {
                    if (node->IsDestroyed())
                    {
                        for (const auto& component : node->GetComponentMap() | std::views::values)
                            ForgetHash(component);

                        PushOp(templateSnapshot.operationBlob, OpDestroy{ node->GetAbsolutePath() });

                        ++templateSnapshot.header.operationCount;

                        continue;
                    }

                    if (node->WasJustCreated())
                    {
                        PushOp(templateSnapshot.operationBlob, OpCreate{ node->GetAbsolutePath(), node->GetTypeName(), node->GetOwningClient() });
                        ++templateSnapshot.header.operationCount;

                        for (auto& component : node->GetComponentMap() | std::views::values)
                        {
                            PushOp(templateSnapshot.operationBlob, OpAddComponent{ node->GetAbsolutePath(), component->GetTypeName(), CommonNetwork::SerializePointerToBlob(component) });

                            ++templateSnapshot.header.operationCount;

                            component->ClearWasAdded();

                            RememberHash(component);
                        }

                        node->ClearJustCreated();
                    }
                }

                for (const auto& [gameObject, componentType] : dirtyComponentSet)
                {
                    const auto gameObjectPointer = gameObject.lock();

                    if (!gameObjectPointer)
                        continue;

                    const auto componentIterator = gameObjectPointer->GetComponentMap().find(componentType);

                    if (componentIterator == gameObjectPointer->GetComponentMap().end())
                    {
                        PushOp(templateSnapshot.operationBlob, OpRemoveComponent{ gameObjectPointer->GetAbsolutePath(), componentType.name() });

                        ++templateSnapshot.header.operationCount;

                        ForgetHash(componentIterator->second);

                        continue;
                    }

                    if (const auto& component = componentIterator->second; component->WasAdded())
                    {
                        if (HasStateChanged(component))
                        {
                            PushOp(templateSnapshot.operationBlob, OpAddComponent{ gameObjectPointer->GetAbsolutePath(), component->GetTypeName(), CommonNetwork::SerializePointerToBlob(component) });
                            component->ClearWasAdded();

                            ++templateSnapshot.header.operationCount;
                        }
                    }
                    else
                    {
                        if (HasStateChanged(component))
                        {
                            std::vector<std::uint8_t> blob = CommonNetwork::SerializePointerToBlob(component);

                            PushOp(templateSnapshot.operationBlob, OpSetField{ gameObjectPointer->GetAbsolutePath(),  component->GetTypeName(), "ALL", blob });
                            
                            ++templateSnapshot.header.operationCount;
                        }
                    }
                }

                dirtyComponentSet.clear();
                dirtyGameObjectSet.clear();
            }

            if (templateSnapshot.header.operationCount == 0)
                return;

#ifdef IS_SERVER
            for (const NetworkId id : Blaster::Server::Network::ServerNetwork::GetInstance().GetConnectedClients())
            {
                Snapshot snapshot = templateSnapshot;
                snapshot.header.sequence = SyncTracker::AllocateSequence(id);
                snapshot.header.ack = SyncTracker::GetLastIncoming(id);

                Blaster::Server::Network::ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Snapshot, snapshot);

                std::cout << "Sent snapshot to client '" << id << "' with seqence '" << snapshot.header.sequence << "' and ack '" << snapshot.header.ack << "' ('" << snapshot.header.operationCount << "' operations)!" << std::endl;
            }
#else
            {
                constexpr NetworkId ServerId = 0;

                Snapshot snapshot = templateSnapshot;

                snapshot.header.sequence = SyncTracker::AllocateSequence(ServerId);
                snapshot.header.ack = SyncTracker::GetLastIncoming(ServerId);

                Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_Snapshot, snapshot);

                std::cout << "Sent snapshot to server with seqence '" << snapshot.header.sequence << "' and ack '" << snapshot.header.ack << "' ('" << snapshot.header.operationCount << "' operations)!" << std::endl;
            }
#endif
             
            for (auto iterator = lastHashMap.begin(); iterator != lastHashMap.end(); )
            {
                if (iterator->first == nullptr)
                {
                    std::cerr << "Items in the lastHashMap variable in SenderSynchronization had null keys. This shouldn't happen!" << std::endl;
                    iterator = lastHashMap.erase(iterator);
                }
                else
                    ++iterator;
            }
        }

        static void SynchronizeFullTree(const NetworkId targetClient, const std::vector<std::shared_ptr<GameObject>>& gameObjectList)
        {
            Snapshot snapshot;

            snapshot.header.operationCount = 0;
            snapshot.header.sequence = SyncTracker::AllocateSequence(targetClient);
            snapshot.header.ack = SyncTracker::GetLastIncoming(targetClient);

#ifdef IS_SERVER
            snapshot.header.route = Route::ServerBroadcast;
            snapshot.header.origin = 0;
#else
            snapshot.header.route = Route::RelayOnce;
            snapshot.header.origin = Blaster::Client::Network::ClientNetwork::GetInstance().GetNetworkId();
#endif

            for (const auto& root : gameObjectList)
                SerializeSubTree(std::static_pointer_cast<IGameObjectSynchronization>(root), snapshot);

#ifdef IS_SERVER
            Blaster::Server::Network::ServerNetwork::GetInstance().SendTo(targetClient, PacketType::S2C_Snapshot, snapshot);
#else
            Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_Snapshot, snapshot);
#endif
        }

        static void RememberHash(const std::shared_ptr<Component>& comp)
        {
            const uint64_t handle = ComponentStateHash(comp);

            lastHashMap[comp.get()] = handle;
        }

        static void ForgetHash(const std::shared_ptr<Component>& comp)
        {
            lastHashMap.erase(comp.get());
        }

    private:

        SenderSynchronization() = default;

        static void WakeFlusher()
        {
            bool expected = false;

            const bool firstWriter = flushRequested.compare_exchange_strong(expected, true, std::memory_order_release, std::memory_order_relaxed);

            if (firstWriter)
            {
#ifdef IS_SERVER
                boost::asio::post(Blaster::Server::Network::ServerNetwork::GetInstance().GetIoContext(), []
                    {
                        MainThreadExecutor::GetInstance().EnqueueTask(nullptr, []()
                            {
                                FlushDirty();
                            });
                    });
#else
                boost::asio::post(Blaster::Client::Network::ClientNetwork::GetInstance().GetIoContext(), []
                    {
                        MainThreadExecutor::GetInstance().EnqueueTask(nullptr, []()
                            {
                                FlushDirty();
                            });
                    });
#endif
            }
        }

        static void SerializeSubTree(const std::shared_ptr<IGameObjectSynchronization>& node, Snapshot& snap)
        {
            PushOp(snap.operationBlob, OpCreate{ node->GetAbsolutePath(), node->GetTypeName(), node->GetOwningClient() });
            ++snap.header.operationCount;

            for (const auto& component : node->GetComponentMap() | std::views::values)
            {
                const std::vector<std::uint8_t> blob = CommonNetwork::SerializePointerToBlob(component);

                PushOp(snap.operationBlob, OpAddComponent{ node->GetAbsolutePath(), component->GetTypeName(), blob });

                ++snap.header.operationCount;
            }

            for (const auto& child: node->GetChildMap() | std::views::values)
                SerializeSubTree(std::static_pointer_cast<IGameObjectSynchronization>(child), snap);
        }

        template <typename Op>
        static void PushOp(std::vector<std::uint8_t>& destination, const Op& operation)
        {
            std::vector<std::uint8_t> temporary;
            DataConversion<Op>::Encode(operation, temporary);

            CommonNetwork::WriteTrivial(destination, static_cast<std::uint8_t>(Op::Code));

            const auto length = static_cast<std::uint32_t>(temporary.size());
            CommonNetwork::WriteTrivial(destination, length);

            CommonNetwork::WriteRaw(destination, temporary.data(), temporary.size());
        }

        template <typename Value>
        static std::vector<std::uint8_t> SerializeToBlob(const Value& value)
        {
            std::ostringstream stream;
            boost::archive::text_oarchive archive(stream);

            archive << value;

            const std::string& text = stream.str();

            return { text.begin(), text.end() };
        }

        template <typename T>
        static std::string ToArchiveString(const T& value)
        {
            std::ostringstream stream;
            boost::archive::text_oarchive archive(stream);

            archive << value;

            return stream.str();
        }

        static std::uint64_t HashString(const std::string& s)
        {
            std::uint64_t hash = 14695981039346656037ull;

            for (unsigned char c : s)
                hash = (hash ^ c) * 1099511628211ull;

            return hash;
        }

        template <typename C>
        static std::uint64_t ComponentStateHash(const C& comp)
        {
            return HashString(ToArchiveString(comp));
        }

        static bool HasStateChanged(const std::shared_ptr<Component>& comp)
        {
            const uint64_t h = ComponentStateHash(comp);

            auto it = lastHashMap.find(comp.get());
            const bool dirty = (it == lastHashMap.end()) || (it->second != h);

            if (dirty)
                lastHashMap[comp.get()] = h;

            return dirty;
        }

        inline static std::unordered_set<std::shared_ptr<IGameObjectSynchronization>> dirtyGameObjectSet;
        inline static std::unordered_set<DirtyCompKey, DirtyCompHash, DirtyCompEqual> dirtyComponentSet;

        inline static std::atomic<bool> flushRequested = false;
        inline static std::atomic<std::uint64_t> nextSeq = 1;

        inline static std::unordered_map<const Component*, std::uint64_t> lastHashMap;

        inline static std::shared_mutex dirtyMutex;
    };
}
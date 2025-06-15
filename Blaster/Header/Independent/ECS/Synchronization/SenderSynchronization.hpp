#pragma once

#include <unordered_set>
#include <queue>
#include <boost/archive/text_oarchive.hpp>
#include "Independent/ECS/Synchronization/CommonSynchronization.hpp"
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
    class SenderSynchronization final
    {

    public:

        SenderSynchronization(const SenderSynchronization&) = delete;
        SenderSynchronization(SenderSynchronization&&) = delete;
        SenderSynchronization& operator=(const SenderSynchronization&) = delete;
        SenderSynchronization& operator=(SenderSynchronization&&) = delete;

        static void MarkDirty(const std::shared_ptr<GameObject>& rawGameObject)
        {
            auto gameObject = std::static_pointer_cast<IGameObjectSynchronization>(rawGameObject);

            if (gameObject == nullptr)
                return;

            dirty.insert(gameObject);

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

        static void FlushDirty()
        {
            if (!flushRequested.exchange(false, std::memory_order_acq_rel))
                return;

            if (dirty.empty())
                return;

            Snapshot snapshot = {};

            snapshot.header.sequence = nextSeq++;
            snapshot.header.operationCount = 0;

            for (auto& gameObject : dirty)
            {
                if (gameObject->IsDestroyed())
                {
                    PushOp(snapshot.operationBlob, OpDestroy { gameObject->GetAbsolutePath() });

                    ++snapshot.header.operationCount;
                    continue;
                }

                if (gameObject->WasJustCreated())
                {
                    PushOp(snapshot.operationBlob, OpCreate { gameObject->GetAbsolutePath(), gameObject->GetTypeName() });

                    ++snapshot.header.operationCount;
                }
                
                std::shared_lock lock(gameObject->GetMutex());

                for (auto& component : gameObject->GetComponentMap() | std::views::values)
                {
                    if (component->WasAdded())
                    {
                        PushOp(snapshot.operationBlob, OpAddComponent { gameObject->GetAbsolutePath(), component->GetTypeName(), CommonNetwork::SerializePointerToBlob(component) });

                        ++snapshot.header.operationCount;
                        continue;
                    }

                    if (component->WasRemoved())
                    {
                        lastHashMap.erase(component.get());

                        PushOp(snapshot.operationBlob, OpRemoveComponent { gameObject->GetAbsolutePath(), component->GetTypeName() });

                        ++snapshot.header.operationCount;
                        continue;
                    }

                    const std::uint64_t current = ComponentStateHash(*component);

                    const auto hit = lastHashMap.find(component.get());
                    const bool changed = hit == lastHashMap.end() || hit->second != current;

                    if (changed)
                    {
                        const std::vector<std::uint8_t> blob = CommonNetwork::SerializePointerToBlob(component);

                        PushOp(snapshot.operationBlob, OpSetField{ gameObject->GetAbsolutePath(), component->GetTypeName(), "ALL", blob });

                        ++snapshot.header.operationCount;
                    }

                    lastHashMap[component.get()] = current;
                }
            }

            dirty.clear();

#ifdef IS_SERVER
            for (NetworkId id : Blaster::Server::Network::ServerNetwork::GetInstance().GetConnectedClients())
                Blaster::Server::Network::ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Snapshot, snapshot);
#else
            Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_Snapshot, snapshot);
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

        static void SynchronizeFullTree(NetworkId targetClient, std::vector<std::shared_ptr<GameObject>> gameObjectList)
        {
            Snapshot snapshot;

            snapshot.header.sequence = 0;
            snapshot.header.operationCount = 0;

            for (const auto& root : gameObjectList)
                SerializeSubTree(std::static_pointer_cast<IGameObjectSynchronization>(root), snapshot);

#ifdef IS_SERVER
            Blaster::Server::Network::ServerNetwork::GetInstance().SendTo(targetClient, PacketType::S2C_Snapshot, snapshot);
#else
            Blaster::Client::Network::ClientNetwork::GetInstance().Send(PacketType::C2S_Snapshot, snapshot);
#endif
        }

    private:

        SenderSynchronization() = default;

        static void SerializeSubTree(const std::shared_ptr<IGameObjectSynchronization>& node, Snapshot& snap)
        {
            PushOp(snap.operationBlob, OpCreate{ node->GetAbsolutePath(), node->GetTypeName() });
            ++snap.header.operationCount;

            for (const auto& component : node->GetComponentMap() | std::views::values)
            {
                const std::vector<std::uint8_t> blob = CommonNetwork::SerializePointerToBlob(component);

                PushOp(snap.operationBlob, OpAddComponent{ node->GetAbsolutePath(), component->GetTypeName(), blob });

                ++snap.header.operationCount;
            }

            for (const auto& [name, child] : node->GetChildMap())
                SerializeSubTree(std::static_pointer_cast<IGameObjectSynchronization>(child), snap);
        }

        template <typename Op>
        static void PushOp(std::vector<std::uint8_t>& destination, const Op& operation)
        {
            std::vector<std::uint8_t> temporary;
            DataConversion<Op>::Encode(operation, temporary);

            CommonNetwork::WriteTrivial(destination, static_cast<std::uint8_t>(Op::Code));

            const std::uint32_t length = static_cast<std::uint32_t>(temporary.size());
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

            return std::vector<std::uint8_t>(text.begin(), text.end());
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

        inline static std::unordered_set<std::shared_ptr<IGameObjectSynchronization>> dirty;
        inline static std::atomic<bool> flushRequested = false;
        inline static std::atomic<std::uint64_t> nextSeq = 1;

        inline static std::unordered_map<const Component*, std::uint64_t> lastHashMap;
    };
}
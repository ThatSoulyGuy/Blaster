#pragma once

#include <unordered_set>
#include <boost/archive/text_oarchive.hpp>
#include "Independent/ECS/IGameObjectSynchronization.hpp"
#include "Independent/Network/CommonSynchronization.hpp"
#include "Server/Network/ServerNetwork.hpp"

namespace Blaster::Independent::ECS
{
    class GameObject;
}

using namespace Blaster::Independent::ECS;
using namespace Blaster::Independent::Network;

namespace Blaster::Server::Network
{
    class ServerSynchronization final
    {

    public:

        ServerSynchronization(const ServerSynchronization&) = delete;
        ServerSynchronization(ServerSynchronization&&) = delete;
        ServerSynchronization& operator=(const ServerSynchronization&) = delete;
        ServerSynchronization& operator=(ServerSynchronization&&) = delete;

        static void MarkDirty(const std::shared_ptr<GameObject>& rawGameObject)
        {
            auto gameObject = std::static_pointer_cast<IGameObjectSynchronization>(rawGameObject);

            if (gameObject == nullptr)
                return;

            dirty.insert(gameObject);

            bool expected = false;

            const bool firstWriter = flushRequested.compare_exchange_strong(expected, true, std::memory_order_release, std::memory_order_relaxed);

            if (firstWriter)
                boost::asio::post(ServerNetwork::GetInstance().GetIoContext(), [] { FlushDirty(); });
        }

        static void FlushDirty()
        {
            if (!flushRequested.exchange(false, std::memory_order_acq_rel))
                return;

            if (dirty.empty())
                return;

            Snapshot snapshot = {};

            snapshot.header.sequence = nextSeq++;
            snapshot.header.opCount = 0;

            for (auto& gameObject : dirty)
            {
                if (gameObject->IsDestroyed())
                {
                    PushOp(snapshot.opBlob, OpDestroy { gameObject->GetAbsolutePath() });

                    ++snapshot.header.opCount;
                    continue;
                }

                if (gameObject->WasJustCreated())
                {
                    PushOp(snapshot.opBlob, OpCreate { gameObject->GetAbsolutePath(), gameObject->GetTypeName() });

                    ++snapshot.header.opCount;
                }

                for (auto& [tid, component] : gameObject->GetComponentMap())
                {
                    if (component->WasAdded())
                    {
                        PushOp(snapshot.opBlob, OpAddComponent { gameObject->GetAbsolutePath(), component->GetTypeName(), CommonNetwork::SerializePointerToBlob(component) });

                        ++snapshot.header.opCount;
                        continue;
                    }

                    if (component->WasRemoved())
                    {
                        lastHashMap.erase(component.get());

                        PushOp(snapshot.opBlob, OpRemoveComponent { gameObject->GetAbsolutePath(), component->GetTypeName() });

                        ++snapshot.header.opCount;
                        continue;
                    }

                    const std::uint64_t current = ComponentStateHash(*component);

                    const auto hit = lastHashMap.find(component.get());
                    const bool changed = hit == lastHashMap.end() || hit->second != current;

                    if (changed)
                    {
                        const std::vector<std::uint8_t> blob = CommonNetwork::SerializePointerToBlob(component);

                        PushOp(snapshot.opBlob, OpSetField{ gameObject->GetAbsolutePath(), component->GetTypeName(), "ALL", blob });

                        ++snapshot.header.opCount;
                    }

                    lastHashMap[component.get()] = current;
                }
            }

            dirty.clear();

            for (NetworkId id : ServerNetwork::GetInstance().GetConnectedClients())
                ServerNetwork::GetInstance().SendTo(id, PacketType::S2C_Snapshot, snapshot);

            for (auto iterator = lastHashMap.begin(); iterator != lastHashMap.end(); )
            {
                if (iterator->first == nullptr)
                {
                    std::cerr << "Items in the lastHashMap variable in ServerSynchronization had null keys. This shouldn't happen!" << std::endl;
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
            snapshot.header.opCount = 0;

            for (const auto& root : gameObjectList)
                SerializeSubTree(std::static_pointer_cast<IGameObjectSynchronization>(root), snapshot);

            ServerNetwork::GetInstance().SendTo(targetClient, PacketType::S2C_Snapshot, snapshot);
        }

    private:

        ServerSynchronization() = default;

        static void SerializeSubTree(const std::shared_ptr<IGameObjectSynchronization>& node, Snapshot& snap)
        {
            PushOp(snap.opBlob, OpCreate{ node->GetAbsolutePath(), node->GetTypeName() });
            ++snap.header.opCount;

            for (const auto& component : node->GetComponentMap() | std::views::values)
            {
                const std::vector<std::uint8_t> blob = CommonNetwork::SerializePointerToBlob(component);

                PushOp(snap.opBlob, OpAddComponent{ node->GetAbsolutePath(), component->GetTypeName(), blob });

                ++snap.header.opCount;
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
            std::ostringstream oss;
            boost::archive::text_oarchive oa(oss);

            oa << value;

            const std::string& text = oss.str();

            return std::vector<std::uint8_t>(text.begin(), text.end());
        }

        template <typename T>
        static std::string ToArchiveString(const T& value)
        {
            std::ostringstream oss;
            boost::archive::text_oarchive oa(oss);

            oa << value;

            return oss.str();
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
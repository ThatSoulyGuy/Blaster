#pragma once

#include <unordered_set>
#include <queue>
#include <shared_mutex>
#include <chrono>
#include <boost/archive/text_oarchive.hpp>
#include "Independent/ECS/Synchronization/CommonSynchronization.hpp"
#include "Independent/ECS/IGameObjectSynchronization.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"

#ifdef IS_SERVER
#include "Server/Network/ServerNetwork.hpp"
#else
#include "Client/Network/ClientNetwork.hpp"
#endif

namespace Blaster::Independent::ECS::Synchronization
{
    struct PeerSyncState
    {
        uint64_t lastOutgoingSequence{ 0 };
        uint64_t lastIncomingSequence{ 0 };
        uint64_t lastAckedOutgoing{ 0 };

        std::unordered_set<uint64_t> unackedOutgoing{};
    };

    class SyncTracker final
    {

    public:

        SyncTracker() = delete;

        static uint64_t AllocateSequence(NetworkId peer)
        {
            std::unique_lock guard(mutex);

            auto& state = peerStateMap[peer];
            const uint64_t next = ++state.lastOutgoingSequence;

            state.unackedOutgoing.insert(next);

            return next;
        }

        static void MarkDelivered(NetworkId peer, uint64_t sequence)
        {
            std::unique_lock guard(mutex);

            auto& state = peerStateMap[peer];
            state.lastIncomingSequence = std::max(state.lastIncomingSequence, sequence);
        }

        static void MarkAck(NetworkId peer, uint64_t ack)
        {
            std::unique_lock guard(mutex);

            auto& state = peerStateMap[peer];
            state.lastAckedOutgoing = std::max(state.lastAckedOutgoing, ack);

            for (auto it = state.unackedOutgoing.begin(); it != state.unackedOutgoing.end(); )
            {
                if (*it <= ack)
                    it = state.unackedOutgoing.erase(it);
                else
                    ++it;
            }
        }

        [[nodiscard]]
        static uint64_t GetLastIncoming(NetworkId peer)
        {
            std::shared_lock guard(mutex);

            const auto it = peerStateMap.find(peer);

            return (it == peerStateMap.end()) ? 0 : it->second.lastIncomingSequence;
        }

        [[nodiscard]]
        static uint64_t GetLastAcked(NetworkId peer)
        {
            std::shared_lock guard(mutex);

            const auto it = peerStateMap.find(peer);

            return (it == peerStateMap.end()) ? 0 : it->second.lastAckedOutgoing;
        }

    private:

        inline static std::unordered_map<NetworkId, PeerSyncState> peerStateMap{};
        inline static std::shared_mutex mutex{};

    };
}
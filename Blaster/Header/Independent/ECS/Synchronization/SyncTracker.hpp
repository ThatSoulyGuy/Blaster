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

        SyncTracker(const SyncTracker&) = delete;
        SyncTracker(SyncTracker&&) = delete;
        SyncTracker& operator=(const SyncTracker&) = delete;
        SyncTracker& operator=(SyncTracker&&) = delete;

        uint64_t AllocateSequence(NetworkId peer)
        {
            std::unique_lock guard(mutex);

            auto& state = peerStateMap[peer];
            const uint64_t next = ++state.lastOutgoingSequence;

            state.unackedOutgoing.insert(next);

            return next;
        }

        void MarkDelivered(NetworkId peer, uint64_t sequence)
        {
            std::unique_lock guard(mutex);

            auto& state = peerStateMap[peer];
            state.lastIncomingSequence = std::max(state.lastIncomingSequence, sequence);
        }

        void MarkAck(NetworkId peer, uint64_t ack)
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
        uint64_t GetLastIncoming(NetworkId peer)
        {
            std::shared_lock guard(mutex);

            const auto it = peerStateMap.find(peer);

            return (it == peerStateMap.end()) ? 0 : it->second.lastIncomingSequence;
        }

        [[nodiscard]]
        uint64_t GetLastAcked(NetworkId peer)
        {
            std::shared_lock guard(mutex);

            const auto it = peerStateMap.find(peer);

            return (it == peerStateMap.end()) ? 0 : it->second.lastAckedOutgoing;
        }

        static SyncTracker& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<SyncTracker>(new SyncTracker());
            });

            return *instance;
        }

    private:

        SyncTracker() = default;

        std::unordered_map<NetworkId, PeerSyncState> peerStateMap{};
        std::shared_mutex mutex{};

        static std::once_flag initializationFlag;
        static std::unique_ptr<SyncTracker> instance;

    };

    std::once_flag SyncTracker::initializationFlag;
    std::unique_ptr<SyncTracker> SyncTracker::instance;
}
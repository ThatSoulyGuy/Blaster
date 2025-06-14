#pragma once

#include "Server/Network/ServerNetwork.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Independent/Network/CommonNetwork.hpp"
#include <random>
#include <thread>
#include <vector>
#include <atomic>
#include <barrier>
#include <unordered_set>

using namespace Blaster::Server::Network;
using namespace Blaster::Client::Network;
using namespace Blaster::Independent::Network;

namespace Blaster::Independent::Test
{
    enum class PacketTypeStress : std::uint16_t
    {
        C2S_Stress = 60000,
        S2C_StressAck = 60001
    };

    struct StressRecord
    {
        std::uint32_t threadId;
        std::uint32_t index;
        std::uint64_t payload;
    };

    struct StressServer
    {
        static void Activate()
        {
            ServerNetwork::GetInstance().RegisterReceiver(static_cast<PacketType>(PacketTypeStress::C2S_Stress), [](const NetworkId who, std::vector<std::uint8_t> data)
                {
                    auto fields = CommonNetwork::DisassembleData(std::span(data.data(), data.size()));
                    const StressRecord rec = std::any_cast<StressRecord>(fields[0]);

                    ServerNetwork::GetInstance().SendTo(who, static_cast<PacketType>(PacketTypeStress::S2C_StressAck), rec);
                });
        }
    };

    class StressClient
    {

    public:

        explicit StressClient(std::size_t threadsPerClient, std::size_t messagesPerThread) : threads(threadsPerClient), msgsPerThread(messagesPerThread), remaining(threadsPerClient* messagesPerThread), total(threadsPerClient* messagesPerThread) {}

        void Start()
        {
            ClientNetwork::GetInstance().RegisterReceiver(static_cast<PacketType>(PacketTypeStress::S2C_StressAck), [this](std::vector<std::uint8_t> data)
                {
                    auto anyVec = CommonNetwork::DisassembleData(std::span(data.data(), data.size()));

                    const StressRecord rec = std::any_cast<StressRecord>(anyVec[0]);
                    const std::uint64_t key = (std::uint64_t(rec.threadId) << 32) | rec.index;
                    const bool expected = seen.insert(key).second;

                    if (!expected)
                        duplicates.fetch_add(1, std::memory_order_relaxed);

                    if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
                        done.arrive_and_wait();
                });

            ProgressReporter reporter(remaining, total);

            for (std::size_t t = 0; t < threads; ++t)
                workers.emplace_back([this, tid = static_cast<std::uint32_t>(t)] { Worker(tid); });

            done.arrive_and_wait();

            for (auto& jt : workers)
                jt.join();

            reporter.Stop();

            std::cout << "Stress test finished: " << duplicates.load() << " duplicate / out-of-order packets\n";
        }

    private:

        class ProgressReporter
        {

        public:

            ProgressReporter(const std::atomic<std::size_t>& remaining, std::size_t total) : remaining(remaining), total(total)
            {
                thr = std::jthread(&ProgressReporter::Loop, this);
            }

            void Stop()
            {
                running.store(false, std::memory_order_release);
            }

        private:

            void Loop()
            {
                while (running.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::seconds(3));

                    const std::size_t rem = remaining.load(std::memory_order_acquire);
                    const std::size_t acked = total - rem;

                    std::cout << "Progress: " << acked << " / " << total << " acks received (" << rem << " remaining)\n";

                    if (rem == 0)
                        break;
                }
            }

            const std::atomic<std::size_t>& remaining;

            std::size_t total;
            std::atomic<bool> running = true;
            std::jthread thr;
        };


        void Worker(std::uint32_t threadId)
        {
            std::mt19937_64 rng(std::random_device{}());

            for (std::uint32_t i = 0; i < msgsPerThread; ++i)
            {
                StressRecord record{ threadId, i, rng() };
                ClientNetwork::GetInstance().Send(static_cast<PacketType>(PacketTypeStress::C2S_Stress), record);
            }
        }

        std::size_t threads;
        std::size_t msgsPerThread;
        std::vector<std::jthread> workers;

        std::atomic<std::size_t> remaining;
        std::atomic<std::size_t> duplicates = 0;
        std::unordered_set<std::uint64_t> seen;
        std::barrier<> done{ 2 };

        std::atomic<std::size_t> total = 0;
    };

    inline void StartStressServer()
    {
        StressServer::Activate();
    }

    inline void StartStressClient(std::size_t threadsPerClient, std::size_t messagesPerThread)
    {
        static StressClient stress(threadsPerClient, messagesPerThread);

        stress.Start();
    }
}

template <>
struct Blaster::Independent::Network::DataConversion<Blaster::Independent::Test::StressRecord> : Blaster::Independent::Network::DataConversionBase<DataConversion<Blaster::Independent::Test::StressRecord>, Blaster::Independent::Test::StressRecord>
{
    using Type = Blaster::Independent::Test::StressRecord;

    static void Encode(const Type& v, std::vector<std::uint8_t>& buf)
    {
        CommonNetwork::WriteTrivial(buf, v);
    }

    static std::any Decode(std::span<const std::uint8_t> bytes)
    {
        assert(bytes.size() == sizeof(Type));

        Type v;
        std::memcpy(&v, bytes.data(), sizeof(Type));

        return v;
    }
};
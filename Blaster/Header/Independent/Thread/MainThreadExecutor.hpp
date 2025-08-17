#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <memory>
#include <unordered_set>

namespace Blaster::Independent::Thread
{
    class MainThreadExecutor final
    {

    public:

        MainThreadExecutor(const MainThreadExecutor&) = delete;
        MainThreadExecutor(MainThreadExecutor&&) = delete;
        MainThreadExecutor& operator=(const MainThreadExecutor&) = delete;
        MainThreadExecutor& operator=(MainThreadExecutor&&) = delete;

        using TaskFunction = std::move_only_function<void()>;

        template <class F> requires std::invocable<F&>
        bool EnqueueTask(void* holder, F&& task)
        {
            std::scoped_lock lock(mutex);

            if (holder)
            {
                const auto [it, inserted] = pendingHolders.emplace(holder);

                if (!inserted)
                    return false;
            }

            tasks.emplace_back(Task{ holder, TaskFunction{std::forward<F>(task)} });

            return true;
        }

        void CancelTask(const void* holder)
        {
            std::scoped_lock lock(mutex);

            pendingHolders.erase(const_cast<void*>(holder));
        }

        bool CancelTaskHard(const void* holder)
        {
            std::scoped_lock lock(mutex);

            const auto h = const_cast<void*>(holder);
            bool removed = false;

            removed = std::erase_if(tasks, [h](const Task& t) { return t.holder == h; }) > 0 || removed;
            pendingHolders.erase(h);

            return removed;
        }

        void Execute()
        {
            std::deque<Task> local;
            {
                std::scoped_lock lock(mutex);
                local.swap(tasks);
            }

            if (local.empty())
                return;

            {
                std::scoped_lock lock(mutex);

                for (const auto& task : local)
                    if (task.holder) pendingHolders.erase(task.holder);
            }

            for (auto& task : local)
            {
                try
                {
                    if (task.function) task.function();
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[MainThreadExecutor] task threw: " << e.what() << '\n';
                }
                catch (...)
                {
                    std::cerr << "[MainThreadExecutor] task threw unknown exception\n";
                }
            }
        }

        [[nodiscard]] static MainThreadExecutor& GetInstance()
        {
            static MainThreadExecutor instance;

            return instance;
        }

    private:

        MainThreadExecutor() = default;

        struct Task
        {
            void* holder{};
            TaskFunction function{};
        };

        std::mutex mutex;
        std::deque<Task> tasks;
        std::unordered_set<void*> pendingHolders;
    };
}

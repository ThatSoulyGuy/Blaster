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

        void EnqueueTask(void* holder, std::function<void()> task)
        {
            std::scoped_lock guard(mutex);

            if (holder && !pendingHolders.emplace(holder).second)
                return;

            tasks.emplace(holder, std::move(task));
        }

        void CancelTask(const void* holder)
        {
            std::scoped_lock guard(mutex);

            pendingHolders.erase(const_cast<std::unordered_set<void *>::key_type>(holder));
        }

        void Execute()
        {
            std::queue< item_type > local;

            {
                std::scoped_lock guard( mutex );
                local.swap( tasks );
            }

            while (!local.empty())
            {
                auto [holder, function] = std::move(local.front());

                local.pop();

                if (holder)
                {
                    std::scoped_lock guard(mutex);
                    pendingHolders.erase(holder);
                }

                function();
            }
        }

        static MainThreadExecutor& GetInstance()
        {
            std::call_once(initFlag, []
            {
                instance.reset(new MainThreadExecutor());
            });

            return *instance;
        }

    private:

        MainThreadExecutor()  = default;

        using item_type = std::tuple<void*, std::function<void()>>;

        std::mutex mutex;
        std::queue<item_type> tasks;
        std::unordered_set<void*> pendingHolders;

        static std::once_flag initFlag;
        static std::unique_ptr<MainThreadExecutor> instance;
    };

    std::once_flag MainThreadExecutor::initFlag;
    std::unique_ptr<MainThreadExecutor> MainThreadExecutor::instance;
}

#pragma once

#include <mutex>
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Utility/SingletonManager.hpp"

#ifdef IS_SERVER
#include "Server/Network/ServerNetwork.hpp"
#endif

using namespace std::chrono_literals;
using namespace Blaster::Independent::Utility;

namespace Blaster::Independent::ECS
{
    class GameObjectManager final : public SingletonManager<std::shared_ptr<GameObject>, const std::string&>
    {

    public:

        GameObjectManager(const GameObjectManager&) = delete;
        GameObjectManager(GameObjectManager&&) = delete;
        GameObjectManager& operator=(const GameObjectManager&) = delete;
        GameObjectManager& operator=(GameObjectManager&&) = delete;

        std::shared_ptr<GameObject> Register(std::shared_ptr<GameObject> gameObject) override
        {
            return Register(gameObject, ".");
        }

        std::shared_ptr<GameObject> Register(std::shared_ptr<GameObject> gameObject, const std::string& path, bool markDirty = true)
        {
            assert(gameObject != nullptr && "Cannot register a null GameObject");

            if (path == ".")
            {
                std::string name = gameObject->GetName();

                if (rootGameObjectMap.contains(gameObject->GetName()))
                {
                    std::cerr << "Root map already contains game object '" << gameObject->GetName() << "'!" << std::endl;
                    return nullptr;
                }

                rootGameObjectMap.insert({ gameObject->GetName(), std::move(gameObject) });

                if (markDirty)
                    Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(rootGameObjectMap[name]);

                return rootGameObjectMap[name];
            }

            auto parentOptional = Get(path);

            if (!parentOptional.has_value())
            {
                std::cerr << "Parent path '" << path << "' does not exist; cannot register child '" << gameObject->GetName() << "'!" << std::endl;
                return nullptr;
            }

            if (markDirty)
                Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(gameObject);

            auto result = parentOptional.value()->AddChild(std::move(gameObject));

#ifdef IS_SERVER
            if (result->GetOwningClient().has_value() && result->GetOwningClient().value() != 0 && Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(result->GetOwningClient().value()).has_value())
                Blaster::Server::Network::ServerNetwork::GetInstance().GetClient(result->GetOwningClient().value()).value()->ownedGameObjectList.insert({ result->GetAbsolutePath(), std::static_pointer_cast<IGameObjectSynchronization>(result) });
#endif

            return result;
        }

        void Unregister(const std::string& path) override
        {
            Unregister(path, true);
        }

        void Unregister(const std::string& path, bool markDirty)
        {
            auto gameObjectOptional = Get(path);

            if (!gameObjectOptional.has_value())
            {
                std::cerr << "Cannot unregister; path '" << path << "' does not exist!" << std::endl;
                return;
            }

            const auto gameObject = gameObjectOptional.value();

            gameObject->MarkDestroyed();

            if (markDirty)
                Blaster::Independent::ECS::Synchronization::SenderSynchronization::GetInstance().MarkDirty(gameObject);

            const std::string absolutePath = gameObject->GetAbsolutePath();
            const bool isRoot = absolutePath.find('.') == std::string::npos;

            if (isRoot)
            {
                rootGameObjectMap.erase(gameObject->GetName());
                return;
            }

            const std::string_view absoluteView = absolutePath;
            const std::string_view parentPathView = absoluteView.substr(0, absoluteView.rfind('.'));
            const std::string parentPath(parentPathView);
            const std::string childName = gameObject->GetName();

            auto parentOptional = Get(parentPath);

            if (!parentOptional.has_value())
            {
                std::cerr << "Internal inconsistency: parent '" << parentPath << "' not found while unregistering '" << absolutePath << "'!" << std::endl;
                return;
            }

            parentOptional.value()->RemoveChild(childName, markDirty);
        }

        void UnregisterDeferred(const std::string& path, bool markDirty = true)
        {
            unregistrationQueue.push({ path, markDirty });
        }

        bool Has(const std::string& path) const override
        {
            return GetInternal(path).has_value();
        }

        std::optional<std::shared_ptr<GameObject>> Get(const std::string& path) override
        {
            return GetInternal(path);
        }

        std::vector<std::shared_ptr<GameObject>> GetAll() const override
        {
            std::vector<std::shared_ptr<GameObject>> result;

            result.reserve(rootGameObjectMap.size());
            std::ranges::transform(rootGameObjectMap, std::back_inserter(result), [](const auto& pair) { return pair.second; });

            return result;
        }

        void SetCamera(std::optional<std::shared_ptr<Client::Render::Camera>> camera)
        {
            this->camera = camera;
        }

        [[nodiscard]]
        std::optional<std::shared_ptr<Client::Render::Camera>> GetCamera() const
        {
            return camera;
        }

        void Update()
        {
            IterationGuard guard(*this);

            for (const auto& gameObject : rootGameObjectMap | std::views::values)
                gameObject->Update();
        }

        void Render(const std::optional<std::shared_ptr<Client::Render::Camera>>& camera)
        {
            if (!camera.has_value())
            {
                std::cerr << "No camera! Skipping rendering this frame..." << std::endl;
                return;
            }

            IterationGuard guard(*this);

            for (const auto& gameObject : rootGameObjectMap | std::views::values)
                gameObject->Render(camera.value());
        }

        void RenderUI()
        {
            IterationGuard guard(*this);

            std::vector<std::shared_ptr<GameObject>> ordered{ rootGameObjectMap.size() };

            std::ranges::transform(rootGameObjectMap, ordered.begin(), [](const auto& pair) { return pair.second; });

            std::ranges::sort(ordered, [](const auto& a, const auto& b) { return a->GetCreationIndex() < b->GetCreationIndex(); });

            for (const auto& object : ordered)
                object->RenderUI();
        }

        void Clear()
        {
            unregistrationQueue = {};

            rootGameObjectMap.clear();

            camera = std::nullopt;
        }

        static GameObjectManager& GetInstance()
        {
            std::call_once(initializationFlag, []()
                {
                    instance = std::unique_ptr<GameObjectManager>(new GameObjectManager());
                });

            return *instance;
        }

    private:

        class IterationGuard
        {

        public:

            explicit IterationGuard(GameObjectManager& m) : manager(m)
            {
                ++manager.iterationDepth;
            }

            ~IterationGuard()
            {
                --manager.iterationDepth;

                if (manager.iterationDepth == 0)
                    manager.FlushQueues();
            }

        private:

            GameObjectManager& manager;

        };

        struct PendingUnregistration
        {
            std::string path;

            bool markDirty;
        };

        GameObjectManager() = default;

        static std::vector<std::string> SplitPath(const std::string& path)
        {
            std::vector<std::string> segments;
            std::string segment;
            std::stringstream stringStream(path);

            while (std::getline(stringStream, segment, '.'))
            {
                if (!segment.empty())
                    segments.push_back(segment);
            }

            return segments;
        }

        std::optional<std::shared_ptr<GameObject>> GetInternal(const std::string& path) const
        {
            if (path == ".")
                return std::nullopt;

            const std::vector<std::string> segments = SplitPath(path);

            if (segments.empty())
                return std::nullopt;

            auto rootIterator = rootGameObjectMap.find(segments.front());

            if (rootIterator == rootGameObjectMap.end())
                return std::nullopt;

            std::shared_ptr<GameObject> current = rootIterator->second;

            for (std::size_t index = 1; index < segments.size(); ++index)
            {
                if (!current->HasChild(segments[index]))
                    return std::nullopt;

                current = current->GetChild(segments[index]).value();
            }

            return current;
        }

        void FlushQueues()
        {
            while (!unregistrationQueue.empty())
            {
                const auto [path, markDirty] = std::move(unregistrationQueue.front());

                unregistrationQueue.pop();
                Unregister(path, markDirty);
            }
        }

        [[nodiscard]]
        bool IsIterating() const
        {
            return iterationDepth != 0;
        }

        std::unordered_map<std::string, std::shared_ptr<GameObject>> rootGameObjectMap = {};

        std::optional<std::shared_ptr<Client::Render::Camera>> camera;

        std::queue<PendingUnregistration> unregistrationQueue;

        std::uint32_t iterationDepth{ 0 };

        static std::once_flag initializationFlag;
        static std::unique_ptr<GameObjectManager> instance;
    };

    std::once_flag GameObjectManager::initializationFlag;
    std::unique_ptr<GameObjectManager> GameObjectManager::instance;
}
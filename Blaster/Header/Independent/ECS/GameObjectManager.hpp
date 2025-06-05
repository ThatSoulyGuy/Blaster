#pragma once

#include <mutex>
#include "Independent/ECS/GameObject.hpp"
#include "Independent/Utility/SingletonManager.hpp"

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
            if (gameObjectMap.contains(gameObject->GetName()))
            {
                std::cout << "Game object map for game object manager already contains game object '" << gameObject->GetName() << "'!" << std::endl;
                return nullptr;
            }

            std::string name = gameObject->GetName();

            gameObjectMap.insert({ name, std::move(gameObject) });

            return gameObjectMap[name];
        }

        void Unregister(const std::string& name) override
        {
            if (!gameObjectMap.contains(name))
            {
                std::cout << "Game object map for game object manager doesn't contain game object '" << name << "'!" << std::endl;
                return;
            }

            gameObjectMap.erase(name);
        }

        bool Has(const std::string& name) const override
        {
            return gameObjectMap.contains(name);
        }

        std::optional<std::shared_ptr<GameObject>> Get(const std::string& name) override
        {
            return gameObjectMap.contains(name) ? std::make_optional(gameObjectMap[name]) : std::nullopt;
        }

        std::vector<std::shared_ptr<GameObject>> GetAll() const override
        {
            std::vector<std::shared_ptr<GameObject>> result;

            result.reserve(gameObjectMap.size());

            std::ranges::transform(gameObjectMap, std::back_inserter(result), [](const auto& pair) { return pair.second; });

            return result;
        }

        void Update()
        {
            for (const auto& gameObject : gameObjectMap | std::views::values)
                gameObject->Update();
        }

        void Render(const std::optional<std::shared_ptr<Client::Render::Camera>>& camera)
        {
            if (!camera.has_value())
            {
                std::cerr << "No camera! Skipping rendering this frame..." << std::endl;

                return;
            }

            for (const auto& gameObject : gameObjectMap | std::views::values)
                gameObject->Render(camera.value());
        }

        static GameObjectManager& GetInstance()
        {
            std::call_once(initializationFlag, [&]()
            {
                instance = std::unique_ptr<GameObjectManager>(new GameObjectManager());
            });

            return *instance;
        }

    private:

        GameObjectManager() = default;

        std::unordered_map<std::string, std::shared_ptr<GameObject>> gameObjectMap = {};

        static std::once_flag initializationFlag;
        static std::unique_ptr<GameObjectManager> instance;

    };

    std::once_flag GameObjectManager::initializationFlag;
    std::unique_ptr<GameObjectManager> GameObjectManager::instance;
}
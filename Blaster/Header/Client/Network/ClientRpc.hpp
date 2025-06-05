#pragma once

#include <ranges>
#include "Client/Network/ClientNetwork.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Network/CommonNetwork.hpp"
#include "Independent/Network/CommonRpc.hpp"

using namespace Blaster::Independent::Network;

namespace Blaster::Client::Network
{
    class ClientRpc final
    {

    public:

        using PromiseBase = std::shared_ptr<void>;

        template <class T>
        using Promise = std::shared_ptr<std::promise<T>>;

        template <class T>
        using Future = std::future<T>;

        ClientRpc(const ClientRpc&) = delete;
        ClientRpc(ClientRpc&&) = delete;
        ClientRpc& operator=(const ClientRpc&) = delete;
        ClientRpc& operator=(ClientRpc&&) = delete;

        static Future<std::shared_ptr<GameObject>> CreateGameObject(std::string name)
        {
            const std::vector<std::uint8_t> buffer(name.begin(), name.end());

            return MakeCall<std::shared_ptr<GameObject>>(RpcType::C2S_CreateGameObject, buffer);
        }

        static Future<void> DestroyGameObject(std::string name)
        {
            std::vector<std::uint8_t> buffer(name.begin(), name.end());

            return MakeCall<void>(RpcType::C2S_DestroyGameObject, buffer);
        }

        static Future<std::shared_ptr<Component>> AddComponent(std::string gameObjectName, const std::shared_ptr<Component>& component)
        {
            auto componentBytes = NetworkSerialize::ObjectToBytes(component);

            std::vector<std::uint8_t> buffer(gameObjectName.begin(), gameObjectName.end());

            buffer.push_back('\0');

            const auto type = component->GetTypeName();

            buffer.insert(buffer.end(), type.begin(), type.end());
            buffer.push_back('\0');
            buffer.insert(buffer.end(), componentBytes.begin(), componentBytes.end());

            return MakeCall<std::shared_ptr<Component>>(RpcType::C2S_AddComponent, buffer);
        }

        static Future<void> RemoveComponent(std::string gameObjectName, std::string type)
        {
            std::vector<std::uint8_t> buffer(gameObjectName.begin(), gameObjectName.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), type.begin(), type.end());

            return MakeCall<void>(RpcType::C2S_RemoveComponent, buffer);
        }

        static Future<void> AddChild(std::string parent, std::string child)
        {
            std::vector<std::uint8_t> buffer(parent.begin(), parent.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), child.begin(), child.end());

            return MakeCall<void>(RpcType::C2S_AddChild, buffer);
        }

        static Future<void> RemoveChild(std::string parent, std::string child)
        {
            std::vector<std::uint8_t> buffer(parent.begin(), parent.end());

            buffer.push_back('\0');
            buffer.insert(buffer.end(), child.begin(), child.end());

            return MakeCall<void>(RpcType::C2S_RemoveChild, buffer);
        }

        static Future<void> TranslateTo(std::string gameObject, const Vector<float, 3> target, const float seconds)
        {
            std::vector<std::uint8_t> buffer(gameObject.begin(), gameObject.end());
            buffer.push_back('\0');

            auto pushBlob = [&](const auto& obj)
            {
                auto blob = NetworkSerialize::ObjectToBytes(obj);
                const auto length = static_cast<std::uint32_t>(blob.size());

                buffer.insert(buffer.end(), reinterpret_cast<const std::uint8_t*>(&length), reinterpret_cast<const std::uint8_t*>(&length) + sizeof length);
                buffer.insert(buffer.end(), blob.begin(), blob.end());
            };

            auto split = [&](const std::string& path)
            {
                std::vector<std::string> parts;
                std::stringstream stream(path);
                std::string part;

                while (std::getline(stream, part, '.'))
                {
                    if (!part.empty())
                        parts.push_back(part);
                }

                return parts;
            };

            pushBlob(target);
            pushBlob(seconds);

            auto parts = split(gameObject);

            const auto leaf = std::ranges::fold_left(parts | std::views::drop(1), GameObjectManager::GetInstance().Get(parts.front()),
                [](const std::optional<std::shared_ptr<GameObject>>& current, const std::string_view name)
                {
                    return current.and_then([&](const std::shared_ptr<GameObject>& next){ return next->GetChild(std::string(name.begin(), name.end())); });
                });

            if (leaf.has_value())
                leaf.value()->GetTransform()->SetLocalPosition(target);

            return MakeCall<void>(RpcType::C2S_TranslateTo, buffer);
        }

        static void HandleReply(std::vector<std::uint8_t> data)
        {
            RpcHeader header{};
            std::memcpy(&header, data.data(), sizeof header);
            data.erase(data.begin(), data.begin() + sizeof header);

            PromiseBase promiseBase;

            {
                std::lock_guard lock(mutex);

                auto it = pendingMap.find(header.id);

                if (it == pendingMap.end())
                    return;

                promiseBase = it->second;
                pendingMap.erase(it);
            }

            std::cout << "[RPC-RX] id=" << header.id << "  type=" << static_cast<int>(header.type) << '\n';

            switch (header.type)
            {
                case RpcType::S2C_CreateGameObject:
                {
                    const std::string name(data.begin(), data.end());

                    auto promise = std::static_pointer_cast<std::promise<std::shared_ptr<GameObject>>>(promiseBase);

                    auto toStringView = [](auto&& sub)
                    {
                        return std::string_view(&*sub.begin(), std::ranges::distance(sub));
                    };

                    auto getOrCreateRoot = [&](const std::string_view id)
                    {
                        if (auto opt = GameObjectManager::GetInstance().Get(std::string{id}))
                            return *opt;

                        auto root = GameObject::Create(std::string{id});

                        GameObjectManager::GetInstance().Register(root);

                        return root;
                    };

                    auto getOrCreateChild = [](const std::shared_ptr<GameObject>& parent, const std::string_view id)
                    {
                        if (auto optionalChild = parent->GetChild(std::string{ id }))
                            return *optionalChild;

                        auto child = GameObject::Create(std::string{ id });

                        parent->AddChild(child);

                        return child;
                    };

                    auto tokenView = name | std::views::split('.') | std::views::transform(toStringView);
                    auto tokenIterator = tokenView.begin();

                    if (tokenIterator == tokenView.end())
                    {
                        promise->set_value(nullptr);
                        break;
                    }

                    std::shared_ptr<GameObject> node = getOrCreateRoot(*tokenIterator++);

                    for (auto id : tokenView | std::views::drop(1))
                        node = getOrCreateChild(node, id);

                    promise->set_value(node);

                    break;
                }

                case RpcType::S2C_AddComponent:
                {
                    const auto promise = std::static_pointer_cast<std::promise<std::shared_ptr<Component>>>(promiseBase);

                    auto nul1 = std::ranges::find(data, '\0');

                    if (nul1 == data.end())
                        throw std::runtime_error("malformed packet");

                    auto nul2 = std::ranges::find(nul1 + 1, data.end(), '\0');

                    if (nul2 == data.end())
                        throw std::runtime_error("malformed packet");

                    std::string_view path {reinterpret_cast<char*>(data.data()), static_cast<std::size_t>(nul1 - data.begin())};

                    std::string_view type {reinterpret_cast<char*>(&*(nul1 + 1)), static_cast<std::size_t>(nul2 - (nul1 + 1))};

                    const std::span blob{ &*(nul2 + 1), static_cast<std::size_t>(data.end() - (nul2 + 1)) };

                    auto toStringView = [](auto&& sub) { return std::string_view(&*sub.begin(), std::ranges::distance(sub)); };

                    auto tokenView = path | std::views::split('.') | std::views::transform(toStringView);

                    auto tokenIterator = tokenView.begin();

                    if (tokenIterator == tokenView.end())
                        throw std::runtime_error("empty game-object path");

                    auto leaf = GameObjectManager::GetInstance().Get(std::string{*tokenIterator});

                    for (auto id : tokenView | std::views::drop(1))
                        leaf = leaf.and_then([&](auto& n){ return n->GetChild(std::string{id}); });

                    if (!leaf)
                    {
                        std::cerr << "Unknown game-object '" << path << "'\n";

                        promise->set_value(nullptr);

                        break;
                    }

                    auto destinationGameObjet = *leaf;

                    std::shared_ptr<Component> component;

                    NetworkSerialize::ObjectFromBytes(std::vector(blob.begin(), blob.end()), component);

                    if (!component)
                    {
                        if (auto raw = ComponentFactory::Instantiate(std::string{type}))
                            component = std::static_pointer_cast<Component>(raw);
                    }

                    if (!component)
                    {
                        promise->set_value(nullptr);
                        break;
                    }

                    if (destinationGameObjet->HasComponentDynamic(std::string{type}))
                        destinationGameObjet->RemoveComponentDynamic(std::string{type});

                    promise->set_value(destinationGameObjet->AddComponentDynamic(component));

                    break;
                }

                default:
                    std::static_pointer_cast<std::promise<void>>(promiseBase)->set_value();
            }
        }

    private:

        ClientRpc() = default;

        template<class T>
        static Future<T> MakeCall(const RpcType type, std::vector<std::uint8_t> payload)
        {
            const std::uint64_t id = nextId.fetch_add(1, std::memory_order_relaxed);

            const RpcHeader header{ id, type };

            std::vector<std::uint8_t> packet(sizeof header);
            std::memcpy(packet.data(), &header, sizeof header);
            packet.insert(packet.end(), payload.begin(), payload.end());

            auto promise = std::make_shared<std::promise<T>>();

            {
                std::lock_guard lock(mutex);
                pendingMap[id] = promise;
            }

            ClientNetwork::GetInstance().Send(PacketType::C2S_Rpc, packet);

            return promise->get_future();
        }

        static std::mutex mutex;

        static std::unordered_map<std::uint64_t, PromiseBase> pendingMap;
        static std::atomic_uint64_t nextId;

    };

    std::mutex ClientRpc::mutex;
    std::unordered_map<std::uint64_t, ClientRpc::PromiseBase> ClientRpc::pendingMap = {};
    std::atomic_uint64_t ClientRpc::nextId = 1;
}
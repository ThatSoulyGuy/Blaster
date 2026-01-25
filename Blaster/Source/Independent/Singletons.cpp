#include "Independent/ECS/Synchronization/CommonSynchronization.hpp"
#include "Client/Core/InputManager.hpp"

thread_local std::vector<Blaster::Independent::ECS::Synchronization::DirtyRequest> Blaster::Independent::ECS::Synchronization::gDeferredDirty;
#include "Client/Core/Window.hpp"
#include "Client/Network/ClientNetwork.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/TextureManager.hpp"
#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/ECS/Synchronization/SyncTracker.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Item/ItemRegistry.hpp"
#include "Independent/Physics/PhysicsWorld.hpp"
#include "Independent/Utility/Time.hpp"
#include "Server/Network/ServerNetwork.hpp"

std::once_flag Blaster::Client::Core::Window::initializationFlag;
std::unique_ptr<Blaster::Client::Core::Window> Blaster::Client::Core::Window::instance;

std::once_flag Blaster::Client::Network::ClientNetwork::initializationFlag;
std::unique_ptr<Blaster::Client::Network::ClientNetwork> Blaster::Client::Network::ClientNetwork::instance;

std::once_flag Synchronization::SyncTracker::initializationFlag;
std::unique_ptr<Synchronization::SyncTracker> Synchronization::SyncTracker::instance;

std::once_flag Synchronization::SenderSynchronization::initializationFlag;
std::unique_ptr<Synchronization::SenderSynchronization> Synchronization::SenderSynchronization::instance;

std::once_flag GameObjectManager::initializationFlag;
std::unique_ptr<GameObjectManager> GameObjectManager::instance;

std::once_flag Time::initializationFlag;
std::unique_ptr<Time> Time::instance;

std::once_flag Blaster::Server::Network::ServerNetwork::initializationFlag;
std::unique_ptr<Blaster::Server::Network::ServerNetwork> Blaster::Server::Network::ServerNetwork::instance;

std::once_flag Blaster::Client::Render::ShaderManager::initializationFlag;
std::unique_ptr<Blaster::Client::Render::ShaderManager> Blaster::Client::Render::ShaderManager::instance;

std::once_flag Blaster::Client::Render::TextureManager::initializationFlag;
std::unique_ptr<Blaster::Client::Render::TextureManager> Blaster::Client::Render::TextureManager::instance;

std::unique_ptr<InputManager> InputManager::instance = nullptr;
std::once_flag InputManager::initializationFlag;

std::once_flag Blaster::Independent::Physics::PhysicsWorld::initializationFlag;
std::unique_ptr<Blaster::Independent::Physics::PhysicsWorld> Blaster::Independent::Physics::PhysicsWorld::instance;

std::once_flag Blaster::Independent::Item::ItemRegistry::initializationFlag;
std::unique_ptr<Blaster::Independent::Item::ItemRegistry> Blaster::Independent::Item::ItemRegistry::instance;
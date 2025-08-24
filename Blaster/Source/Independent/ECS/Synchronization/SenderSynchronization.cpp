#include "Independent/ECS/Synchronization/SenderSynchronization.hpp"
#include "Independent/ECS/GameObject.hpp"

namespace Blaster::Independent::ECS::Synchronization
{
    std::shared_ptr<IGameObjectSynchronization> SenderSynchronization::CastGameObject(const std::shared_ptr<GameObject>& object)
    {
        return std::static_pointer_cast<IGameObjectSynchronization>(object);
    }

}
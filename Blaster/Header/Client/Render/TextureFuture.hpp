#pragma once

#include "Client/Render/TextureManager.hpp"

namespace Blaster::Client::Render
{
    class TextureFuture final : public Component
    {

    public:

        TextureFuture(const TextureFuture&) = delete;
        TextureFuture(TextureFuture&&) = delete;
        TextureFuture& operator=(const TextureFuture&) = delete;
        TextureFuture& operator=(TextureFuture&&) = delete;

        void Initialize()
        {
#ifndef IS_SERVER
            GetGameObject()->AddComponent(TextureManager::GetInstance().Get(path).value());
#endif
        }

        static std::shared_ptr<TextureFuture> Create(const std::string& path)
        {
            std::shared_ptr<TextureFuture> result(new TextureFuture());

            result->path = path;

            return result;
        }

    private:

        TextureFuture() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(path);
        }

        std::string path;

        DESCRIBE_AND_REGISTER(TextureFuture, (Component), (), (), (path))

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::TextureFuture, 29874)
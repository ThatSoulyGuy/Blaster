#pragma once

#include <functional>
#include <string>
#include <array>
#include <utility>
#include <boost/serialization/string.hpp>
#include "Client/Core/InputManager.hpp"
#include "Client/Render/Shader.hpp"
#include "Client/Render/Texture.hpp"
#include "Client/UI/UIElement.hpp"

using namespace Blaster::Client::Render;

namespace Blaster::Client::UI
{
    class UIElementImage : public UIElement
    {

    public:

        void SetTexture(const std::shared_ptr<Texture>& texture)
        {
            this->texture = texture;
        }

        const std::shared_ptr<Texture>& GetTexture() const
        {
            return texture;
        }

        void Update() override
        {
            if (auto mesh = GetMesh(); texture)
            {
                mesh->QueueShaderCall("uTexture", 0);
                mesh->QueueRenderCall([t = texture] { t->Bind(0); });
            }
        }

    private:

        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(texture);
        } 
        
        std::shared_ptr<Texture> texture;

        DESCRIBE_AND_REGISTER(UIElementImage, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::UIElementImage, 28712)
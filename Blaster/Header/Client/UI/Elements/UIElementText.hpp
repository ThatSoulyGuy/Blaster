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
    class UIElementText : public UIElement
    {

    public:

        struct Font
        {
            std::shared_ptr<Render::Texture> atlas;

            struct Glyph
            {
                Vector<float, 2> uvMin, uvMax;
                Vector<float, 2> size, bearing;
                
                float advance;
            };

            std::array<Glyph, 128> table;
        };

        void SetFont(const std::shared_ptr<Font>& f)
        {
            font = f;
            
            regenerateMesh = true;
        }

        void SetString(const std::string& s)
        {
            text = s;
            
            regenerateMesh = true;
        }

        void SetColor(const Vector<float, 3>& c)
        {
            tint = c;
        }

        const std::string& GetString() const noexcept
        {
            return text;
        }

        const std::shared_ptr<Font>& GetFont() const noexcept
        {
            return font;
        }

        void Initialize() override
        {
            UIElement::Initialize();

            regenerateMesh = true;
        }

        void Update() override
        {
            if (regenerateMesh && font)
            {
                GenerateMesh();
                regenerateMesh = false;
            }

            if (font && font->atlas)
            {
                auto mesh = GetMesh();
                mesh->QueueShaderCall("uTexture", 0);
                mesh->QueueShaderCall("uTint", tint);
                mesh->QueueRenderCall([a = font->atlas] { a->Bind(0); });
            }
        }

    private:

        void GenerateMesh()
        {
            auto mesh = GetMesh();
            std::vector<Render::Vertices::UIVertex> v;
            std::vector<uint32_t>                   i;

            Vector<float, 2> cursor = { 0,0 };
            for (char c : text)
            {
                if (c < 32 || c >= 127) continue;
                const auto& g = font->table[static_cast<size_t>(c)];

                Vector<float, 2> p0 = cursor + Vector<float, 2>{ g.bearing.x(), -g.bearing.y()};
                Vector<float, 2> p1 = p0 + Vector<float, 2>{ g.size.x(), 0};
                Vector<float, 2> p2 = p0 + Vector<float, 2>{ 0, -g.size.y()};
                Vector<float, 2> p3 = p0 + Vector<float, 2>{ g.size.x(), -g.size.y()};

                uint32_t start = static_cast<uint32_t>(v.size());

                auto push = [&](const Vector<float, 2>& pos, const Vector<float, 2>& uv)
                    {
                        v.push_back({ {pos.x(), pos.y(), 0.f}, {tint.x(), tint.y(), tint.z()}, uv });
                    };

                push(p0, g.uvMin);
                push(p1, { g.uvMax.x(), g.uvMin.y() });
                push(p2, { g.uvMin.x(), g.uvMax.y() });
                push(p3, g.uvMax);

                i.insert(i.end(), { start + 0, start + 1, start + 2,  start + 1, start + 3, start + 2 });

                cursor.x() += g.advance;
            }

            mesh->SetVertices(std::move(v));
            mesh->SetIndices(std::move(i));
        }

        std::shared_ptr<Font> font;
        std::string text = "Text";

        Vector<float, 3> tint = { 1,1,1 };

        bool regenerateMesh = false;

        friend class boost::serialization::access;

        template<typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive& boost::serialization::base_object<Component>(*this);
            
            archive & BOOST_SERIALIZATION_NVP(text);
            archive & BOOST_SERIALIZATION_NVP(tint);
            archive & BOOST_SERIALIZATION_NVP(font);
        }

        DESCRIBE_AND_REGISTER(UIElementText, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::UIElementImage, 29712)
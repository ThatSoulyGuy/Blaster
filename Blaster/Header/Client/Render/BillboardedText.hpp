#pragma once

#ifndef IS_SERVER

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glad/glad.h>

#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Math/Vector.hpp"
#include "Client/Render/Vertices/UIVertex.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/Texture.hpp"

using namespace Blaster::Client::Render::Vertices;

namespace Blaster::Client::Render
{
    class BillboardedText final : public Blaster::Independent::ECS::Component
    {

    public:

        void Initialize() override
        {
            using namespace Blaster::Independent::ECS;

            std::vector<UIVertex> vertices =
            {
                { { -0.5f, -0.5f, 0.f }, {1,1,1}, {0,1} },
                { {  0.5f, -0.5f, 0.f }, {1,1,1}, {1,1} },
                { { -0.5f,  0.5f, 0.f }, {1,1,1}, {0,0} },
                { {  0.5f,  0.5f, 0.f }, {1,1,1}, {1,0} },
            };

            std::vector<uint32_t> indices =
            {
                0, 1, 2,
                1, 3, 2,
                2, 1, 0,
                2, 3, 1
            };

            mesh = GetGameObject()->AddComponent(Mesh<UIVertex>::Create(vertices, indices));
            mesh->Generate();

            GetGameObject()->AddComponent(ShaderManager::GetInstance().Get("blaster.textured_billboard").value());

            RebuildTexture();

            ApplyScaleFromTexture();
        }

        void Render(const std::shared_ptr<Camera>&) override
        {
            if (const auto camera = Blaster::Independent::ECS::GameObjectManager::GetInstance().GetCamera())
            {
                auto transform = GetGameObject()->GetTransform3d();
                const auto camT = camera.value()->GetGameObject()->GetTransform3d();

                const Vector<float, 3> cameraPos = camT->GetWorldPosition();
                const Vector<float, 3> nameTagPosition = transform->GetWorldPosition();

                Vector<float, 3> toCam = cameraPos - nameTagPosition;
                toCam.y() = 0.0f;

                constexpr float kEps = 1e-6f;

                if (Vector<float, 3>::LengthSquared(toCam) > kEps)
                {
                    toCam = Vector<float, 3>::Normalize(toCam);

                    float yawDeg = std::atan2(toCam.x(), toCam.z()) * 180.0f / 3.14159265f;

                    auto r = transform->GetLocalRotation();

                    auto normalize = [](float a)
                        {
                            a = std::fmod(a + 180.f, 360.f);

                            if (a < 0.f) a += 360.f;
                                return a - 180.f;
                        };

                    r.y() = normalize(yawDeg);

                    transform->SetLocalRotation(r);
                }
            }

            if (mesh && texture)
            {
                mesh->QueueShaderCall("uTexture", 0);
                mesh->QueueRenderCall([&] { texture->Bind(0); });
            }
        }

        void SetText(const std::string& name)
        {
            if (displayText == name)
                return;

            displayText = name;

            RebuildTexture();
            ApplyScaleFromTexture();
        }

        void SetColor(const Vector<float, 3>& color)
        {
            tint = color;
            RebuildTexture();
        }

        void SetDesiredHeight(float h)
        {
            desiredHeightWS = std::max(h, 0.001f);
            ApplyScaleFromTexture();
        }

        static std::shared_ptr<BillboardedText> CreateForTeam(const std::string& name, const AssetPath& path, bool isRedTeam, float desiredWorldHeight = 4.0f, float verticalOffset = 22.0f)
        {
            Vector<float, 3> color = isRedTeam ? Vector<float, 3>{1.0f, 0.35f, 0.35f} : Vector<float, 3>{ 0.35f, 0.55f, 1.0f };

            return Create(name, path, desiredWorldHeight, verticalOffset, color);
        }

        static std::shared_ptr<BillboardedText> Create(const std::string& text, const AssetPath& path, float desiredWorldHeight = 4.0f, float verticalOffset = 22.0f, Vector<float, 3> color = { 1.0f,1.0f,1.0f }, unsigned fontPixelHeight = 48, unsigned paddingPixels = 4)
        {
            auto result = std::shared_ptr<BillboardedText>(new BillboardedText());

            result->displayText = text;
            result->path = path;
            result->desiredHeightWS = desiredWorldHeight;
            result->offsetY = verticalOffset;
            result->tint = color;
            result->fontPixelSize = fontPixelHeight;
            result->paddingPixels = paddingPixels;

            return result;
        }

    private:

        BillboardedText() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(displayText);
            archive & BOOST_SERIALIZATION_NVP(path);
            archive & BOOST_SERIALIZATION_NVP(tint);
            archive & BOOST_SERIALIZATION_NVP(desiredHeightWS);
            archive & BOOST_SERIALIZATION_NVP(offsetY);
            archive & BOOST_SERIALIZATION_NVP(fontPixelSize);
            archive & BOOST_SERIALIZATION_NVP(paddingPixels);
        }

        void RebuildTexture()
        {
            if (displayText.empty())
            {
                std::vector<std::uint8_t> rgba(4 * 4 * 4, 0);
                texture = Texture::CreateFromMemory("nametag_empty", { 4,4 }, rgba, true, GL_RGBA8, GL_RGBA);

                return;
            }

            static FT_Library s_ft = nullptr;

            if (!s_ft)
            {
                if (const FT_Error e = FT_Init_FreeType(&s_ft); e != 0)
                    throw std::runtime_error("FreeType initialization failed (BillboardedText).");
            }

            const std::string path = this->path.GetFullPath();

            FT_Face face{};

            if (const FT_Error e = FT_New_Face(s_ft, path.c_str(), 0, &face); e != 0)
                throw std::runtime_error("Failed to load font face for BillboardedText.");

            if (const FT_Error e = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(fontPixelSize)); e != 0)
                throw std::runtime_error("FT_Set_Pixel_Sizes failed (BillboardedText).");

            std::uint32_t width = 0;
            std::uint32_t maxTop = 0, maxBottom = 0;

            auto considerChar = [&](char c)
                {
                    if (c < 32 || c > 126)
                        return;

                    FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER);
                    const FT_GlyphSlot g = face->glyph;

                    width += static_cast<std::uint32_t>(g->advance.x >> 6);
                    maxTop = std::max<std::uint32_t>(maxTop, static_cast<std::uint32_t>(g->bitmap_top));

                    const std::uint32_t below = (g->bitmap.rows > g->bitmap_top) ? static_cast<std::uint32_t>(g->bitmap.rows - g->bitmap_top) : 0u;

                    maxBottom = std::max<std::uint32_t>(maxBottom, below);
                };

            for (char c : displayText)
                considerChar(c);

            std::uint32_t height = std::max<std::uint32_t>(1u, maxTop + maxBottom);

            width += paddingPixels * 2;
            height += paddingPixels * 2;

            std::vector<std::uint8_t> rgba(width * height * 4, 0u);

            std::uint32_t penX = paddingPixels;

            constexpr std::uint8_t kBgA = 112;
            for (std::size_t i = 0; i < rgba.size(); i += 4)
            {
                rgba[i + 0] = 0;
                rgba[i + 1] = 0;
                rgba[i + 2] = 0;
                rgba[i + 3] = kBgA;
            }

            const std::uint8_t cr = static_cast<std::uint8_t>(std::clamp(tint.x() * 255.f, 0.f, 255.f));
            const std::uint8_t cg = static_cast<std::uint8_t>(std::clamp(tint.y() * 255.f, 0.f, 255.f));
            const std::uint8_t cb = static_cast<std::uint8_t>(std::clamp(tint.z() * 255.f, 0.f, 255.f));

            auto blitGlyph = [&](char c)
                {
                    if (c < 32 || c > 126)
                        return;

                    FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER);

                    const FT_GlyphSlot g = face->glyph;
                    const FT_Bitmap& bm = g->bitmap;

                    const int dstOriginX = static_cast<int>(penX) + g->bitmap_left + static_cast<int>(paddingPixels);
                    const int dstOriginY = static_cast<int>(paddingPixels) + static_cast<int>(maxTop) - g->bitmap_top;

                    for (unsigned row = 0; row < bm.rows; ++row)
                    {
                        const std::uint8_t* src = bm.buffer + row * bm.pitch;
                        const int y = dstOriginY + static_cast<int>(row);

                        if (y < 0 || y >= static_cast<int>(height))
                            continue;

                        for (unsigned col = 0; col < bm.width; ++col)
                        {
                            const int x = dstOriginX + static_cast<int>(col);

                            if (x < 0 || x >= static_cast<int>(width))
                                continue;

                            const std::uint8_t a_src = src[col];

                            if (a_src == 0)
                                continue;

                            const std::size_t i = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;

                            const std::uint8_t a_dst = rgba[i + 3];

                            const std::uint16_t Aout = static_cast<std::uint16_t>(a_src + ((a_dst * (255 - a_src) + 127) / 255));

                            if (Aout > 0)
                            {
                                rgba[i + 0] = static_cast<std::uint8_t>((cr * a_src + Aout / 2) / Aout);
                                rgba[i + 1] = static_cast<std::uint8_t>((cg * a_src + Aout / 2) / Aout);
                                rgba[i + 2] = static_cast<std::uint8_t>((cb * a_src + Aout / 2) / Aout);
                                rgba[i + 3] = static_cast<std::uint8_t>(Aout);
                            }
                            else
                            {
                                rgba[i + 0] = 0;
                                rgba[i + 1] = 0;
                                rgba[i + 2] = 0;
                                rgba[i + 3] = 0;
                            }
                        }
                    }

                    penX += static_cast<std::uint32_t>(g->advance.x >> 6);
                };

            for (char c : displayText)
                blitGlyph(c);

            FT_Done_Face(face);

            texture = Texture::CreateFromMemory("nametag_" + displayText, { width, height }, rgba, true, GL_RGBA8, GL_RGBA);
            textureDimensions = { static_cast<float>(width), static_cast<float>(height) };
        }

        void ApplyScaleFromTexture()
        {
            if (textureDimensions.x() <= 0.f || textureDimensions.y() <= 0.f)
                return;

            const float aspect = textureDimensions.x() / textureDimensions.y();
            const float scaleX = desiredHeightWS * aspect;
            const float scaleY = desiredHeightWS;

            auto transform = GetGameObject()->GetTransform3d();

            transform->SetLocalScale({ scaleX / 2, scaleY / 2, 1.0f });
        }

        std::shared_ptr<Mesh<UIVertex>> mesh;
        std::shared_ptr<Texture> texture;

        std::string displayText;
        AssetPath path;
        Vector<float, 3> tint = { 1.f, 1.f, 1.f };
        Vector<float, 2> textureDimensions{ 0.f, 0.f };

        float desiredHeightWS = 4.f;
        float offsetY = 22.f;
        unsigned fontPixelSize = 48;
        unsigned paddingPixels = 4;

        DESCRIBE_AND_REGISTER(BillboardedText, (Blaster::Independent::ECS::Component), (), (), (displayText, path, tint, desiredHeightWS, offsetY, fontPixelSize, paddingPixels))
    };
}

REGISTER_COMPONENT(Blaster::Client::Render::BillboardedText, 88421)

#endif
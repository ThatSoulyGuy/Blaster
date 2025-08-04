#pragma once

#include <functional>
#include <string>
#include <array>
#include <utility>
#include <ft2build.h>
#include <freetype/ftbitmap.h>
#include FT_FREETYPE_H
#include <boost/serialization/string.hpp>
#include "Client/Core/InputManager.hpp"
#include "Client/Render/Shader.hpp"
#include "Client/Render/Texture.hpp"
#include "Client/UI/UIElement.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"

using namespace Blaster::Client::Render;

namespace Blaster::Client::UI::Elements
{
    class UIElementText : public UIElement
    {

    public:

        struct GlyphInfo
        {
            Vector<float, 2> uvTopLeft;
            Vector<float, 2> uvBottomRight;
            Vector<int, 2> bearing;

            std::uint32_t advance;
            std::uint32_t widthPixels;
            std::uint32_t heightPixels;
        };

        struct Font
        {
            std::shared_ptr<Render::Texture> atlas;
            std::array<GlyphInfo, 95> glyphs;

            static std::shared_ptr<Font> Create(const AssetPath& path, std::uint32_t pixelHeight, std::uint32_t cellPixels, std::uint32_t spreadPixels)
            {
                constexpr std::uint32_t firstChar{ 32 };
                constexpr std::uint32_t lastChar{ 126 };
                constexpr std::uint32_t charCount{ lastChar - firstChar + 1 };

                static FT_Library ftLibrary{ nullptr };

                if (ftLibrary == nullptr)
                {
                    if (const FT_Error error{ FT_Init_FreeType(&ftLibrary) }; error != 0)
                        throw std::runtime_error("FreeType initialisation failed.");
                }

                FT_Face face{};

                if (const FT_Error error{ FT_New_Face(ftLibrary, path.GetFullPath().c_str(), 0, &face) }; error != 0)
                    throw std::runtime_error("Failed to load font face.");

                if (const FT_Error error{ FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelHeight)) }; error != 0)
                    throw std::runtime_error("FT_Set_Pixel_Sizes failed.");

                std::uint32_t maxWidth{ 0 }, maxHeight{ 0 };
                for (std::uint32_t i{ 0 }; i < charCount; ++i) {
                    const FT_ULong codepoint{ firstChar + i };
                    FT_Load_Char(face, codepoint, FT_LOAD_RENDER);
                    maxWidth = std::max<std::uint32_t>(maxWidth, face->glyph->bitmap.width);
                    maxHeight = std::max<std::uint32_t>(maxHeight, face->glyph->bitmap.rows);
                }

                if (cellPixels == 0 || maxWidth + 2 * spreadPixels > cellPixels || maxHeight + 2 * spreadPixels > cellPixels)
                    cellPixels = std::max(maxWidth, maxHeight) + 2 * spreadPixels;

                const std::uint32_t cellsPerRow{ static_cast<std::uint32_t>(std::ceil(std::sqrt(charCount))) };
                const std::uint32_t atlasSidePixels{ cellsPerRow * cellPixels };
                const std::uint32_t atlasSize{ atlasSidePixels * atlasSidePixels };

                std::vector<std::uint8_t> atlasPixels(atlasSize, 0);

                std::shared_ptr<Font> font{ new Font() };

                for (std::uint32_t i{ 0 }; i < charCount; ++i)
                {
                    const char32_t charCode{ static_cast<char32_t>(firstChar + i) };

                    if (const FT_Error error{ FT_Load_Char(face, static_cast<FT_ULong>(charCode), FT_LOAD_RENDER) }; error != 0)
                        throw std::runtime_error("FT_Load_Char failed.");

                    const FT_GlyphSlot glyph{ face->glyph };
                    const FT_Bitmap& bitmap{ glyph->bitmap };

                    const std::uint32_t col{ i % cellsPerRow };
                    const std::uint32_t row{ i / cellsPerRow };

                    const std::uint32_t xPos{ col * cellPixels + spreadPixels };
                    const std::uint32_t yPos{ row * cellPixels + spreadPixels };

                    for (std::uint32_t y{ 0 }; y < bitmap.rows; ++y)
                    {
                        const std::uint8_t* bitmapRow{ bitmap.buffer + y * bitmap.pitch };
                        const std::uint32_t atlasRow{ (yPos + y) * atlasSidePixels + xPos };

                        if (bitmap.width + 2 * spreadPixels > cellPixels || bitmap.rows + 2 * spreadPixels > cellPixels)
                            throw std::runtime_error("cellPixels too small for chosen font size.");
                        
                        std::copy_n(bitmapRow, bitmap.width, atlasPixels.begin() + atlasRow);
                    }

                    auto& glyphInfo{ font->glyphs[i] };

                    glyphInfo.uvTopLeft = { static_cast<float>(xPos) / atlasSidePixels, static_cast<float>(yPos) / atlasSidePixels };
                    glyphInfo.uvBottomRight = { static_cast<float>(xPos + bitmap.width) / atlasSidePixels, static_cast<float>(yPos + bitmap.rows) / atlasSidePixels };
                    glyphInfo.bearing = { glyph->bitmap_left, glyph->bitmap_top };
                    glyphInfo.advance = static_cast<std::uint32_t>(glyph->advance.x) >> 6;
                    glyphInfo.widthPixels = bitmap.width;
                    glyphInfo.heightPixels = bitmap.rows;
                }

                FT_Done_Face(face);

                font->atlas = Render::Texture::CreateFromMemory(std::format("{}_atlas", path.GetFullPath()), { atlasSidePixels, atlasSidePixels }, atlasPixels, true, GL_R8, GL_RED);

                return font;
            }

        private:

            friend class boost::serialization::access;

            template <typename Archive>
            void serialize(Archive& archive, const unsigned) { }
        };

        UIElementText(const UIElementText&) = delete;
        UIElementText(UIElementText&&) = delete;
        UIElementText& operator=(const UIElementText&) = delete;
        UIElementText& operator=(UIElementText&&) = delete;

        void Initialize() override
        {
            UIElement::Initialize();
        }
        
        [[nodiscard]]
        std::shared_ptr<Font> GetFont() const noexcept
        {
            return font;
        }

        void SetFont(const std::shared_ptr<Font>& font)
        {
            this->font = font;
        }
        
        [[nodiscard]]
        std::string GetText() const noexcept
        {
            return text;
        }

        void SetText(const std::string& text)
        {
            this->text = text;
        }

        [[nodiscard]]
        Vector<float, 3> GetTint() const noexcept
        {
            return tint;
        }

        void SetTint(const Vector<float, 3>& tint)
        {
            this->tint = tint;
        }

        void RenderUI() override
        {
            if (font && font->atlas)
            {
                auto mesh = GetMesh();

                mesh->QueueShaderCall("uFontAtlas", 0);
                mesh->QueueRenderCall([a = font->atlas] { a->Bind(0); });
            }
        }

        std::optional<std::shared_ptr<Shader>> GetShader() const override
        {
            return ShaderManager::GetInstance().Get("blaster.text_ui");
        }

        void Generate() override
        {
            if (font == nullptr)
                return;

            if (GetGameObject()->HasComponent<Texture>())
            {
                auto current = GetGameObject()->GetComponent<Texture>().value();

                if (current.get() != font->atlas.get())
                {
                    current->Uninitialize();
                    GetGameObject()->RemoveComponent<Texture>();
                }
            }

            if (!GetGameObject()->HasComponent<Texture>())
                GetGameObject()->AddComponent(font->atlas);

            std::vector<Render::Vertices::UIVertex> vertices;
            std::vector<std::uint32_t> indices;

            float currentPositionX{ 0.0f };
            float currentPositionY{ 0.0f };
            float maximumLineWidth{ 0.0f };

            std::uint32_t lineCounter{ 0 };
            std::uint32_t indexOffset{ 0 };

            std::uint32_t maximumGlyphHeight{ 0 };

            for (const auto& glyph : font->glyphs)
                maximumGlyphHeight = std::max(maximumGlyphHeight, glyph.heightPixels);

            const float lineHeight{ static_cast<float>(maximumGlyphHeight) };
            
            float minX{ std::numeric_limits<float>::max() }, minY{ std::numeric_limits<float>::max() };
            float maxX{ std::numeric_limits<float>::lowest() }, maxY{ std::numeric_limits<float>::lowest() };

            for (const char character : text)
            {
                if (character == '\n')
                {
                    currentPositionX = 0.0f;
                    ++lineCounter;
                    currentPositionY = -static_cast<float>(lineCounter) * lineHeight;

                    continue;
                }

                if (character < 32 || character > 126)
                    continue;

                const auto& glyphInfo{ font->glyphs[static_cast<std::size_t>(character) - 32] };

                const float x0{ currentPositionX + static_cast<float>(glyphInfo.bearing.x()) };
                const float y0{ currentPositionY + static_cast<float>(glyphInfo.bearing.y()) };

                const float glyphWidth{ static_cast<float>(glyphInfo.widthPixels) };
                const float glyphHeight{ static_cast<float>(glyphInfo.heightPixels) };

                const auto textureCoordinateLeft = glyphInfo.uvTopLeft.x();
                const auto textureCoordinateTop = glyphInfo.uvTopLeft.y();
                const auto textureCoordinateRight = glyphInfo.uvBottomRight.x();
                const auto textureCoordinateBottom = glyphInfo.uvBottomRight.y();

                Render::Vertices::UIVertex topLeftVertex;
                topLeftVertex.position = { x0, y0, 0.0f };
                topLeftVertex.color = tint;
                topLeftVertex.uvs = { textureCoordinateLeft, textureCoordinateTop };

                Render::Vertices::UIVertex bottomLeftVertex;
                bottomLeftVertex.position = { x0, y0 - glyphHeight,0.0f };
                bottomLeftVertex.color = tint;
                bottomLeftVertex.uvs = { textureCoordinateLeft, textureCoordinateBottom };

                Render::Vertices::UIVertex bottomRightVertex;
                bottomRightVertex.position = { x0 + glyphWidth, y0 - glyphHeight,0.0f };
                bottomRightVertex.color = tint;
                bottomRightVertex.uvs = { textureCoordinateRight, textureCoordinateBottom };

                Render::Vertices::UIVertex topRightVertex;
                topRightVertex.position = { x0 + glyphWidth, y0, 0.0f };
                topRightVertex.color = tint;
                topRightVertex.uvs = { textureCoordinateRight, textureCoordinateTop };

                vertices.push_back(topLeftVertex);
                vertices.push_back(bottomLeftVertex);
                vertices.push_back(bottomRightVertex);
                vertices.push_back(topRightVertex);

                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 1);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indexOffset += 4;

                currentPositionX += static_cast<float>(glyphInfo.advance);
                maximumLineWidth = std::max(maximumLineWidth, currentPositionX);

                minX = std::min({ minX, x0 });
                maxX = std::max({ maxX, x0 + glyphWidth });
                minY = std::min({ minY, y0 - glyphHeight });
                maxY = std::max({ maxY, y0 });
            }

            const float boxWidth = maxX - minX;
            const float boxHeight = maxY - minY;
            const float invW = 1.0f / boxWidth;
            const float invH = 1.0f / boxHeight;

            for (auto& vertex : vertices)
            {
                vertex.position.x() = (vertex.position.x() - minX) * invW;
                vertex.position.y() = (maxY - vertex.position.y()) * invH;
            }

            auto mesh = GetMesh();

            mesh->SetVertices(std::move(vertices));
            mesh->SetIndices(std::move(indices));
            mesh->Generate();

            GetGameObject()->GetTransform2d()->SetDimensions({ boxWidth, boxHeight });
        }

        static std::shared_ptr<UIElementText> Create()
        {
            return std::shared_ptr<UIElementText>(new UIElementText());
        }

    private:

        UIElementText() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
            
            archive & BOOST_SERIALIZATION_NVP(text);
            archive & BOOST_SERIALIZATION_NVP(tint);
            archive & BOOST_SERIALIZATION_NVP(font);
        }

        std::shared_ptr<Font> font;
        std::string text = "!";

        Vector<float, 3> tint = { 1.0f, 1.0f, 1.0f };

        bool regenerateMesh = false;

        DESCRIBE_AND_REGISTER(UIElementText, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Elements::UIElementText, 99712)
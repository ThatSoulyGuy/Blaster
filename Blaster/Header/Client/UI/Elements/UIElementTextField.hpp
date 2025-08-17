#pragma once

#include <functional>
#include <string>
#include <limits>
#include <boost/serialization/string.hpp>
#include "Client/Core/InputManager.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/Texture.hpp"
#include "Client/UI/UIElement.hpp"
#include "Client/UI/Elements/UIElementText.hpp"
#include "Independent/Utility/Time.hpp"

using namespace Blaster::Client::Core;
using namespace Blaster::Client::Render;

namespace Blaster::Client::UI::Elements
{
    class UIElementTextField : public UIElement
    {

    public:

        using Font = UIElementText::Font;

        ~UIElementTextField() override
        {
            if (sFocused == this)
                sFocused = nullptr;
        }

        UIElementTextField(const UIElementTextField&) = delete;
        UIElementTextField(UIElementTextField&&) = delete;
        UIElementTextField& operator=(const UIElementTextField&) = delete;
        UIElementTextField& operator=(UIElementTextField&&) = delete;

        void SetFont(const std::shared_ptr<Font>& font)
        {
            this->font = font;
            needsGenerate = true;
        }

        std::shared_ptr<Font> GetFont() const noexcept
        {
            return font;
        }

        void SetText(const std::string& text)
        {
            this->text = text;
            caret = std::min<std::size_t>(caret, text.size());
            
            needsGenerate = true;
        }

        const std::string& GetText() const noexcept
        {
            return text;
        }

        void SetPlaceholder(const std::string& placeholder)
        {
            this->placeholder = placeholder;
            
            needsGenerate = true;
        }

        const std::string& GetPlaceholder() const noexcept
        {
            return placeholder;
        }

        void SetTextColor(const Vector<float, 3>& textColor)
        {
            this->textColor = textColor;

            needsGenerate = true;
        }

        void SetPlaceholderColor(const Vector<float, 3>& placeholderColor)
        {
            this->placeholderColor = placeholderColor;
            
            needsGenerate = true;
        }

        void SetMaxLength(std::size_t maxLength)
        {
            this->maxLength = maxLength;
        }

        std::size_t GetMaxLength() const noexcept
        {
            return maxLength;
        }

        void SetFocused(bool focused)
        {
            if (focused)
            {
                if (sFocused && sFocused != this)
                {
                    sFocused->isFocused = false;
                    sFocused->caretVisible = false;
                    sFocused->needsGenerate = true;
                }

                sFocused = this;

                isFocused = true;
                caretBlinkTimer = 0.0f;
                caretVisible = true;
                needsGenerate = true;
            }
            else
            {
                if (sFocused == this)
                    sFocused = nullptr;

                isFocused = false;
                caretVisible = false;
                needsGenerate = true;
            }
        }

        bool IsFocused() const noexcept
        {
            return isFocused;
        }

        void SetSubmitOnEnter(bool submitOnEnter)
        {
            this->submitOnEnter = submitOnEnter;
        }

        void SetOnSubmit(std::function<void(const std::string&)> callback)
        {
            onSubmit = std::move(callback);
        }

        void SetCaret(std::size_t i)
        {
            caret = std::min<std::size_t>(i, text.size());
            needsGenerate = true;
        }

        std::size_t GetCaret() const noexcept
        {
            return caret;
        }

        void Initialize() override
        {
            UIElement::Initialize();
        }

        void Update() override
        {
            if (InputManager::GetInstance().GetMouseState(MouseCode::LEFT, MouseState::PRESSED))
            {
                const bool inside = IsMouseInside();

                if (inside && sFocused != this)
                {
                    SetFocused(true);

                    caret = text.size();
                }
                else if (!inside && sFocused == this)
                    SetFocused(false);
            }

            bool changed = false;

            if (isFocused)
            {
                std::string typed = InputManager::GetInstance().ConsumeTextInput();

                if (!typed.empty())
                {
                    for (char c : typed)
                    {
                        if (c < 32 || c > 126)
                            continue;

                        if (text.size() >= maxLength)
                            break;

                        text.insert(text.begin() + static_cast<std::ptrdiff_t>(caret), c);
                        ++caret;

                        changed = true;
                    }
                }

                auto& inputManager = InputManager::GetInstance();

                if (inputManager.GetKeyState(KeyCode::BACKSPACE, KeyState::PRESSED))
                {
                    if (caret > 0)
                    {
                        text.erase(text.begin() + static_cast<std::ptrdiff_t>(caret - 1));
                        --caret;

                        changed = true;
                    }
                }

                if (inputManager.GetKeyState(KeyCode::DELETE_KEY, KeyState::PRESSED))
                {
                    if (caret < text.size())
                    {
                        text.erase(text.begin() + static_cast<std::ptrdiff_t>(caret));
                        changed = true;
                    }
                }

                if (inputManager.GetKeyState(KeyCode::LEFT, KeyState::PRESSED))
                {
                    if (caret > 0)
                    {
                        --caret;
                        changed = true;
                    }
                }

                if (inputManager.GetKeyState(KeyCode::RIGHT, KeyState::PRESSED))
                {
                    if (caret < text.size())
                    {
                        ++caret;
                        changed = true;
                    }
                }

                if (inputManager.GetKeyState(KeyCode::HOME, KeyState::PRESSED))
                {
                    caret = 0;
                    changed = true;
                }

                if (inputManager.GetKeyState(KeyCode::END, KeyState::PRESSED))
                {
                    caret = text.size();
                    changed = true;
                }

                if (submitOnEnter && inputManager.GetKeyState(KeyCode::ENTER, KeyState::PRESSED))
                {
                    if (onSubmit)
                        onSubmit(text);
                }

                caretBlinkTimer += Time::GetInstance().GetDeltaTime();

                if (caretBlinkTimer >= caretBlinkPeriod)
                {
                    caretBlinkTimer = 0.f;
                    caretVisible = !caretVisible;

                    changed = true;
                }
            }
            else
            {
                if (caretVisible != false)
                {
                    caretVisible = false;
                    changed = true;
                }
            }

            if (changed)
                needsGenerate = true;

            if (needsGenerate && font)
                Generate();
        }

        void Generate() override
        {
            needsGenerate = false;

            if (!font)
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

            vertices.reserve(128);
            indices.reserve(6 * 64);

            const std::string& drawText = (text.empty() && !isFocused) ? placeholder : text;
            const Vector<float, 3>& drawColor = (text.empty() && !isFocused) ? placeholderColor : textColor;

            uint32_t maxGlyphH = 0;

            for (const auto& g : font->glyphs)
                maxGlyphH = std::max(maxGlyphH, g.heightPixels);

            const float lineHeight = static_cast<float>(maxGlyphH);

            float x = 0.f;
            float y = 0.f;
            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();

            std::uint32_t idxOff = 0;

            auto emitGlyph = [&](const UIElementText::GlyphInfo& g, float gx, float gy, const Vector<float, 3>& color)
                {
                    const float w = static_cast<float>(g.widthPixels);
                    const float h = static_cast<float>(g.heightPixels);

                    const float x0 = gx + g.bearing.x();
                    const float y0 = gy + g.bearing.y();

                    Render::Vertices::UIVertex v0;

                    v0.position = { x0, y0, 0.0f };
                    v0.color = color;
                    v0.uvs = { g.uvTopLeft.x(), g.uvTopLeft.y() };

                    Render::Vertices::UIVertex v1;

                    v1.position = { x0, y0 - h, 0.0f };
                    v1.color = color;
                    v1.uvs = { g.uvTopLeft.x(), g.uvBottomRight.y() };

                    Render::Vertices::UIVertex v2;

                    v2.position = { x0 + w, y0 - h, 0.0f };
                    v2.color = color;
                    v2.uvs = { g.uvBottomRight.x(), g.uvBottomRight.y() };

                    Render::Vertices::UIVertex v3;

                    v3.position = { x0 + w, y0, 0.0f };
                    v3.color = color;
                    v3.uvs = { g.uvBottomRight.x(), g.uvTopLeft.y() };

                    vertices.push_back(v0);
                    vertices.push_back(v1);
                    vertices.push_back(v2);
                    vertices.push_back(v3);

                    indices.push_back(idxOff + 0);
                    indices.push_back(idxOff + 1);
                    indices.push_back(idxOff + 2);
                    indices.push_back(idxOff + 0);
                    indices.push_back(idxOff + 2);
                    indices.push_back(idxOff + 3);

                    idxOff += 4;

                    minX = std::min(minX, x0);
                    maxX = std::max(maxX, x0 + w);
                    minY = std::min(minY, y0 - h);
                    maxY = std::max(maxY, y0);
                };

            for (std::size_t i = 0; i < drawText.size(); ++i)
            {
                const char c = drawText[i];

                if (c < 32 || c > 126)
                    continue;

                const auto& g = font->glyphs[static_cast<std::size_t>(c) - 32];

                if (isFocused && caretVisible && (&drawText == &text) && caret == i)
                {
                    const auto& bar = font->glyphs[static_cast<std::size_t>('|') - 32];
                    emitGlyph(bar, x, y, textColor);
                }

                emitGlyph(g, x, y, drawColor);
                x += static_cast<float>(g.advance);
            }

            if (isFocused && caretVisible && (&drawText == &text) && caret == text.size())
            {
                const auto& bar = font->glyphs[static_cast<std::size_t>('|') - 32];
                emitGlyph(bar, x, y, textColor);
            }

            if (vertices.empty())
            {
                Render::Vertices::UIVertex vertex{};

                vertex.position = { 0,0,0 };
                vertex.color = drawColor;
                vertex.uvs = { 0,0 };

                vertices.push_back(vertex);
                vertices.push_back(vertex);
                vertices.push_back(vertex);
                vertices.push_back(vertex);

                indices = { 0, 1, 2, 0, 2, 3 };

                minX = minY = 0.f;
                maxX = maxY = 1.f;
            }

            const float boxWidth = std::max(1.f, maxX - minX);
            const float boxHeight = std::max(1.f, maxY - minY);
            const float invWidth = 1.f / boxWidth;
            const float invHeight = 1.f / boxHeight;

            for (auto& vertex : vertices)
            {
                vertex.position.x() = (vertex.position.x() - minX) * invWidth;
                vertex.position.y() = (maxY - vertex.position.y()) * invHeight;
            }

            auto mesh = GetMesh();

            mesh->SetVertices(std::move(vertices));
            mesh->SetIndices(std::move(indices));
            mesh->Generate();

            GetGameObject()->GetTransform2d()->SetDimensions({ boxWidth, boxHeight });
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

        static std::shared_ptr<UIElementTextField> Create()
        {
            return std::shared_ptr<UIElementTextField>(new UIElementTextField());
        }

    private:

        UIElementTextField() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(text);
            archive & BOOST_SERIALIZATION_NVP(placeholder);
            archive & BOOST_SERIALIZATION_NVP(textColor);
            archive & BOOST_SERIALIZATION_NVP(placeholderColor);
            archive & BOOST_SERIALIZATION_NVP(font);
            archive & BOOST_SERIALIZATION_NVP(maxLength);
            archive & BOOST_SERIALIZATION_NVP(submitOnEnter);
        }

        bool IsMouseInside() const
        {
            const auto [min, max] = GetGameObject()->GetTransform2d()->GetWorldRect();

            const Vector<float, 2> mouse = { float(InputManager::GetInstance().GetMousePosition().x()), float(InputManager::GetInstance().GetMousePosition().y()) };

            return mouse.x() >= min.x() && mouse.x() <= max.x() && mouse.y() >= min.y() && mouse.y() <= max.y();
        }

        std::shared_ptr<Font> font;
        std::string text;
        std::string placeholder;

        Vector<float, 3> textColor{ 1.f, 1.f, 1.f };
        Vector<float, 3> placeholderColor{ 0.7f, 0.7f, 0.7f };

        std::size_t maxLength = 256;

        bool isFocused = false;
        std::size_t caret = 0;

        bool submitOnEnter = true;
        std::function<void(const std::string&)> onSubmit;

        float caretBlinkTimer = 0.f;
        bool caretVisible = false;

        float paddingX = 8.0f;
        float paddingY = 6.0f;

        static constexpr float caretBlinkPeriod = 0.5f;

        bool needsGenerate = true;

        inline static UIElementTextField* sFocused = nullptr;

        DESCRIBE_AND_REGISTER(UIElementTextField, (UIElement), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::UI::Elements::UIElementTextField, 99713)

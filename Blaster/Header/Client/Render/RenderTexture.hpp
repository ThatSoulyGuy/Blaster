#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include <glad/glad.h>

#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/ECS/GameObject.hpp"
#include "Independent/ComponentRegistry.hpp"
#include "Client/Render/Texture.hpp"
#include "Independent/Math/Vector.hpp"

namespace Blaster::Client::Render
{
    class TextureFromGL final : public Texture
    {

    public:

        void Bind(const std::uint32_t unit = 0) const override
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, handle);
        }

        Vector<int, 2> GetDimensions() const override
        {
            return dimensions;
        }

        static std::shared_ptr<TextureFromGL> Create(GLuint glTex, int w, int h)
        {
            auto result = std::shared_ptr<TextureFromGL>(new TextureFromGL());

            result->handle = glTex;
            result->dimensions = { w, h };

            return result;
        }

    private:

        TextureFromGL() = default;

        GLuint handle = 0;
        Vector<int, 2> dimensions{ 1, 1 };

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            (void)archive;
        }
    };

    class RenderTexture final : public Component
    {

    public:

        ~RenderTexture() override
        {
            Destroy();
        }

        RenderTexture(const RenderTexture&) = delete;
        RenderTexture(RenderTexture&&) = delete;
        RenderTexture& operator=(const RenderTexture&) = delete;
        RenderTexture& operator=(RenderTexture&&) = delete;

        void Begin(float r = 0, float g = 0, float b = 0, float a = 1, bool doClear = true)
        {
            glGetIntegerv(GL_VIEWPORT, previousViewport);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, dimensions.x(), dimensions.y());

            if (doClear)
            {
                glDepthMask(GL_TRUE);
                glClearColor(r, g, b, a);
                glClear(GL_COLOR_BUFFER_BIT | (withDepth ? GL_DEPTH_BUFFER_BIT : 0));
            }
        }

        void End()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
        }

        Vector<int, 2> GetDimensions() const
        {
            return dimensions;
        }

        std::shared_ptr<Texture> GetTextureComponent() const
        {
            return colorTextureComponent;
        }

        void Resize(const Vector<int, 2>& dimensions)
        {
            if (this->dimensions == dimensions)
                return;

            this->dimensions.x() = std::max(1, dimensions.x());
            this->dimensions.y() = std::max(1, dimensions.y());

            Destroy();
            Allocate();
        }

        static std::shared_ptr<RenderTexture> Create(const Vector<int, 2>& dimensions, bool withDepth = true, bool linearFilter = true)
        {
            auto result = std::shared_ptr<RenderTexture>(new RenderTexture());

            result->dimensions.x() = std::max(1, dimensions.x());
            result->dimensions.y() = std::max(1, dimensions.y());
            result->withDepth = withDepth;
            result->linearFilter = linearFilter;

            result->Allocate();

            return result;
        }

    private:

        RenderTexture() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);
        }

        void Allocate()
        {
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            glGenTextures(1, &color);
            glBindTexture(GL_TEXTURE_2D, color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, dimensions.x(), dimensions.y(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearFilter ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearFilter ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);

            if (withDepth)
            {
                glGenRenderbuffers(1, &depth);
                glBindRenderbuffer(GL_RENDERBUFFER, depth);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, dimensions.x(), dimensions.y());
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
            }

            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                Destroy();

                throw std::runtime_error("RenderTexture2D: framebuffer incomplete.");
            }

            colorTextureComponent = TextureFromGL::Create(color, dimensions.x(), dimensions.y());
        }

        void Destroy()
        {
            if (depth)
            {
                glDeleteRenderbuffers(1, &depth);
                depth = 0;
            }

            if (color)
            {
                glDeleteTextures(1, &color);
                color = 0;
            }

            if (fbo)
            {
                glDeleteFramebuffers(1, &fbo);
                fbo = 0;
            }

            colorTextureComponent.reset();
        }

        GLuint fbo = 0;
        GLuint color = 0;
        GLuint depth = 0;

        bool withDepth = true;
        bool linearFilter = true;

        Vector<int, 2> dimensions;

        GLint previousViewport[4]{ 0, 0, 0, 0 };

        std::shared_ptr<Texture> colorTextureComponent;

        BOOST_DESCRIBE_CLASS(RenderTexture, (Component), (), (), ())
    };
}

REGISTER_COMPONENT(Blaster::Client::Render::RenderTexture, 99011)
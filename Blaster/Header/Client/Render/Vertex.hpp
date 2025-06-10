#pragma once

#include <vector>
#include <cstddef>
#include <glad/glad.h>

namespace Blaster::Client::Render
{
    struct VertexBufferElement final
    {
        GLuint index;
        GLint componentCount;
        GLenum type;
        GLboolean normalized;
        std::size_t offset;
    };

    class VertexBufferLayout final
    {

    public:

        VertexBufferLayout() = default;

        void Add(const GLuint index, const GLint count, const GLenum type, const std::size_t offset, const GLboolean normalized = GL_FALSE)
        {
            elements.push_back({ index, count, type, normalized, offset });
        }

        void AddAutomatic(const GLuint index, const GLint componentCount, const GLenum type, const GLboolean normalized = GL_FALSE)
        {
            elements.push_back({ index, componentCount, type, normalized, stride });

            stride += componentCount * SizeOfType(type);
        }

        [[nodiscard]]
        const auto& GetElements() const
        {
            return elements;
        }

        [[nodiscard]]
        GLuint GetStride() const
        {
            return static_cast<GLuint>(stride);
        }

    private:

        static std::size_t SizeOfType(const GLenum type)
        {
            switch (type)
            {
                case GL_FLOAT:
                    return sizeof(GLfloat);

                case GL_UNSIGNED_INT:
                    return sizeof(GLuint);

                case GL_UNSIGNED_BYTE:
                    return sizeof(GLubyte);

                default:
                    return 0;
            }
        }

        std::vector<VertexBufferElement> elements;
        std::size_t stride = 0;
    };
}
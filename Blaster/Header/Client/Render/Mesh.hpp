#pragma once

#include <vector>
#include <memory>
#include <concepts>
#include <glad/glad.h>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/array.hpp>
#include "Client/Core/Window.hpp"
#include "Client/Render/Camera.hpp"
#include "Client/Render/Shader.hpp"
#include "Client/Render/Vertex.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"

namespace Blaster::Client::Render
{
    using UniformValue = std::variant<int, float, Vector<float, 2>, Vector<float, 3>, Matrix<float, 4, 4>, std::vector<Matrix<float, 4, 4>>>;

    struct ShaderCall final
    {
        std::string name;
        UniformValue value;

        [[nodiscard]]
        bool operator==(const ShaderCall& other) const
        {
            return OPERATOR_CHECK(name, value);
        }

        [[nodiscard]]
        bool operator!=(const ShaderCall& other) const
        {
            return !(*this == other);
        }
    };

    template <typename T>
    concept VertexType = requires(T a)
    {
        { a.GetLayout() } -> std::same_as<VertexBufferLayout>;
    };

    template <VertexType T>
    class Mesh final : public Component, public std::enable_shared_from_this<Mesh<T>>
    {

    public:

        ~Mesh() override
        {
            if (VAO)
                glDeleteVertexArrays(1, &VAO);

            for (auto& buffer : bufferMap | std::views::values)
                glDeleteBuffers(1, &buffer);
        }

        Mesh(const Mesh&) = delete;
        Mesh(Mesh&&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh& operator=(Mesh&&) = delete;

        void Initialize() override
        {
            if (shouldRegenerate && !GetGameObject()->IsAuthoritative() && GetGameObject()->GetOwningClient().has_value() && GetGameObject()->GetOwningClient().value() != ClientNetwork::GetInstance().GetNetworkId() && !vertices.empty() && !indices.empty())
            {
                for (auto& buffer : bufferMap | std::views::values)
                    buffer = 0;

                Generate();
            }
        }

        void Generate()
        {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &bufferMap["vbo"]);
            glGenBuffers(1, &bufferMap["ebo"]);

            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, bufferMap["vbo"]);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferMap["ebo"]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

            for (const auto layout = T::GetLayout(); auto const& element : layout.GetElements())
            {
                const GLuint stride = sizeof(T);

                glEnableVertexAttribArray(element.index);

                if (element.type == GL_INT || element.type == GL_UNSIGNED_INT)
                    glVertexAttribIPointer(element.index, element.componentCount, element.type, stride, reinterpret_cast<const void*>(element.offset));
                else
                    glVertexAttribPointer(element.index, element.componentCount, element.type, element.normalized, stride, reinterpret_cast<const void*>(element.offset));
            }

            for (auto& [name, callback] : bufferCreationCallbackMap)
                callback(bufferMap[name]);

            glBindVertexArray(0);

            areVerticesDirty = areIndicesDirty = false;
        }

        template <typename U> requires std::constructible_from<UniformValue, U>
        void QueueShaderCall(const std::string& name, U&& data)
        {
            shaderCallDeque.emplace_back(std::string{name}, UniformValue{std::forward<U>(data)});
        }

        void QueueRenderCall(const std::function<void()>& function)
        {
            renderCallDeque.emplace_back(function);
        }

        void Render(const std::shared_ptr<Camera>& camera) override
        {
            if (const auto shader = GetGameObject()->template GetComponent<Shader>())
            {
                CommitIfDirty();

                shader.value()->Bind();

                if (camera->GetGameObject()->HasComponent<Transform3d>())
                {
                    shader.value()->SetUniform("projectionUniform", camera->GetProjectionMatrix());
                    shader.value()->SetUniform("viewUniform", camera->GetViewMatrix());
                    shader.value()->SetUniform("modelUniform", GetGameObject()->GetTransform3d()->GetModelMatrix());
                }
                else
                {
                    shader.value()->SetUniform("projectionUniform", Matrix<float, 4, 4>::Orthographic(0.0f, Window::GetInstance().GetDimensions().x(), Window::GetInstance().GetDimensions().y(), 0.0f, 0.01f, 1000.0f));
                    shader.value()->SetUniform("modelUniform", GetGameObject()->GetTransform2d()->GetModelMatrix());
                }

                for (const auto& [name, value] : shaderCallDeque)
                    std::visit([&](auto&& uniform) { shader.value()->SetUniform(name, uniform); }, value);

                shaderCallDeque.clear();

                for (const auto& function : renderCallDeque)
                    function();

                renderCallDeque.clear();

                glBindVertexArray(VAO);
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
                glBindVertexArray(0);
            }
        }

        void AddBuffer(const std::string& name, const std::function<void(GLuint&)>& callback)
        {
            bufferMap.insert({ name, 0 });

            bufferCreationCallbackMap.insert({ name, callback });
        }

        GLuint& GetBuffer(const std::string& name)
        {
            return bufferMap.at(name);
        }

        void SetVertices(std::vector<T> v)
        {
            MarkVertexChanges(vertices, std::move(v));
        }

        void SetIndices(const std::vector<uint32_t>& i)
        {
            MarkIndexChanges(indices, std::move(i));
        }

        [[nodiscard]]
        bool operator==(const Mesh& other) const
        {
            return OPERATOR_CHECK(vertices, indices, bufferMap);
        }

        [[nodiscard]]
        bool operator!=(const Mesh& other) const
        {
            return !(*this == other);
        }

        static std::shared_ptr<Mesh> Create(const std::vector<T>& vertices, const std::vector<uint32_t>& indices, const bool shouldRegenerate = true)
        {
            std::shared_ptr<Mesh> result(new Mesh());

            result->vertices = vertices;
            result->indices = indices;
            result->shouldRegenerate = shouldRegenerate;

            return result;
        }

    private:

        Mesh() = default;

        friend class Blaster::Independent::ECS::ComponentFactory;
        friend class boost::serialization::access;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            if (!shouldRegenerate)
                return;

            archive & boost::serialization::make_nvp("vertices", vertices);
            archive & boost::serialization::make_nvp("indices", indices);

            archive & boost::serialization::make_nvp("bufferMap", bufferMap);
        }

        void CommitIfDirty()
        {
            if (!areVerticesDirty && !areIndicesDirty)
                return;

            glBindVertexArray(VAO);

            if (areVerticesResized)
            {
                glBindBuffer(GL_ARRAY_BUFFER, bufferMap["vbo"]);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T), vertices.data(), GL_STATIC_DRAW);
            }
            else if (areVerticesDirty)
            {
                glBindBuffer(GL_ARRAY_BUFFER, bufferMap["vbo"]);

                const GLsizeiptr offset = firstVerticeDirty * sizeof(T);
                const GLsizeiptr bytes = (lastVerticeDirty - firstVerticeDirty + 1) * sizeof(T);

                glBufferSubData(GL_ARRAY_BUFFER, offset, bytes, &vertices[firstVerticeDirty]);
            }

            if (areIndicesResized)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferMap["ebo"]);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),indices.data(), GL_STATIC_DRAW);
            }
            else if (areIndicesDirty)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferMap["ebo"]);

                const GLsizeiptr offset = firstIndiceDirty * sizeof(uint32_t);
                const GLsizeiptr bytes = (lastIndiceDirty - firstIndiceDirty + 1) * sizeof(uint32_t);

                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, bytes, &indices[firstIndiceDirty]);
            }

            glBindVertexArray(0);

            areVerticesDirty = areIndicesDirty = false;
            areVerticesResized = areIndicesResized = false;
        }

        template <typename Vector, typename Source>
        void MarkRange(Vector& buffer, Source&& source, bool& dirty, bool& resized, size_t& first, size_t& last)
        {
            if (buffer.size() != source.size())
            {
                buffer = std::forward<Source>(source);
                dirty = resized = true;

                return;
            }

            size_t calculatedFirst = buffer.size(), calculatedLast = 0;

            for (size_t i = 0; i < buffer.size(); ++i)
            {
                if (buffer[i] != source[i])
                {
                    calculatedFirst = std::min(calculatedFirst, i);
                    calculatedLast = i;
                }
            }

            if (calculatedFirst == buffer.size())
                return;

            buffer.swap(source);

            dirty = true;

            first = std::min(first, calculatedFirst);
            last = std::max(last, calculatedLast);
        }

        void MarkVertexChanges(std::vector<T>&& newV)
        {
            MarkRange(vertices, std::move(newV), areVerticesDirty, areVerticesResized, firstVerticeDirty, lastVerticeDirty);
        }

        void MarkIndexChanges(std::vector<uint32_t>&& newI)
        {
            MarkRange(indices, std::move(newI), areIndicesDirty, areIndicesResized, firstIndiceDirty, lastIndiceDirty);
        }

        std::unordered_map<std::string, std::function<void(GLuint&)>> bufferCreationCallbackMap;

        std::deque<ShaderCall> shaderCallDeque;
        std::deque<std::function<void()>> renderCallDeque;
        std::vector<T> vertices;
        std::vector<uint32_t> indices;

        GLuint VAO = 0;

        std::unordered_map<std::string, GLuint> bufferMap =
        {
            { "vbo", 0 },
            { "ebo", 0 }
        };

        bool shouldRegenerate = true;

        bool areVerticesDirty = false, areIndicesDirty = false;
        bool areVerticesResized = false, areIndicesResized = false;

        size_t firstVerticeDirty = std::numeric_limits<size_t>::max(), lastVerticeDirty = 0;
        size_t firstIndiceDirty = std::numeric_limits<size_t>::max(), lastIndiceDirty = 0;

        DESCRIBE_AND_REGISTER(Mesh<T>, (Component), (), (), (shouldRegenerate, areVerticesDirty, areIndicesDirty, areVerticesResized, areIndicesResized, firstVerticeDirty, lastVerticeDirty, firstIndiceDirty, lastIndiceDirty))
    };
}
#pragma once

#include <vector>
#include <memory>
#include <concepts>
#include <glad/glad.h>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/array.hpp>
#include "Client/Render/Camera.hpp"
#include "Client/Render/Shader.hpp"
#include "Client/Render/Vertex.hpp"
#include "Independent/ECS/Component.hpp"
#include "Independent/ECS/ComponentFactory.hpp"

namespace Blaster::Client::Render
{
    template <typename T>
    concept VertexType = std::derived_from<T, VertexExtension>;

    template <VertexType T>
    class Mesh final : public Component, public std::enable_shared_from_this<Mesh<T>>
    {

    public:

        ~Mesh() override
        {
            if (VAO)
                glDeleteVertexArrays(1,&VAO);

            if (VBO)
                glDeleteBuffers(1,&VBO);

            if (EBO)
                glDeleteBuffers(1,&EBO);
        }

        Mesh(const Mesh&) = delete;
        Mesh(Mesh&&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh& operator=(Mesh&&) = delete;

        void Initialize() override
        {
            if (!GetGameObject()->IsAuthoritative() && GetGameObject()->GetOwningClient().has_value() && GetGameObject()->GetOwningClient().value() != ClientNetwork::GetInstance().GetNetworkId() && !vertices.empty() && !indices.empty())
            {
                VAO = VBO = EBO = 0;

                Generate();
            }
        }

        void Generate()
        {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER,VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(uint32_t),indices.data(),GL_STATIC_DRAW);

            const auto layout  = vertices[0].GetLayout();
            const GLuint stride = layout.GetStride();

            for (auto const& [index, count, type, isNormalized, offset] : layout.GetElements())
            {
                glEnableVertexAttribArray(index);
                glVertexAttribPointer(index, count, type, isNormalized, stride, reinterpret_cast<const void*>(offset));
            }

            glBindVertexArray(0);

            areVerticesDirty = areIndicesDirty = false;
        }

        void Render(const std::shared_ptr<Camera>& camera) override
        {
            if (const auto shader = GetGameObject()->template GetComponent<Shader>())
            {
                CommitIfDirty();

                shader.value()->Bind();
                shader.value()->SetUniform("projectionUniform",camera->GetProjectionMatrix());
                shader.value()->SetUniform("viewUniform", camera->GetViewMatrix());
                shader.value()->SetUniform("modelUniform", GetGameObject()->GetTransform()->GetModelMatrix());

                glBindVertexArray(VAO);
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT,nullptr);
                glBindVertexArray(0);
            }
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
        std::string GetTypeName() const override
        {
            return typeid(Mesh).name();
        }

        static std::shared_ptr<Mesh> Create(const std::vector<T>& vertices, const std::vector<uint32_t>& indices)
        {
            std::shared_ptr<Mesh> result(new Mesh());

            result->vertices = vertices;
            result->indices = indices;

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

            archive & boost::serialization::make_nvp("vertices", vertices);
            archive & boost::serialization::make_nvp("indices", indices);

            archive & boost::serialization::make_nvp("vao", VAO);
            archive & boost::serialization::make_nvp("vbo", VBO);
            archive & boost::serialization::make_nvp("ebo", EBO);
        }

        void CommitIfDirty()
        {
            if (!areVerticesDirty && !areIndicesDirty)
                return;

            glBindVertexArray(VAO);

            if (areVerticesResized)
            {
                glBindBuffer(GL_ARRAY_BUFFER,VBO);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T), vertices.data(), GL_STATIC_DRAW);
            }
            else if (areVerticesDirty)
            {
                glBindBuffer(GL_ARRAY_BUFFER,VBO);

                const GLsizeiptr offset = firstVerticeDirty * sizeof(T);
                const GLsizeiptr bytes = (lastVerticeDirty - firstVerticeDirty + 1) * sizeof(T);

                glBufferSubData(GL_ARRAY_BUFFER, offset, bytes, &vertices[firstVerticeDirty]);
            }

            if (areIndicesResized)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),indices.data(), GL_STATIC_DRAW);
            }
            else if (areIndicesDirty)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

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
            markRange(vertices, std::move(newV), areVerticesDirty, areVerticesResized, firstVerticeDirty, lastVerticeDirty);
        }
        void MarkIndexChanges(std::vector<uint32_t>&& newI)
        {
            MarkRange(indices, std::move(newI), areIndicesDirty, areIndicesResized, firstIndiceDirty, lastIndiceDirty);
        }

        std::vector<T> vertices;
        std::vector<uint32_t> indices;

        GLuint VAO = 0,VBO = 0,EBO = 0;

        bool areVerticesDirty = false, areIndicesDirty = false;
        bool areVerticesResized = false, areIndicesResized = false;

        size_t firstVerticeDirty = std::numeric_limits<size_t>::max(), lastVerticeDirty = 0;
        size_t firstIndiceDirty = std::numeric_limits<size_t>::max(), lastIndiceDirty = 0;
    };
}
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
                glDeleteVertexArrays(1, &VAO);

            if (VBO)
                glDeleteBuffers(1, &VBO);

            if (EBO)
                glDeleteBuffers(1, &EBO);
        }

        Mesh(const Mesh&) = delete;
        Mesh(Mesh&&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh& operator=(Mesh&&) = delete;

        void Generate()
        {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

            const VertexBufferLayout layout = vertices[0].GetLayout();
            const GLuint stride = layout.GetStride();

            for (const auto& [index, componentCount, type, normalized, offset] : layout.GetElements())
            {
                glEnableVertexAttribArray(index);
                glVertexAttribPointer(index, componentCount, type, normalized, static_cast<GLsizei>(stride), reinterpret_cast<const void*>(offset));
            }

            glBindVertexArray(0);
        }

        void Render(const std::shared_ptr<Camera>& camera) override
        {
            if (!GetGameObject()->template GetComponent<Shader>().has_value())
                return;

            GetGameObject()->template GetComponent<Shader>().value()->Bind();
            GetGameObject()->template GetComponent<Shader>().value()->SetUniform("projectionUniform", camera->GetProjectionMatrix());
            GetGameObject()->template GetComponent<Shader>().value()->SetUniform("viewUniform", camera->GetViewMatrix());
            GetGameObject()->template GetComponent<Shader>().value()->SetUniform("modelUniform", GetGameObject()->GetTransform()->GetModelMatrix());

            glBindVertexArray(VAO);

            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);

            glBindVertexArray(0);
        }

        void SetVertices(const std::vector<T>& vertices)
        {
            this->vertices = vertices;
        }

        void SetIndices(const std::vector<uint32_t>& indices)
        {
            this->indices = indices;
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

        std::vector<T> vertices;
        std::vector<uint32_t> indices;

        GLuint VAO = 0, VBO = 0, EBO = 0;
    };
}
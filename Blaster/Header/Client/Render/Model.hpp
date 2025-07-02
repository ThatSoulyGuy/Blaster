#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Matrix4x4.h>
#include <assimp/vector3.h>
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/Vertices/ModelVertex.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Texture.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Independent::Thread;

namespace Blaster::Client::Render
{
    class Model final : public Component
    {

    public:

        Model(const Model&) = delete;
        Model(Model&&) = delete;
        Model& operator=(const Model&) = delete;
        Model& operator=(Model&&) = delete;

        void Initialize() override
        {
            if (!GetGameObject()->IsAuthoritative())
                LoadModel();
        }

        void Render(const std::shared_ptr<Camera>&) override
        {
            if (GetGameObject()->HasComponent<Texture>())
            {
                for (auto& child : GetGameObject()->GetChildMap() | std::views::values)
                {
                    child->GetComponent<Mesh<ModelVertex>>().value()->QueueShaderCall<int>("hasTexture", 1);

                    child->GetComponent<Mesh<ModelVertex>>().value()->QueueRenderCall([&]
                        {
                            GetGameObject()->GetComponent<Texture>().value()->Bind(0);
                        });

                }
            }
        }

        static std::shared_ptr<Model> Create(const AssetPath& path)
        {
            std::shared_ptr<Model> result(new Model());

            result->path = path;

            return result;
        }

    private:

        Model() = default;

        friend class boost::serialization::access;
        friend class Blaster::Independent::ECS::ComponentFactory;

        template <typename Archive>
        void serialize(Archive& archive, const unsigned)
        {
            archive & boost::serialization::base_object<Component>(*this);

            archive & BOOST_SERIALIZATION_NVP(path);
        }

        void LoadModel()
        {
            std::cout << "Loading model '" << path.GetFullPath() << "' for game object '" << GetGameObject()->GetAbsolutePath() << "'!" << std::endl;

            Assimp::Importer importer;

            constexpr unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_OptimizeMeshes;

            const aiScene* scene = importer.ReadFile(path.GetFullPath(), flags);

            if (!scene || !scene->mRootNode)
            {
                std::cerr << "Assimp failed loading " << path.GetFullPath() << "\n" << importer.GetErrorString() << '\n';

                return;
            }

            ProcessNode(scene->mRootNode, scene, GetGameObject(), aiMatrix4x4());
        }

        void ProcessNode(const aiNode* node, const aiScene* scene, const std::shared_ptr<GameObject>& parentGO, const aiMatrix4x4& parentTransform)
        {
            const aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

            const std::shared_ptr<GameObject> current = parentGO;

            for (unsigned i = 0; i < node->mNumMeshes; ++i)
                BuildMesh(scene->mMeshes[node->mMeshes[i]], current, i, globalTransform);

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                ProcessNode(node->mChildren[c], scene, current, globalTransform);
        }

        void BuildMesh(const aiMesh* aMesh, const std::shared_ptr<GameObject>& owner, const unsigned localIdx, const aiMatrix4x4& xform) const
        {
            std::vector<ModelVertex> vertices;
            vertices.reserve(aMesh->mNumVertices);

            for (unsigned v = 0; v < aMesh->mNumVertices; ++v)
            {
                ModelVertex vertex;

                vertex.position = TransformPosition(xform, aMesh->mVertices[v]);

                if (aMesh->HasVertexColors(0))
                    vertex.color = ToVector(aMesh->mColors[0][v]);

                if (aMesh->HasTextureCoords(0))
                {
                    aiVector3D t = aMesh->mTextureCoords[0][v];
                    vertex.uv0 = {t.x, t.y};
                }
                vertices.emplace_back(std::move(vertex));
            }

            std::vector<uint32_t> indices;

            indices.reserve(static_cast<std::uint64_t>(aMesh->mNumFaces) * 3);

            for (unsigned f = 0; f < aMesh->mNumFaces; ++f)
            {
                for (unsigned j = 0; j < aMesh->mFaces[f].mNumIndices; ++j)
                    indices.push_back(aMesh->mFaces[f].mIndices[j]);
            }
            
            auto meshGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("mesh" + std::to_string(localIdx), true), GetGameObject()->GetAbsolutePath());
            
            meshGameObject->AddComponent(ShaderManager::GetInstance().Get("blaster.model").value());

            const auto component = meshGameObject->AddComponent(Mesh<ModelVertex>::Create(vertices, indices));

            MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [component]
            {
                component->Generate();
            });
        }

        static Vector<float, 3> ToVector(const aiVector3D& v)
        {
            return {v.x, v.y, v.z};
        }

        static Vector<float, 4> ToVector(const aiColor4D& v)
        {
            return {v.r, v.g, v.b, v.a};
        }

        static Vector<float, 3> TransformPosition(const aiMatrix4x4& m, const aiVector3D& v)
        {
            aiVector3D tmp = m * v;

            return {tmp.x, tmp.y, tmp.z};
        }

        static Vector<float, 3> TransformDirection(const aiMatrix4x4& m, const aiVector3D& v)
        {
            aiMatrix4x4 nMat = m;

            nMat.Inverse().Transpose();

            aiVector3D tmp = nMat * v;

            return {tmp.x, tmp.y, tmp.z};
        }

        AssetPath path;

        DESCRIBE_AND_REGISTER(Model, (Component), (), (), (path))

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Model)
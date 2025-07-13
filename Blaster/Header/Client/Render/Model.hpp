#pragma once

#include <regex>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
#include "Client/Render/Animator.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/Vertices/ModelVertex.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Skeleton.hpp"
#include "Client/Render/Texture.hpp"
#include "Independent/ECS/GameObjectManager.hpp"
#include "Independent/Physics/Colliders/ColliderMesh.hpp"
#include "Independent/Physics/Rigidbody.hpp"
#include "Independent/Thread/MainThreadExecutor.hpp"

using namespace std::chrono_literals;
using namespace Blaster::Client::Render::Vertices;
using namespace Blaster::Independent::Physics::Colliders;
using namespace Blaster::Independent::Physics;
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

            if (buildCollider)
            {
                const auto& [colliderVertices, colliderIndices] = LoadMeshBinary(std::regex_replace(path.GetFullPath(), std::regex(".fbx"), "") + "_data.bin");

                GetGameObject()->AddComponent(ColliderMesh::Create(colliderVertices, colliderIndices));
                GetGameObject()->AddComponent(Rigidbody::Create(10.0f, Rigidbody::Type::STATIC));
            }
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

                if (hasBones)
                {
                    std::array<Matrix<float, 4, 4>, 128> matrices{};
                    const std::size_t count = std::min<std::size_t>(128, skeleton.bones.size());

                    for (std::size_t i = 0; i < count;++i)
                        matrices[i] = skeleton.bones[i].finalPose * skeleton.bones[i].offset;

                    for (auto& child : GetGameObject()->GetChildMap() | std::views::values)
                    {
                        const auto mesh = child->GetComponent<Mesh<ModelVertex>>().value();

                        mesh->QueueShaderCall<int>("uUseSkinning", 1);
                        mesh->QueueShaderCall<std::vector<Matrix<float, 4, 4>>>("uBoneMatrices", { matrices.begin(), matrices.end() });
                    }
                }
            }
        }

        static std::shared_ptr<Model> Create(const AssetPath& path, bool buildCollider = false)
        {
            std::shared_ptr<Model> result(new Model());

            result->path = path;
            result->buildCollider = buildCollider;

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
            archive & BOOST_SERIALIZATION_NVP(buildCollider);
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

            if (buildCollider && !colliderVertices.empty())
            {
                const auto rootGameObject = GetGameObject();

                rootGameObject->AddComponent(ColliderMesh::Create(colliderVertices, colliderIndices));

                rootGameObject->AddComponent(Rigidbody::Create(false));
            }

            if (hasBones)
                GetGameObject()->AddComponent(Animator::Create(&skeleton));
        }

        void ProcessNode(const aiNode* node, const aiScene* scene, const std::shared_ptr<GameObject>& parentGO, const aiMatrix4x4& parentTransform)
        {
            const aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

            const std::shared_ptr<GameObject>& current = parentGO;

            for (unsigned i = 0; i < node->mNumMeshes; ++i)
                BuildMesh(scene->mMeshes[node->mMeshes[i]], current, i, globalTransform);

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                ProcessNode(node->mChildren[c], scene, current, globalTransform);
        }

        void BuildMesh(const aiMesh* aMesh, const std::shared_ptr<GameObject>& owner, const unsigned localIdx, const aiMatrix4x4& xform)
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

                vertices.emplace_back(vertex);
            }

            std::vector<uint32_t> indices;

            indices.reserve(static_cast<std::uint64_t>(aMesh->mNumFaces) * 3);

            for (unsigned f = 0; f < aMesh->mNumFaces; ++f)
            {
                for (unsigned j = 0; j < aMesh->mFaces[f].mNumIndices; ++j)
                    indices.push_back(aMesh->mFaces[f].mIndices[j]);
            }

            std::vector<Vector<uint32_t,4>> boneIds(aMesh->mNumVertices, { 0, 0, 0,0 });
            std::vector<Vector<float,4>> weights(aMesh->mNumVertices, { 0, 0, 0,0 });

            if (buildCollider)
            {
                const auto base = static_cast<std::uint32_t>(colliderVertices.size());

                for (const auto& vertex : vertices)
                    colliderVertices.push_back(vertex.position);

                for (const auto index : indices)
                    colliderIndices.push_back(base + index);
            }

            if (aMesh->HasBones())
            {
                hasBones = true;

                for (unsigned b = 0; b < aMesh->mNumBones; ++b)
                {
                    const aiBone* bone = aMesh->mBones[b];
                    const std::size_t boneIndex = skeleton.GetOrAdd(bone->mName.C_Str(), bone->mOffsetMatrix);

                    for (unsigned w = 0; w < bone->mNumWeights; ++w)
                    {
                        const aiVertexWeight& vertexWeight = bone->mWeights[w];

                        auto& idList = boneIds [vertexWeight.mVertexId];
                        auto& weightList = weights [vertexWeight.mVertexId];

                        for (int slot = 0; slot < 4; ++slot)
                        {
                            if (weightList[slot] == 0.0f)
                            {
                                idList[slot] = static_cast<uint32_t>(boneIndex);
                                weightList[slot] = vertexWeight.mWeight;

                                break;
                            }
                        }
                    }
                }
            }

            const auto meshGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("mesh" + std::to_string(localIdx), true), GetGameObject()->GetAbsolutePath());
            
            meshGameObject->AddComponent(ShaderManager::GetInstance().Get("blaster.model").value());

            const auto component = meshGameObject->AddComponent(Mesh<ModelVertex>::Create(vertices, indices));

            MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [component]
            {
                component->Generate();
            });
        }

        [[nodiscard]]
        static std::pair<std::vector<Vector<float, 3>>, std::vector<std::uint32_t>> LoadMeshBinary(const std::filesystem::path& inputPath)
        {
            std::ifstream stream(inputPath, std::ios::binary);

            if (!stream)
                throw std::runtime_error("LoadMeshBinary: could not open '" + inputPath.string() + '\'');
            
            std::uint32_t vertexCount = 0;
            std::uint32_t indexCount = 0;

            stream.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
            stream.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

            if (!stream)
                throw std::runtime_error("LoadMeshBinary: header read failed or file too short");
            
            std::vector<Vector<float, 3>> positions(vertexCount);
            std::vector<std::uint32_t> indices(indexCount);

            stream.read(reinterpret_cast<char*>(positions.data()), static_cast<std::streamsize>(vertexCount) * sizeof(float) * 3);
            stream.read(reinterpret_cast<char*>(indices.data()), static_cast<std::streamsize>(indexCount) * sizeof(std::uint32_t));

            if (!stream)
                throw std::runtime_error("LoadMeshBinary: payload read failed or file truncated");

            for (Vector<float, 3>& p : positions)
                p.y() = -p.y();

            if (indexCount % 3 == 0)
            {
                for (std::size_t i = 0; i < indexCount; i += 3)
                    std::swap(indices[i + 1], indices[i + 2]);
            }
            
            return { std::move(positions), std::move(indices) }; 
        }

        static Vector<float, 3> ToVector(const aiVector3D& v)
        {
            return { v.x, v.y, v.z };
        }

        static Vector<float, 4> ToVector(const aiColor4D& v)
        {
            return { v.r, v.g, v.b, v.a };
        }

        static Vector<float, 3> TransformPosition(const aiMatrix4x4& m, const aiVector3D& v)
        {
            aiVector3D tmp = m * v;

            return { tmp.x, tmp.y, tmp.z };
        }

        static Vector<float, 3> TransformDirection(const aiMatrix4x4& m, const aiVector3D& v)
        {
            aiMatrix4x4 nMat = m;

            nMat.Inverse().Transpose();

            aiVector3D tmp = nMat * v;

            return {tmp.x, tmp.y, tmp.z};
        }

        AssetPath path;
        bool buildCollider = false;

        std::vector<Vector<float,3>> colliderVertices;
        std::vector<std::uint32_t> colliderIndices;
        Skeleton skeleton;
        bool hasBones = false;

        DESCRIBE_AND_REGISTER(Model, (Component), (), (), (path, buildCollider, hasBones))

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Model, 28374)
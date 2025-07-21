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
#include <zstd.h>

#if defined(_MSC_VER)
#define PACK_BEGIN __pragma(pack(push, 1))
#define PACK_END   __pragma(pack(pop))
#define PACK_ATTR
#elif defined(__GNUC__) || defined(__clang__)
#define PACK_BEGIN
#define PACK_END
#define PACK_ATTR  __attribute__((packed))
#else
#pragma message("Warning: no struct‑packing directive - Header layout will be compiler-specific.")
#define PACK_BEGIN
#define PACK_END
#define PACK_ATTR
#endif

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
                const auto& [colliderVertices, colliderIndices] = LoadColliderCLD1(std::regex_replace(path.GetFullPath(), std::regex(".fbx"), "") + "_data.cld1");

                GetGameObject()->AddComponent(ColliderMesh::Create(colliderVertices, colliderIndices), false);
                GetGameObject()->AddComponent(Rigidbody::Create(10.0f, Rigidbody::Type::STATIC), false);

                GetGameObject()->GetComponent<ColliderMesh>().value()->SetShouldSynchronize(false);
                GetGameObject()->GetComponent<Rigidbody>().value()->SetShouldSynchronize(false);
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

        PACK_BEGIN
        struct Header PACK_ATTR
        {
            char magic[4]{ 'C','L','D','1' };
            uint32_t vertexCnt{ 0 };
            uint32_t indexCnt{ 0 };
            struct { float x, y, z; } bbMin{}, bbMax{};
        };
        PACK_END

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

                    vertex.uv0 = { t.x, 1.0f - t.y };
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

            std::vector<Vector<uint32_t, 4>> boneIds(aMesh->mNumVertices, { 0, 0, 0,0 });
            std::vector<Vector<float, 4>> weights(aMesh->mNumVertices, { 0, 0, 0,0 });

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

        inline std::pair<std::vector<Vector<float, 3>>, std::vector<uint32_t>> LoadColliderCLD1(const std::filesystem::path& filePath)
        {
            std::ifstream file(filePath, std::ios::binary | std::ios::ate);

            if (!file)
                throw std::runtime_error("CLD1 open fail");

            std::vector<uint8_t> buffer(file.tellg());

            file.seekg(0);
            file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

            const uint8_t* data = buffer.data();
            size_t size = buffer.size();

            if (size >= 4 && std::memcmp(data, "ZST\0", 4) == 0)
            {
                const void* frame = data + 4;

                size_t cSz = size - 4;
                size_t rSz = ZSTD_getFrameContentSize(frame, cSz);

                if (rSz == ZSTD_CONTENTSIZE_ERROR || rSz == ZSTD_CONTENTSIZE_UNKNOWN)
                    throw std::runtime_error("bad zstd frame");

                std::vector<uint8_t> raw(rSz);

                if (ZSTD_decompress(raw.data(), rSz, frame, cSz) != rSz)
                    throw std::runtime_error("zstd decompress fail");

                buffer.swap(raw);

                data = buffer.data();
                size = buffer.size();
            }

            if (size < sizeof(Header))
                throw std::runtime_error("CLD1 truncated");

            Header header{};

            std::memcpy(&header, data, sizeof header);

            if (std::memcmp(header.magic, "CLD1", 4) != 0)
                throw std::runtime_error("bad magic");

            const uint8_t* ptr = data + sizeof header;
            const uint8_t* end = data + size;

            const Vector<float, 3> lo{ header.bbMin.x, header.bbMin.y, header.bbMin.z };
            const Vector<float, 3> span{ header.bbMax.x - header.bbMin.x, header.bbMax.y - header.bbMin.y, header.bbMax.z - header.bbMin.z };

            if (ptr + header.vertexCnt * 6 > end)
                throw std::runtime_error("vertex block truncated");

            std::vector<Vector<float, 3>> vertices;
            vertices.reserve(header.vertexCnt);

            for (uint32_t i = 0; i < header.vertexCnt; ++i, ptr += 6)
            {
                uint16_t qx, qy, qz;

                std::memcpy(&qx, ptr + 0, 2);
                std::memcpy(&qy, ptr + 2, 2);
                std::memcpy(&qz, ptr + 4, 2);

                vertices.emplace_back(Vector<float, 3>{ lo.x() + span.x() * Dequant(qx), lo.y() + span.y() * Dequant(qy), lo.z() + span.z() * Dequant(qz) });
            }

            const bool idx32 = header.vertexCnt > 0xFFFF;
            const size_t idxBytes = idx32 ? 4 : 2;

            if (ptr + header.indexCnt * idxBytes > end)
                throw std::runtime_error("index block truncated");

            std::vector<uint32_t> indices;
            indices.reserve(header.indexCnt);

            if (idx32)
            {
                const uint32_t* src = reinterpret_cast<const uint32_t*>(ptr);

                indices.assign(src, src + header.indexCnt);
            }
            else
            {
                const uint16_t* src = reinterpret_cast<const uint16_t*>(ptr);

                for (uint32_t i = 0; i < header.indexCnt; ++i)
                    indices.push_back(src[i]);
            }

            ptr += header.indexCnt * idxBytes;

            if (ptr != end)
                std::cerr << "CLD1 importer: " << (end - ptr) << " unused byte(s) at file end\n";

            return { std::move(vertices), std::move(indices) };
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

        static constexpr std::uint16_t Quant(float n)
        {
            return std::uint16_t(std::clamp(n * 65535.f, 0.f, 65535.f) + 0.5f);
        }

        static constexpr float Dequant(std::uint16_t q)
        {
            return q / 65535.f;
        }

        AssetPath path;
        bool buildCollider = false;

        Skeleton skeleton;
        bool hasBones = false;

        DESCRIBE_AND_REGISTER(Model, (Component), (), (), (path, buildCollider, hasBones))

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Model, 28374)
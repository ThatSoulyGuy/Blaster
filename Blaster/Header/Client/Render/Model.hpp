#pragma once

#include <regex>
#include <set>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
#include "Client/Render/Animator.hpp"
#include "Client/Render/ShaderManager.hpp"
#include "Client/Render/Vertices/ModelVertex.hpp"
#include "Client/Render/Mesh.hpp"
#include "Client/Render/Skeleton.hpp"
#include "Client/Render/TextureManager.hpp"
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

#ifndef IS_SERVER
            if (hasBones && boneUbo == 0)
            {
                glGenBuffers(1, &boneUbo);
                glBindBuffer(GL_UNIFORM_BUFFER, boneUbo);
                glBufferData(GL_UNIFORM_BUFFER, 128 * sizeof(Matrix<float, 4, 4>), nullptr, GL_DYNAMIC_DRAW);
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }
#endif
        }

        void Render(const std::shared_ptr<Camera>&) override
        {
            if (hasBones)
            {
                const size_t boneCnt = skeleton.bones.size();
                std::vector<Matrix<float, 4, 4>> palette(boneCnt);

                for (size_t i = 0; i < boneCnt; ++i)
                    palette[i] = skeleton.bones[i].globalAnimated * skeleton.bones[i].offset;

                glBindBuffer(GL_UNIFORM_BUFFER, boneUbo);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, palette.size() * sizeof(Matrix<float, 4, 4>), palette.data());
                glBindBufferBase(GL_UNIFORM_BUFFER, 0, boneUbo);
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            for (auto& childGameObject : GetGameObject()->GetChildMap() | std::views::values)
            {
                auto meshOptional = childGameObject->GetComponent<Mesh<ModelVertex>>();

                if (!meshOptional)
                    continue;

                auto mesh = meshOptional.value();

                mesh->QueueShaderCall<int>("diffuse", 0);
                mesh->QueueShaderCall<int>("uUseSkinning", hasBones ? 1 : 0);

                mesh->QueueRenderCall([texture = childGameObject->GetComponent<Texture>().value_or(nullptr)]()
                    {
                        if (texture)
                        {
                            glActiveTexture(GL_TEXTURE0);
                            texture->Bind(0);
                        }
                    });
            }
        }

        static std::shared_ptr<Model> Create(const AssetPath& path, bool hasBones = false, bool buildCollider = false)
        {
            std::shared_ptr<Model> result(new Model());

            result->path = path;
            result->buildCollider = buildCollider;
            result->hasBones = hasBones;

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
            archive & BOOST_SERIALIZATION_NVP(hasBones);
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
            {
                FillBindPose(scene->mRootNode, scene->mRootNode->mTransformation, aiMatrix4x4());

                std::shared_ptr<Animator> animator;

                if (auto found = GetGameObject()->GetComponent<Animator>())
                    animator = found.value();
                else
                    animator = GetGameObject()->AddComponent(Animator::Create(&skeleton));

                animator->SetSkeleton(&skeleton);

                for (unsigned a = 0; a < scene->mNumAnimations; ++a)
                {
                    AnimationClip clip = ImportClip(scene->mAnimations[a], skeleton);

                    if (!animator->HasClip(clip.name))
                        animator->AddClip(std::move(clip));
                }
            }
        }

        void ProcessNode(const aiNode* node, const aiScene* scene, const std::shared_ptr<GameObject>& current, const aiMatrix4x4& parentTransform, uint32_t parentBoneId = std::numeric_limits<uint32_t>::max())
        {
            const aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

            uint32_t myBoneId = parentBoneId;

            if (IsBoneNode(node))
                myBoneId = skeleton.GetOrAdd(node->mName.C_Str(), aiMatrix4x4(), node->mTransformation, parentBoneId);
            else if (hasBones)
                myBoneId = skeleton.GetOrAdd(node->mName.C_Str(), aiMatrix4x4(), node->mTransformation, parentBoneId);

            for (unsigned m = 0; m < node->mNumMeshes; ++m)
                BuildMesh(scene->mMeshes[node->mMeshes[m]], scene, current, m, globalTransform, myBoneId);

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                ProcessNode(node->mChildren[c], scene, current, globalTransform, myBoneId);
        }

        void BuildMesh(const aiMesh* aMesh, const aiScene* scene, const std::shared_ptr<GameObject>& owner, const unsigned localIdx, const aiMatrix4x4& xform, const uint32_t nodeParentBoneId)
        {
            std::vector<ModelVertex> vertices;
            vertices.reserve(aMesh->mNumVertices);

            for (unsigned v = 0; v < aMesh->mNumVertices; ++v)
            {
                ModelVertex vertex;

                if (aMesh->HasBones())
                    vertex.position = ToVector(aMesh->mVertices[v]);
                else
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

            std::vector<Vector<uint32_t, 4>> boneIds(aMesh->mNumVertices, { 0, 0, 0, 0 });
            std::vector<Vector<float, 4>> weights(aMesh->mNumVertices, { 0, 0, 0, 0 });

            if (aMesh->HasBones())
            {
                for (unsigned b = 0; b < aMesh->mNumBones; ++b)
                {
                    const aiBone* bone = aMesh->mBones[b];

                    const uint32_t parentId = nodeParentBoneId;
                    const uint32_t boneIndex = skeleton.GetOrAdd(bone->mName.C_Str(), bone->mOffsetMatrix, aiMatrix4x4(), std::numeric_limits<uint32_t>::max());

                    for (unsigned w = 0; w < bone->mNumWeights; ++w)
                    {
                        const aiVertexWeight& vertexWeight = bone->mWeights[w];

                        auto& idList = boneIds[vertexWeight.mVertexId];
                        auto& weightList = weights[vertexWeight.mVertexId];

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

            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                vertices[i].boneIds = boneIds[i];
                vertices[i].weights = weights[i];

                const float wSum = vertices[i].weights.x() + vertices[i].weights.y() + vertices[i].weights.z() + vertices[i].weights.w();

                if (wSum > 0.0f)
                    vertices[i].weights /= wSum;
            }

            const auto meshGameObject = GameObjectManager::GetInstance().Register(GameObject::Create("mesh" + std::to_string(meshCounter++), true), GetGameObject()->GetAbsolutePath());

            meshGameObject->AddComponent(ShaderManager::GetInstance().Get("blaster.model").value());

            const auto meshComponent = meshGameObject->AddComponent(Mesh<ModelVertex>::Create(vertices, indices));

            meshComponent->AddBuffer("bones", [this](GLuint& dst) { dst = boneUbo; });

            std::string materialName;

            if (aMesh->mMaterialIndex < scene->mNumMaterials)
            {
                aiString tmp;

                if (scene->mMaterials[aMesh->mMaterialIndex]->Get(AI_MATKEY_NAME, tmp) == AI_SUCCESS)
                    materialName = tmp.C_Str();
            }

            std::shared_ptr<Texture> texture;

            if (!materialName.empty())
            {
                std::ranges::transform(materialName, materialName.begin(), [](char c) { return std::tolower(c); });
                std::regex_replace(materialName, std::regex(".001"), "");

                texture = TextureManager::GetInstance().Get(materialName).value_or(nullptr);
            }

            if (!texture)
                texture = TextureManager::GetInstance().Get("blaster.error").value();

            meshGameObject->AddComponent(texture);

            meshComponent->QueueRenderCall([textureCopy = texture]()
                {
                    glActiveTexture(GL_TEXTURE0);
                    textureCopy->Bind(0);
                });

            MainThreadExecutor::GetInstance().EnqueueTask(nullptr, [meshComponent]
                {
                    meshComponent->Generate();
                });

            if (hasBones)
            {
                auto transform = meshGameObject->GetTransform();

                Vector<float, 3> position, rotationDegrees, scale;
                DecomposeAiMatrix(xform, position, rotationDegrees, scale);

                transform->SetLocalPosition(position,  false);
                transform->SetLocalRotation(rotationDegrees, false);
                transform->SetLocalScale(scale, false);
            }
        }

        void FillBindPose(const aiNode* node, const aiMatrix4x4&, const aiMatrix4x4& parent)
        {
            aiMatrix4x4 local = node->mTransformation;
            aiMatrix4x4 global = parent * local;

            auto iterator = skeleton.boneIndex.find(node->mName.C_Str());

            if (iterator != skeleton.boneIndex.end())
            {
                BoneInformation& information = skeleton.bones[iterator->second];

                information.localBind = Matrix<float, 4, 4>::FromAssimpMatrix(local);
                information.globalAnimated = Matrix<float, 4, 4>::FromAssimpMatrix(global);
            }

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                FillBindPose(node->mChildren[c], aiMatrix4x4(), global);
        }

        std::pair<std::vector<Vector<float, 3>>, std::vector<uint32_t>> LoadColliderCLD1(const std::filesystem::path& filePath)
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

        bool IsBoneNode(const aiNode* node)
        {
            return skeleton.boneIndex.contains(node->mName.C_Str());
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

        static void DecomposeAiMatrix(const aiMatrix4x4& matrix, Vector<float, 3>& outPosition, Vector<float, 3>& outRotationDegrees, Vector<float, 3>& outScale)
        {
            aiVector3D s;
            aiQuaternion q;
            aiVector3D t;
            matrix.Decompose(s, q, t);

            outPosition = { t.x, t.y, t.z };
            outScale = { s.x, s.y, s.z };

            float ysqr = q.y * q.y;

            float sinr_cosp = 2.f * (q.w * q.x + q.y * q.z);
            float cosr_cosp = 1.f - 2.f * (q.x * q.x + ysqr);
            float roll = std::atan2(sinr_cosp, cosr_cosp);

            float sinp = 2.f * (q.w * q.y - q.z * q.x);
            float pitch;

            if (std::fabs(sinp) >= 1.f)
                pitch = std::copysign(std::numbers::pi_v<float> / 2.f, sinp);
            else
                pitch = std::asin(sinp);

            float siny_cosp = 2.f * (q.w * q.z + q.x * q.y);
            float cosy_cosp = 1.f - 2.f * (ysqr + q.z * q.z);
            float yaw = std::atan2(siny_cosp, cosy_cosp);

            constexpr float rad2deg = 180.f / std::numbers::pi_v<float>;
            outRotationDegrees = { roll * rad2deg, pitch * rad2deg, yaw * rad2deg };
        }

        static constexpr std::uint16_t Quant(float n)
        {
            return std::uint16_t(std::clamp(n * 65535.f, 0.f, 65535.f) + 0.5f);
        }

        static constexpr float Dequant(std::uint16_t q)
        {
            return q / 65535.f;
        }

        static Blaster::Client::Render::AnimationClip ImportClip(const aiAnimation* assimpAnimation, Skeleton& skeleton)
        {
            using namespace Blaster::Client::Render;

            AnimationClip clip;

            clip.name = assimpAnimation->mName.C_Str();

            clip.name = clip.name.substr(clip.name.find("|") + 1);

            clip.ticksPerSecond = (assimpAnimation->mTicksPerSecond > 0.0) ? static_cast<float>(assimpAnimation->mTicksPerSecond) : 25.0f;
            clip.durationSeconds = static_cast<float>(assimpAnimation->mDuration / clip.ticksPerSecond);

            auto toSeconds = [&](double tick)
                {
                    return static_cast<float>(tick / clip.ticksPerSecond);
                };

            for (unsigned c = 0; c < assimpAnimation->mNumChannels; ++c)
            {
                const aiNodeAnim* source = assimpAnimation->mChannels[c];
                const std::string boneName = source->mNodeName.C_Str();

                auto iterator = skeleton.boneIndex.find(boneName);

                if (iterator == skeleton.boneIndex.end())
                    continue;

                Channel dst;

                dst.boneId = static_cast<uint32_t>(iterator->second);

                std::set<float> timeList;

                for (unsigned i = 0; i < source->mNumPositionKeys; ++i)
                    timeList.insert(toSeconds(source->mPositionKeys[i].mTime));

                for (unsigned i = 0; i < source->mNumRotationKeys; ++i)
                    timeList.insert(toSeconds(source->mRotationKeys[i].mTime));

                for (unsigned i = 0; i < source->mNumScalingKeys; ++i)
                    timeList.insert(toSeconds(source->mScalingKeys[i].mTime));

                auto sampleVec3 = [&](const aiVectorKey* keys, unsigned count, float t)
                    {
                        if (count == 0)
                            return Vector<float, 3>{0, 0, 0};

                        if (t <= toSeconds(keys[0].mTime))
                            return Vector<float, 3>{keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z};

                        if (t >= toSeconds(keys[count - 1].mTime))
                            return Vector<float, 3>{keys[count - 1].mValue.x, keys[count - 1].mValue.y, keys[count - 1].mValue.z};

                        for (unsigned i = 1; i < count; ++i)
                        {
                            float t1 = toSeconds(keys[i].mTime);
                            float t0 = toSeconds(keys[i - 1].mTime);

                            if (t < t1)
                            {
                                float alpha = (t - t0) / (t1 - t0);

                                const auto& v0 = keys[i - 1].mValue;
                                const auto& v1 = keys[i].mValue;

                                return Vector<float, 3>{ v0.x + (v1.x - v0.x) * alpha, v0.y + (v1.y - v0.y) * alpha, v0.z + (v1.z - v0.z) * alpha };
                            }
                        }

                        return Vector<float, 3>{0, 0, 0};
                    };

                auto sampleQuaternion = [&](const aiQuatKey* keys, unsigned count, float t)
                    {
                        if (count == 0)
                            return Vector<float, 4>{0, 0, 0, 1};

                        if (t <= toSeconds(keys[0].mTime))
                        {
                            const auto& q = keys[0].mValue;
                            
                            return Vector<float, 4>{q.x, q.y, q.z, q.w};
                        }

                        if (t >= toSeconds(keys[count - 1].mTime))
                        {
                            const auto& q = keys[count - 1].mValue;
                            
                            return Vector<float, 4>{q.x, q.y, q.z, q.w};
                        }
                        
                        for (unsigned i = 1; i < count; ++i)
                        {
                            float t1 = toSeconds(keys[i].mTime);
                            float t0 = toSeconds(keys[i - 1].mTime);

                            if (t < t1)
                            {
                                float alpha = (t - t0) / (t1 - t0);

                                aiQuaternion out;
                                aiQuaternion::Interpolate(out, keys[i - 1].mValue, keys[i].mValue, alpha);

                                return Vector<float, 4>{out.x, out.y, out.z, out.w};
                            }
                        }

                        return Vector<float, 4>{0, 0, 0, 1};
                    };

                for (float time : timeList)
                {
                    Key key;

                    key.time = time;
                    key.translation = sampleVec3(source->mPositionKeys, source->mNumPositionKeys, time);
                    key.scale = sampleVec3(source->mScalingKeys, source->mNumScalingKeys, time);
                    key.rotation = sampleQuaternion(source->mRotationKeys, source->mNumRotationKeys, time);

                    dst.keys.push_back(key);
                }

                clip.channels.push_back(std::move(dst));
            }

            return clip;
        }

        AssetPath path;
        bool buildCollider = false;

        Skeleton skeleton;
        bool hasBones = false;

        std::size_t meshCounter = 0;
        GLuint boneUbo{ 0 };

        DESCRIBE_AND_REGISTER(Model, (Component), (), (), (path, buildCollider, hasBones))

    };
}

REGISTER_COMPONENT(Blaster::Client::Render::Model, 28374)
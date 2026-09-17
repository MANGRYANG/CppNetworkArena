#include "assimp_model_loader.h"

#include "graphics/material/material_data.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>

namespace cna::client
{
    namespace
    {
        // 머티리얼 정보를 기반으로 기본 색상 텍스처를 조회하는 함수
        bool TryGetBaseColorTextureReference(const aiMaterial& material, aiString& textureReference)
        {
            // 표준 PBR 머티리얼의 기본 색상 채널에서 텍스처 조회에 성공한 경우
            if (material.GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
                material.GetTexture(aiTextureType_BASE_COLOR, 0, &textureReference) == AI_SUCCESS &&
                textureReference.length > 0
            )
            {
                return true;
            }

            // 구형 포맷의 Diffuse 채널에서 텍스처 조회에 성공한 경우
            if (material.GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
                material.GetTexture(aiTextureType_DIFFUSE, 0, &textureReference) == AI_SUCCESS &&
                textureReference.length > 0
            )
            {
                return true;
            }

            return false;
        }

        // 임베디드 텍스처의 실제 바이트 수를 계산하는 함수
        bool TryGetEmbeddedTextureDataSize(const aiTexture& texture, std::size_t& dataSize)
        {
            if (!texture.pcData || texture.mWidth == 0)
            {
                return false;
            }

            // 텍스쳐가 압축되어 있는 경우
            if (texture.mHeight == 0)
            {
                dataSize = static_cast<std::size_t>(texture.mWidth);

                return true;
            }

            const std::size_t width = static_cast<std::size_t>(texture.mWidth);
            const std::size_t height = static_cast<std::size_t>(texture.mHeight);

            if (width > std::numeric_limits<std::size_t>::max() / height)
            {
                return false;
            }

            const std::size_t texelCount = width * height;

            if (texelCount > std::numeric_limits<std::size_t>::max() / sizeof(aiTexel))
            {
                return false;
            }

            dataSize = texelCount * sizeof(aiTexel);

            return true;
        }

        // 모델이 참조하는 텍스처 정보를 CPU 텍스처 데이터로 변환하는 함수
        bool TryCreateModelTextureData
        (
            const aiScene& scene,
            const std::filesystem::path& modelFilePath,
            const aiString& textureReference,
            ModelTextureData& modelTextureData
        )
        {
            const std::string sourceReference = textureReference.C_Str();

            if (sourceReference.empty())
            {
                return false;
            }

            modelTextureData.sourceReference = sourceReference;

            // 모델 내부에서 텍스처 참조 문자열을 기반으로 텍스처 탐색
            const aiTexture* const embeddedTexture = scene.GetEmbeddedTexture(textureReference.C_Str());

            // 모델 내부에서 텍스처를 발견한 경우
            if (embeddedTexture)
            {
                std::size_t embeddedDataSize = 0;

                if (!TryGetEmbeddedTextureDataSize(*embeddedTexture, embeddedDataSize))
                {
                    return false;
                }

                modelTextureData.sourceType = ModelTextureSourceType::Embedded;
                modelTextureData.width = embeddedTexture->mWidth;
                modelTextureData.height = embeddedTexture->mHeight;
                modelTextureData.formatHint = embeddedTexture->achFormatHint;

                const std::byte* const dataBegin = reinterpret_cast<const std::byte*>(embeddedTexture->pcData);

                modelTextureData.embeddedData.assign(dataBegin, dataBegin + embeddedDataSize);

                return true;
            }

            // 내장 텍스처를 가리키는 참조 문자열이나 해당 텍스처를 발견하지 못한 경우
            if (sourceReference.front() == '*')
            {
                return false;
            }

            // 외장 텍스처 경로 설정
            std::filesystem::path textureFilePath = sourceReference;

            // 상대 경로는 모델 파일이 위치한 디렉터리를 기준으로 해석
            if (textureFilePath.is_relative())
            {
                textureFilePath = modelFilePath.parent_path()/textureFilePath;
            }

            modelTextureData.sourceType = ModelTextureSourceType::ExternalFile;
            modelTextureData.filePath = textureFilePath.lexically_normal();

            return true;
        }

        // 이미 동일 텍스처 소스가 ModelData 구조체에 등록되어 있는지 확인하는 함수
        std::size_t FindModelTextureIndex(const ModelData& modelData, const ModelTextureData& modelTextureData)
        {
            for (std::size_t textureIndex = 0; textureIndex < modelData.textures.size(); ++textureIndex)
            {
                const ModelTextureData& existingTextureData = modelData.textures[textureIndex];

                if (existingTextureData.sourceType != modelTextureData.sourceType)
                {
                    continue;
                }

                if (modelTextureData.IsEmbedded())
                {
                    if (existingTextureData.sourceReference == modelTextureData.sourceReference)
                    {
                        return textureIndex;
                    }
                }
                else if (existingTextureData.filePath == modelTextureData.filePath)
                {
                    return textureIndex;
                }
            }

            return InvalidTextureIndex;
        }

        // 텍스처 데이터를 ModelData 구조체에 등록하는 함수
        std::size_t AddModelTextureData(ModelData& modelData, ModelTextureData modelTextureData)
        {
            const std::size_t existingTextureIndex = FindModelTextureIndex(modelData, modelTextureData);

            // 이미 텍스처 데이터가 등록되어 있는 경우 변경하지 않음
            if (existingTextureIndex != InvalidTextureIndex)
            {
                return existingTextureIndex;
            }

            modelData.textures.push_back(std::move(modelTextureData));

            return modelData.textures.size() - 1;
        }
    }

    bool AssimpModelLoadResult::Succeeded() const noexcept
    {
        return errorMessage.empty() && !modelData.IsEmpty();
    }

    AssimpModelLoadResult LoadStaticModelData(const std::filesystem::path& modelFilePath)
    {
        AssimpModelLoadResult result;

        // 파일 경로가 비어 있는 경우 로딩을 시작하지 않음
        if (modelFilePath.empty())
        {
            result.errorMessage = "Model file path is empty.";

            return result;
        }

        // 후처리 프로세스 플래그 설정
        unsigned int postProcessFlags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType |
            aiProcess_ValidateDataStructure |
            aiProcess_PreTransformVertices |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_ConvertToLeftHanded;

        Assimp::Importer importer;

        // 노드 변환을 정점에 미리 적용하여 로딩
        const aiScene* const scene = importer.ReadFile(modelFilePath.string(), postProcessFlags);

        // aiScene 로딩에 실패한 경우
        if (!scene)
        {
            result.errorMessage = importer.GetErrorString();

            if (result.errorMessage.empty())
            {
                result.errorMessage = "Failed to load model file.";
            }

            return result;
        }

        // 로딩한 aiScene 내부에 렌더링할 유효한 데이터가 존재하지 않는 경우
        if (!scene->mRootNode || scene->mNumMeshes == 0 || !scene->mMeshes)
        {
            result.errorMessage = "Model does not contain a valid mesh scene.";

            return result;
        }

        // 로드된 aiScene에 존재하는 머티리얼 수만큼 공간 확보
        result.modelData.materials.reserve(scene->mNumMaterials);

        for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            // 머티리얼 데이터 구성
            ModelMaterialData modelMaterial;

            // 머티리얼 이름 등록
            modelMaterial.name = "Material_" + std::to_string(materialIndex);

            const aiMaterial* const sourceMaterial = scene->mMaterials[materialIndex];

            if (sourceMaterial)
            {
                aiString materialName;

                // 머티리얼에 부여된 이름이 존재하는 경우 해당 이름 사용
                if (sourceMaterial->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS &&
                    materialName.length > 0
                )
                {
                    modelMaterial.name = materialName.C_Str();
                }

                aiString baseColorTextureReference;

                // 머티리얼 정보를 기반으로 기본 색상 텍스처 조회
                if (TryGetBaseColorTextureReference(*sourceMaterial, baseColorTextureReference))
                {
                    ModelTextureData modelTextureData;

                    // 조회된 기본 색상 텍스처를 CPU 텍스처 데이터로 변환
                    if (TryCreateModelTextureData(*scene, modelFilePath, baseColorTextureReference, modelTextureData))
                    {
                        modelMaterial.baseColorTextureIndex = AddModelTextureData(result.modelData, std::move(modelTextureData));
                    }
                }
            }

            result.modelData.materials.push_back(std::move(modelMaterial));
        }

        // 로드된 aiScene에 존재하는 메쉬 수만큼 공간 확보
        result.modelData.meshes.reserve(scene->mNumMeshes);

        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* const sourceMesh = scene->mMeshes[meshIndex];

            // 삼각형 정적 지오메트리가 없는 메쉬는 렌더링 대상에서 제외
            if (!sourceMesh || !sourceMesh->HasPositions() || sourceMesh->mNumVertices == 0 || sourceMesh->mNumFaces == 0 ||
                (sourceMesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0
            )
            {
                continue;
            }

            ModelMeshData modelMesh;

            // 모델 메쉬 이름 등록
            modelMesh.name = sourceMesh->mName.C_Str();

            if (modelMesh.name.empty())
            {
                modelMesh.name = "MeshPart_" + std::to_string(meshIndex);
            }

            // 머티리얼 인덱스가 유효한 경우 CPU 모델 메쉬와 연결
            if (sourceMesh->mMaterialIndex < result.modelData.materials.size())
            {
                modelMesh.materialIndex = static_cast<std::size_t>(sourceMesh->mMaterialIndex);
            }

            // 모델 메쉬를 이루는 정점 및 인덱스 데이터 공간 확보
            modelMesh.meshData.vertices.reserve(sourceMesh->mNumVertices);
            modelMesh.meshData.indices.reserve(static_cast<std::size_t>(sourceMesh->mNumFaces) * 3);

            // 정점 데이터 등록
            for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex)
            {
                const aiVector3D& sourcePosition = sourceMesh->mVertices[vertexIndex];

                // 기본 색상은 흰색으로 설정
                DirectX::XMFLOAT4 vertexColor = { 1.0f, 1.0f, 1.0f, 1.0f };

                // FBX가 정점별 색상 채널을 제공하면 기본 색상 대신 사용
                if (sourceMesh->HasVertexColors(0))
                {
                    const aiColor4D& sourceColor = sourceMesh->mColors[0][vertexIndex];

                    vertexColor = { sourceColor.r, sourceColor.g, sourceColor.b, sourceColor.a };
                }

                // 정점에 대한 노멀 벡터
                DirectX::XMFLOAT3 vertexNormal = { 0.0f, 0.0f, 0.0f };

                // FBX 모델에 노멀 벡터가 포함되어 있는 경우
                if (sourceMesh->HasNormals())
                {
                    const aiVector3D& sourceNormal = sourceMesh->mNormals[vertexIndex];

                    vertexNormal = { sourceNormal.x, sourceNormal.y, sourceNormal.z };
                }

                // 텍스처 좌표
                DirectX::XMFLOAT2 textureCoordinate = { 0.0f, 0.0f };

                // FBX 모델에 텍스쳐 좌표가 포함되어 있는 경우
                if (sourceMesh->HasTextureCoords(0))
                {
                    const aiVector3D& sourceTextureCoordinate = sourceMesh->mTextureCoords[0][vertexIndex];

                    textureCoordinate = { sourceTextureCoordinate.x, sourceTextureCoordinate.y };
                }

                modelMesh.meshData.vertices.push_back
                (
                    {
                        {
                            sourcePosition.x,
                            sourcePosition.y,
                            sourcePosition.z
                        },
                        vertexColor,
                        vertexNormal,
                        textureCoordinate
                    }
                );
            }

            // 인덱스 데이터 등록
            for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex)
            {
                const aiFace& sourceFace = sourceMesh->mFaces[faceIndex];

                // 삼각분할 후에도 삼각형이 아닌 면이 남아 있으면 실패 처리
                if (sourceFace.mNumIndices != 3 || !sourceFace.mIndices)
                {
                    result.errorMessage = "Model contains a face that could not be triangulated.";

                    return result;
                }

                for (unsigned int faceVertexIndex = 0; faceVertexIndex < 3; ++faceVertexIndex)
                {
                    const unsigned int sourceVertexIndex = sourceFace.mIndices[faceVertexIndex];

                    modelMesh.meshData.indices.push_back(static_cast<std::uint32_t>(sourceVertexIndex));
                }
            }

            result.modelData.meshes.push_back(std::move(modelMesh));
        }

        if (result.modelData.IsEmpty())
        {
            result.errorMessage = "Model does not contain renderable triangle geometry.";
        }

        return result;
    }
}
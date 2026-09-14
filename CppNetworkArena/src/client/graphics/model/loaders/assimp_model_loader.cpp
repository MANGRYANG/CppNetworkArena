#include "assimp_model_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace cna::client
{
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
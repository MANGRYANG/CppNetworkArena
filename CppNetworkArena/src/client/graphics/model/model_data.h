#pragma once

#include "graphics/material/material_data.h"
#include "graphics/mesh/mesh_data.h"

#include <string>
#include <vector>

namespace cna::client
{
    inline constexpr std::size_t InvalidMaterialIndex = std::numeric_limits<std::size_t>::max();

    // 외부 모델을 구성하는 개별 CPU 메쉬 데이터와 이름 및 머티리얼 연결 정보를 보관하는 구조체
    struct ModelMeshData final
    {
        std::string name;
        MeshData meshData;
        std::size_t materialIndex = InvalidMaterialIndex;
    };

    // 하나의 외부 모델에서 불러온 CPU 메쉬, 머티리얼 및 텍스처 원본 데이터를 보관하는 구조체
    struct ModelData final
    {
        std::vector<ModelMeshData> meshes;
        std::vector<ModelMaterialData> materials;
        std::vector<ModelTextureData> textures;

        // 렌더링할 수 있는 메쉬가 하나도 없는지 확인하는 함수
        bool IsEmpty() const noexcept
        {
            return meshes.empty();
        }
    };
}
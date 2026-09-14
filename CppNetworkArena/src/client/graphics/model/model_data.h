#pragma once

#include "graphics/mesh/mesh_data.h"

#include <string>
#include <vector>

namespace cna::client
{
    // 외부 모델을 구성하는 개별 CPU 메쉬 데이터와 이름을 보관하는 구조체
    struct ModelMeshData final
    {
        std::string name;
        MeshData meshData;
    };

    // 하나의 외부 모델에서 불러온 CPU 메쉬 목록을 보관하는 구조체
    struct ModelData final
    {
        std::vector<ModelMeshData> meshes;

        // 렌더링할 수 있는 메쉬가 하나도 없는지 확인하는 함수
        bool IsEmpty() const noexcept
        {
            return meshes.empty();
        }
    };
}
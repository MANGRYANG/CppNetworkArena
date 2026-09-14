#pragma once

#include "vertex3d.h"

#include <cstdint>
#include <vector>

namespace cna::client
{
    // GPU 메쉬 생성에 사용할 CPU 측 정점 및 인덱스 데이터
    struct MeshData final
    {
        std::vector<Vertex3D> vertices;
        std::vector<std::uint32_t> indices;
    };
}
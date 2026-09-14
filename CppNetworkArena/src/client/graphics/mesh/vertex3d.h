#pragma once

#include <DirectXMath.h>

namespace cna::client
{
    // 위치 및 색상으로 구성된 3D 정점 데이터 구조체
    struct Vertex3D final
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };
}
#pragma once

#include <DirectXMath.h>

namespace cna::client
{
    // 위치, 색상, 노멀, 탄젠트 및 텍스처 좌표로 구성된 3D 정점 데이터 구조체
    struct Vertex3D final
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 tangent;
        DirectX::XMFLOAT2 textureCoordinate;
    };
}
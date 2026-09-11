#pragma once

#include <DirectXMath.h>

namespace cna::client
{
    // 렌더 객체의 위치, 회전 및 스케일 정보를 저장하는 3D 변환 구조체
    struct Transform3D final
    {
        // 렌더 객체의 월드 공간에서의 위치
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };

        // 렌더 객체의 각 축에 적용할 라디안 단위 회전값
        DirectX::XMFLOAT3 rotationRadians{ 0.0f, 0.0f, 0.0f };

        // 렌더 객체의 각 축에 적용할 스케일 배율
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

        // 저장된 크기, 회전 및 위치를 결합하여 월드 변환 행렬을 반환하는 함수
        DirectX::XMMATRIX GetWorldMatrix() const noexcept;
    };
}
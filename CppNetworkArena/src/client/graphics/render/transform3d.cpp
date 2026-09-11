#include "transform3d.h"

namespace cna::client
{
    DirectX::XMMATRIX Transform3D::GetWorldMatrix() const noexcept
    {
        // 객체의 스케일 변환 행렬 생성
        const DirectX::XMMATRIX scaleMatrix =
            DirectX::XMMatrixScaling
            (
                scale.x,
                scale.y,
                scale.z
            );

        // 객체의 회전 변환 행렬 생성
        const DirectX::XMMATRIX rotationMatrix =
            DirectX::XMMatrixRotationRollPitchYaw
            (
                rotationRadians.x,
                rotationRadians.y,
                rotationRadians.z
            );

        // 객체의 이동 변환 행렬 생성
        const DirectX::XMMATRIX translationMatrix =
            DirectX::XMMatrixTranslation
            (
                position.x,
                position.y,
                position.z
            );

        // SRT 변환을 결합한 월드 변환 행렬 반환
        return scaleMatrix * rotationMatrix * translationMatrix;
    }
}
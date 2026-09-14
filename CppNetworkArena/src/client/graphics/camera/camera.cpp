#include "camera.h"

#include <cmath>

namespace
{
    // 카메라 방향 벡터의 유효성을 판정할 때 사용할 최소 길이의 제곱
    constexpr float kEpsilon = 0.000001f;

    // 전달된 벡터가 NaN이나 Inf가 아니면서 0에 근접하지 않은 유효 벡터인지 확인하기 위한 내부 헬퍼
    inline bool IsValidVector(DirectX::FXMVECTOR vector) noexcept
    {
        // NaN 및 Inf 상태인지 확인
        if (DirectX::XMVector3IsNaN(vector) || DirectX::XMVector3IsInfinite(vector))
        {
            return false;
        }

        // 0에 가깝지 않은 유효 벡터인지 확인
        return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vector)) > kEpsilon;
    }

    // 카메라 구성 정보를 바탕으로 뷰 변환 행렬을 생성하는 내부 헬퍼
    bool BuildViewMatrix(const cna::client::FixedCamera3D::CameraConfig& cameraConfig, DirectX::XMMATRIX& outViewMatrix) noexcept
    {
        const DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&cameraConfig.position);
        const DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&cameraConfig.target);
        const DirectX::XMVECTOR upHint = DirectX::XMLoadFloat3(&cameraConfig.upHint);

        // 현재 카메라의 전방 벡터 계산
        const DirectX::XMVECTOR forwardVector = DirectX::XMVectorSubtract(target, eye);

        // 카메라 전방 벡터의 크기가 0에 근접하는 경우 뷰 변환 행렬 생성 실패 처리
        if (!IsValidVector(forwardVector))
        {
            return false;
        }

        // 벡터의 크기가 외적 계산에 끼치는 영향을 제거하기 위한 정규화 수행
        const DirectX::XMVECTOR normalizedForward = DirectX::XMVector3Normalize(forwardVector);
        const DirectX::XMVECTOR normalizedUpHint = DirectX::XMVector3Normalize(upHint);

        // 현재 카메라의 우향 벡터 계산
        const DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(normalizedUpHint, normalizedForward);

        // 참조 상방 벡터와 카메라 전방 벡터가 평행에 근접하는 경우 뷰 변환 행렬 생성 실패 처리
        if (!IsValidVector(rightVector))
        {
            return false;
        }

        // 뷰 변환 행렬 생성
        outViewMatrix = DirectX::XMMatrixLookAtLH(eye, target, upHint);

        return true;
    }

    // 카메라 구성 정보를 바탕으로 투영 변환 행렬을 생성하는 내부 헬퍼
    bool BuildProjectionMatrix(const cna::client::FixedCamera3D::CameraConfig& cameraConfig, DirectX::XMMATRIX& outProjectionMatrix) noexcept
    {
        const float fovY = cameraConfig.fieldOfViewYRadians;
        const float aspectRatio = cameraConfig.aspectRatio;
        const float nearPlane = cameraConfig.nearPlane;
        const float farPlane = cameraConfig.farPlane;

        // 유효하지 않은 구성 정보인 경우 투영 변환 행렬 생성 실패 처리
        if (!std::isfinite(fovY) || !std::isfinite(aspectRatio) || !std::isfinite(nearPlane) || !std::isfinite(farPlane))
        {
            return false;
        }

        // 카메라 세로 방향 시야각의 범위 검증
        if (fovY <= 0.0f || fovY >= DirectX::XM_PI)
        {
            return false;
        }

        // 화면 종횡비의 범위 검증
        if (aspectRatio <= 0.0f)
        {
            return false;
        }

        // 클리핑 평면의 범위 검증
        if (nearPlane <= 0.0f || farPlane <= nearPlane)
        {
            return false;
        }

        // 원근 투영 변환 행렬 생성
        outProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearPlane, farPlane);

        return true;
    }
}

namespace cna::client
{
    bool FixedCamera3D::Initialize(const CameraConfig& cameraConfig) noexcept
    {
        DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixIdentity();

        // 뷰 변환 행렬 생성 및 유효성 검증
        if (!BuildViewMatrix(cameraConfig, viewMatrix))
        {
            return false;
        }

        // 투영 변환 행렬 생성 및 유효성 검증
        if (!BuildProjectionMatrix(cameraConfig, projectionMatrix))
        {
            return false;
        }
        
        // 뷰-투영 변환 행렬 결합
        const DirectX::XMMATRIX viewProjection = viewMatrix * projectionMatrix;

        // 연산 결과를 저장용 멤버에 보관
        DirectX::XMStoreFloat4x4(&viewProjectionMatrix_, viewProjection);

        return true;
    }

    DirectX::XMMATRIX FixedCamera3D::GetViewProjectionMatrix() const noexcept
    {
        return DirectX::XMLoadFloat4x4(&viewProjectionMatrix_);
    }
}
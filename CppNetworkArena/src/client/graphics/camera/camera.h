#pragma once

#include <DirectXMath.h>

namespace cna::client
{
    // 3D 공간을 바라보는 고정 카메라를 관리하는 클래스
    class FixedCamera3D final
    {
    public:
        // 카메라를 초기화하기 위한 카메라 구성 정보
        struct CameraConfig
        {
            // 현재 카메라의 위치 정보
            DirectX::XMFLOAT3 position;
            // 현재 카메라가 주시하고 있는 타겟의 위치 정보
            DirectX::XMFLOAT3 target;
            // 카메라 좌표계를 계산하기 위해 기준으로 삼는 참조 상방 벡터
            DirectX::XMFLOAT3 upHint;

            // 카메라의 수직 시야각
            float fieldOfViewYRadians;
            // 화면 종횡비
            float aspectRatio;
            // 뷰 프러스텀 근평면까지의 거리
            float nearPlane;
            // 뷰 프러스텀 원평면까지의 거리
            float farPlane;
        };

        // 카메라의 시점 및 원근 투영 설정을 구성하고 뷰-투영 행렬을 사전 연산하는 함수
        bool Initialize(const CameraConfig& cameraConfig) noexcept;

        // 사전에 연산된 뷰-투영 결합 변환 행렬을 반환하는 함수
        DirectX::XMMATRIX GetViewProjectionMatrix() const noexcept;

    private:
        // 3D 월드 공간 좌표를 클립 공간으로 변환하는 뷰-투영 결합 변환 행렬
        DirectX::XMFLOAT4X4 viewProjectionMatrix_ =
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    };
}
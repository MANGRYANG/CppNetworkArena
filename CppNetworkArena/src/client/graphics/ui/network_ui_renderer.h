#pragma once

#include <Windows.h>

#include <d3d11.h>

namespace cna::client
{
    // Dear ImGui를 사용하여 네트워크 상태 및 패킷 정보를 시각화하는 UI 렌더러 클래스
    class NetworkUiRenderer final
    {
    public:
        NetworkUiRenderer() = default;
        ~NetworkUiRenderer();

        // 복사 생성자 및 복사 대입 연산자 삭제
        NetworkUiRenderer(const NetworkUiRenderer&) = delete;
        NetworkUiRenderer& operator=(const NetworkUiRenderer&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        NetworkUiRenderer(NetworkUiRenderer&&) = delete;
        NetworkUiRenderer& operator=(NetworkUiRenderer&&) = delete;

        // Win32 윈도우와 DirectX 11 자원을 기반으로 네트워크 UI 렌더링 자원을 초기화하는 함수
        bool Initialize
        (
            HWND windowHandle,
            ID3D11Device* device,
            ID3D11DeviceContext* deviceContext
        );

        // 네트워크 UI 렌더링 프레임을 시작하는 함수
        bool BeginFrame();

        // 네트워크 상태를 표시하는 UI 창을 출력하는 함수
        void DrawNetworkWindow();

        // 네트워크 UI 렌더링 프레임을 종료하고 화면에 출력하는 함수
        bool EndFrame();

        // Win32 메시지를 Dear ImGui Win32 Platform Backend에 전달하는 함수
        bool HandleWindowMessage
        (
            HWND hwnd,
            UINT uMsg,
            WPARAM wParam,
            LPARAM lParam
        ) noexcept;

        // 생성된 Dear ImGui 및 Backend 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // 네트워크 UI 렌더러 초기화 여부
        bool initialized_ = false;

        // 현재 네트워크 UI 프레임의 렌더링 진행 여부
        bool frameActive_ = false;

        // Dear ImGui Win32 Platform Backend 초기화 여부
        bool win32BackendInitialized_ = false;

        // Dear ImGui DirectX 11 Renderer Backend 초기화 여부
        bool directX11BackendInitialized_ = false;
    };
}
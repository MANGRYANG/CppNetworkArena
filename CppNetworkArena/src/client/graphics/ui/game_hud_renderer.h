#pragma once

#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <string_view>

namespace cna::client
{
    // Direct2D와 DirectWrite를 사용하여 게임 HUD를 출력하는 렌더러 클래스
    class GameHudRenderer final
    {
    public:
        GameHudRenderer() = default;
        ~GameHudRenderer();

        // 복사 생성자 및 복사 대입 연산자 삭제
        GameHudRenderer(const GameHudRenderer&) = delete;
        GameHudRenderer& operator=(const GameHudRenderer&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        GameHudRenderer(GameHudRenderer&&) = delete;
        GameHudRenderer& operator=(GameHudRenderer&&) = delete;

        // DirectX 11 디바이스와 스왑 체인을 기반으로 HUD 렌더링 자원을 초기화하는 함수
        bool Initialize(ID3D11Device* device, IDXGISwapChain* swapChain);

        // HUD 렌더링 프레임을 시작하는 함수
        bool BeginFrame();

        // 현재 HUD 프레임에 문자열을 출력하는 함수
        void DrawText
        (
            std::wstring_view text,
            float x,
            float y,
            float width,
            float height
        );

        // HUD 렌더링 프레임을 종료하는 함수
        bool EndFrame();

        // 스왑 체인 크기 변경 전에 백 버퍼를 참조하는 HUD 자원을 해제하는 함수
        void PrepareForResize() noexcept;

        // 스왑 체인 크기 변경 후 백 버퍼 기반 HUD 자원을 다시 생성하는 함수
        bool RestoreAfterResize();

        // 생성된 HUD 렌더링 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // 현재 스왑 체인의 백 버퍼를 기반으로 Direct2D 타겟 비트맵을 생성하는 함수
        bool CreateBackBufferResources();

        // 현재 백 버퍼를 참조하는 Direct2D 자원을 정리하는 함수
        void ReleaseBackBufferResources() noexcept;

        // HUD 출력 대상 스왑 체인
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;

        // Direct2D 1.1 자원 생성을 담당하는 팩토리
        Microsoft::WRL::ComPtr<ID2D1Factory1> direct2DFactory_;

        // DirectX 11 디바이스와 연결된 Direct2D 디바이스
        Microsoft::WRL::ComPtr<ID2D1Device> direct2DDevice_;

        // Direct2D 렌더링 명령을 수행하는 디바이스 컨텍스트
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> direct2DDeviceContext_;

        // DirectWrite 텍스트 자원 생성을 담당하는 팩토리
        Microsoft::WRL::ComPtr<IDWriteFactory> directWriteFactory_;

        // HUD 텍스트 형식을 정의하는 DirectWrite 자원
        Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;

        // 현재 스왑 체인 백 버퍼와 연결된 Direct2D 타겟 비트맵
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap_;

        // HUD 텍스트 출력에 사용하는 단색 브러시
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;

        // HUD 렌더러 초기화 여부
        bool initialized_ = false;

        // 현재 HUD 프레임의 렌더링 진행 여부
        bool frameActive_ = false;
    };
}
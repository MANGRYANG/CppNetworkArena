#pragma once

#include <Windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdint>

namespace cna::client
{
    // DirectX 11 그래픽 자원의 생성 및 프레임 출력을 관리하기 위한 렌더러 클래스
    class D3D11Renderer final
    {
    public:
        D3D11Renderer() = default;
        ~D3D11Renderer();

        // 복사 생성자 및 복사 대입 연산자 삭제
        D3D11Renderer(const D3D11Renderer&) = delete;
        D3D11Renderer& operator=(const D3D11Renderer&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        D3D11Renderer(D3D11Renderer&&) = delete;
        D3D11Renderer& operator=(D3D11Renderer&&) = delete;

        // 렌더러 초기화 함수
        bool Initialize(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight);

        // 프레임을 그리기 전 배경색으로 채우는 함수
        bool BeginFrame(float r, float g, float b, float a);

        bool EndFrame();

        // 생성된 DirectX 11 그래픽 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // 디바이스 및 스왑 체인을 생성하는 함수
        bool CreateDeviceAndSwapChain(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight);

        // 렌더 타겟 뷰를 생성하는 함수
        bool CreateRenderTargetView();

        // GPU 리소스를 생성하는 객체
        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        // 리소스를 조작하고 GPU에 Draw 명령을 내리는 객체
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext_;
        // 버퍼 스왑을 통해 화면 송출을 요청하는 객체
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;

        // 렌더 타겟을 가리키는 뷰
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;

        // DirectX 11 그래픽 자원의 초기화 여부를 저장하는 플래그
        bool initialized_ = false;
    };
}
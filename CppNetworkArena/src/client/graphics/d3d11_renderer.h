#pragma once

#include "d3d11_mesh.h"
#include "d3d11_shader_program.h"

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

        // 기본 2D 그래픽스 파이프라인을 사용하여 테스트 사각형을 그리는 함수
        bool DrawTestRectangle();

        // 백 버퍼와 프론트 버퍼를 교체하여 프레임을 출력하는 함수
        bool EndFrame();

        // 스왑 체인에 등록된 클라이언트 영역 크기를 변경하는 함수
        bool Resize(std::uint32_t clientWidth, std::uint32_t clientHeight) noexcept;

        // 생성된 DirectX 11 그래픽 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // 디바이스 및 스왑 체인을 생성하는 함수
        bool CreateDeviceAndSwapChain(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight);

        // 렌더 타겟 뷰를 생성하는 함수
        bool CreateRenderTargetView();

        // 2D 셰이더 프로그램 및 사각형 메쉬를 초기화하는 함수
        bool CreateGraphicsPipeline();

        // 뷰포트 설정 함수
        void SetViewport(std::uint32_t clientWidth, std::uint32_t clientHeight) noexcept;

        // GPU 리소스를 생성하는 객체
        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        // 리소스를 조작하고 GPU에 Draw 명령을 내리는 객체
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext_;
        // 버퍼 스왑을 통해 화면 송출을 요청하는 객체
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;

        // 렌더 타겟을 가리키는 뷰
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;

        // 2D 정점 셰이더와 픽셀 셰이더를 관리하는 셰이더 프로그램
        D3D11ShaderProgram shaderProgram_;

        // 그래픽스 파이프라인 검증용 사각형 메쉬
        D3D11Mesh testQuadMesh_;

        // DirectX 11 그래픽 자원의 초기화 여부를 저장하는 플래그
        bool initialized_ = false;
    };
}
#include "d3d11_renderer.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace
{
    // 렌더러가 요구하는 그래픽 카드의 하드웨어 기능 수준 목록
    constexpr D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0
    };
}

namespace cna::client
{
    D3D11Renderer::~D3D11Renderer()
    {
        Shutdown();
    }

    bool D3D11Renderer::Initialize(const HWND hwnd, const std::uint32_t clientWidth, const std::uint32_t clientHeight)
    {
        // 이미 초기화되었거나 전달된 윈도우 정보가 유효하지 않은 경우 무시
        if (initialized_ || !hwnd || clientWidth == 0 || clientHeight == 0)
        {
            return false;
        }

        // 디바이스 및 스왑 체인 생성
        if (!CreateDeviceAndSwapChain(hwnd, clientWidth, clientHeight))
        {
            // DirectX 11 자원 정리 후 실패 처리
            Shutdown();

            return false;
        }

        // 렌더 타겟 뷰 생성
        if (!CreateRenderTargetView())
        {
            // DirectX 11 자원 정리 후 실패 처리
            Shutdown();

            return false;
        }

        initialized_ = true;

        return true;
    }

    bool D3D11Renderer::BeginFrame(float r, float g, float b, float a)
    {
        // 렌더링에 필요한 그래픽 자원이 준비되지 않은 경우 실패 처리
        if (!initialized_ || !deviceContext_ || !renderTargetView_)
        {
            return false;
        }

        // 화면을 채울 RGBA 색상 데이터 배열을 저장한 배열
        const float clearColor[4] = { r, g, b, a };

        // 출력 병합기 단계에 렌더 타겟 뷰를 바인딩
        deviceContext_->OMSetRenderTargets
        (
            1,
            renderTargetView_.GetAddressOf(),
            nullptr
        );

        // 렌더 타겟 뷰가 참조하고 있는 백 버퍼의 모든 픽셀을 지정된 색상으로 초기화
        deviceContext_->ClearRenderTargetView
        (
            renderTargetView_.Get(),
            clearColor
        );

        return true;
    }

    bool D3D11Renderer::EndFrame()
    {
        // 스왑 체인에 필요한 그래픽 자원이 준비되지 않은 경우 실패 처리
        if (!initialized_ || !swapChain_)
        {
            return false;
        }

        // 수직 동기화 주기에 맞추어 백 버퍼와 프론트 버퍼 교체
        const HRESULT presentResult = swapChain_->Present(1, 0);

        if (FAILED(presentResult))
        {
            return false;
        }

        return true;
    }

    void D3D11Renderer::Shutdown() noexcept
    {
        initialized_ = false;

        // 디바이스 컨텍스트가 참조하고 있는 파이프라인 자원 해제
        if (deviceContext_)
        {
            deviceContext_->ClearState();
        }

        renderTargetView_.Reset();
        swapChain_.Reset();
        deviceContext_.Reset();
        device_.Reset();
    }

    bool D3D11Renderer::CreateDeviceAndSwapChain(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight)
    {
        // 스왑 체인 설명자 구조체
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        // 백 버퍼 표시 모드 설정
        // 백 버퍼의 가로 픽셀 크기 설정
        swapChainDesc.BufferDesc.Width = clientWidth;
        // 백 버퍼의 세로 픽셀 크기 설정
        swapChainDesc.BufferDesc.Height = clientHeight;
        // 특정 주사율을 고정하지 않고 현재 디스플레이 설정을 사용
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        // 화면 색상 데이터 포맷 지정 (RGBA 각각 8비트씩 총 32비트 사용)
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        // 스캔라인 출력 순서 설정 (기본값 사용)
        swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        // 화면 스케일링 방식 설정 (기본값 사용)
        swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        // 안티앨리어싱을 사용하지 않도록 설정
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;

        // 생성할 백 버퍼의 사용 용도 설정 (렌더 타겟으로 사용)
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        // 생성할 백 버퍼의 개수를 1개로 설정
        swapChainDesc.BufferCount = 1;
        // 스왑 체인의 결과물을 전달할 윈도우의 핸들
        swapChainDesc.OutputWindow = hwnd;
        // 창 모드로 설정
        swapChainDesc.Windowed = TRUE;
        // 버퍼 교체 시 이전 내용을 삭제하도록 설정
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        // 기타 특수 기능 사용하지 않음
        swapChainDesc.Flags = 0;

        // 디바이스 생성 시 사용할 특수 기능을 담는 변수
        UINT createDeviceFlags = 0;

        // Debug 모드로 빌드하는 경우 debug layer 기능 플래그 추가
    #if defined(_DEBUG)
            createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

        // 그래픽 카드 검사 후 최종적으로 선택될 기능 수준을 담는 변수
        D3D_FEATURE_LEVEL createdFeatureLevel = {};

        // 디바이스 및 스왑 체인 생성
        HRESULT hr = D3D11CreateDeviceAndSwapChain
        (
            nullptr,                        // 메인 그래픽 카드를 기본값으로 사용
            D3D_DRIVER_TYPE_HARDWARE,       // 드라이버 타입을 하드웨어 방식으로 설정
            nullptr,                        // 드라이버가 하드웨어 방식이라 사용하지 않음
            createDeviceFlags,              // 디버그 모드 관련 플래그 전달
            featureLevels,                  // 그래픽 카드에 요구할 기능 수준 목록 전달
            ARRAYSIZE(featureLevels),       // 기능 수준 목록의 원소 개수 전달
            D3D11_SDK_VERSION,              // DirectX 11 SDK 버전 전달
            &swapChainDesc,                 // 스왑 체인 구조체 주소 전달
            swapChain_.GetAddressOf(),      // 할당된 스왑 체인 객체를 저장할 멤버 변수의 주소
            device_.GetAddressOf(),         // 할당된 디바이스 객체를 저장할 멤버 변수의 주소
            &createdFeatureLevel,           // 최종 선정된 기능 수준을 담을 주소 전달
            deviceContext_.GetAddressOf()   // 할당된 디바이스 컨텍스트 객체를 저장할 멤버 변수의 주소
        );

        // 디바이스 및 스왑 체인 생성에 실패한 경우
        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    bool D3D11Renderer::CreateRenderTargetView()
    {
        // 스왑 체인의 백 버퍼를 임시로 가리키는 스마트 포인터
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

        // backBuffer가 스왑 체인의 백 버퍼에 엑세스할 수 있도록 설정
        HRESULT hr = swapChain_->GetBuffer
        (
            0,
            IID_PPV_ARGS(backBuffer.GetAddressOf())
        );

        // 스왑 체인에서 백 버퍼를 꺼내오지 못한 경우
        if (FAILED(hr))
        {
            return false;
        }

        // backBuffer와 연결된 렌더 타겟 뷰 생성
        hr = device_->CreateRenderTargetView
        (
            backBuffer.Get(),
            nullptr,
            renderTargetView_.GetAddressOf()
        );

        // 렌더 타겟 뷰 생성에 실패한 경우
        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }
}
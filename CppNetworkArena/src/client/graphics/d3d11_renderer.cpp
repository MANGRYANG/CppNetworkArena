#include "d3d11_renderer.h"

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    // 위치 및 색상으로 구성된 2D 정점 데이터 구조체
    struct Vertex2D final
    {
        float position[2];
        float color[4];
    };

    // 고정 카메라의 뷰-투영 결합 변환 행렬을 담는 구조체
    struct CameraData final
    {
        DirectX::XMFLOAT4X4 viewProjectionMatrix;
    };

    // 뷰-투영 결합 변환 행렬은 상수 버퍼 형태로 정점 셰이더에 전달되므로 16바이트 정렬 확인
    static_assert((sizeof(CameraData) % 16) == 0, "Constant buffer size must be 16-byte aligned.");

    // Vertex2D 구조체에 대응하는 Input Layout 설명자 배열
    const D3D11_INPUT_ELEMENT_DESC InputLayoutDescs[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    };

    // 렌더러가 요구하는 그래픽 카드의 하드웨어 기능 수준 목록
    constexpr D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0
    };

    // Windows 환경에서의 최대 경로 길이를 포함할 수 있는 문자열 버퍼 크기 설정
    constexpr DWORD MaxExecutablePathLength = 32768;

    // 게임 화면에서 사용할 가상 해상도의 너비
    constexpr float VirtualScreenWidth = 1280.0f;
    // 게임 화면에서 사용할 가상 해상도의 높이
    constexpr float VirtualScreenHeight = 720.0f;

    // 게임 화면에서 유지할 가상 해상도의 종횡비
    constexpr float VirtualScreenAspectRatio = VirtualScreenWidth / VirtualScreenHeight;

    // 현재 프로세스의 실행 파일이 위치한 디렉터리 경로를 반환하는 함수
    std::filesystem::path GetExecutableDirectory()
    {
        std::wstring executablePathBuffer(MaxExecutablePathLength, L'\0');

        // 현재 프로세스의 실행 파일 경로 저장
        const DWORD executablePathLength = GetModuleFileNameW
        (
            nullptr,
            executablePathBuffer.data(),
            static_cast<DWORD>(executablePathBuffer.size())
        );

        if (executablePathLength == 0 || executablePathLength >= static_cast<DWORD>(executablePathBuffer.size()))
        {
            return {};
        }

        // 경로 버퍼 크기 재조정
        executablePathBuffer.resize(executablePathLength);

        // 실행 파일이 위치한 디렉터리 경로 반환
        return std::filesystem::path(executablePathBuffer).parent_path();
    }
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

        // 2D 셰이더 프로그램 및 사각형 메쉬 초기화
        if (!CreateGraphicsPipeline())
        {
            // DirectX 11 자원 정리 후 실패 처리
            Shutdown();

            return false;
        }

        // 게임 화면의 종횡비가 클라이언트 영역 내부에 유지되도록 뷰포트 설정
        SetFixedAspectRatioViewport(clientWidth, clientHeight);

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

    bool D3D11Renderer::DrawTestRectangle()
    {
        // 사각형 출력에 필요한 파이프라인 자원이 준비되지 않은 경우 실패 처리
        if (!initialized_ || !deviceContext_ || !cameraConstantBuffer_ || !shaderProgram_.IsInitialized() || !testQuadMesh_.IsInitialized())
        {
            return false;
        }

        // 정점 및 픽셀 셰이더를 그래픽스 파이프라인에 바인딩
        shaderProgram_.Bind(deviceContext_.Get());

        // 정점 셰이더의 상수 버퍼 0번 슬롯에 카메라 상수 버퍼 바인딩
        ID3D11Buffer* const cameraConstantBuffer = cameraConstantBuffer_.Get();
        deviceContext_->VSSetConstantBuffers(0, 1, &cameraConstantBuffer);

        // Draw call 호출
        testQuadMesh_.Draw(deviceContext_.Get());

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

    bool D3D11Renderer::Resize(std::uint32_t clientWidth, std::uint32_t clientHeight) noexcept
    {
        // 클라이언트 영역 크기 변경에 필요한 자원이 준비되지 않은 경우 실패 처리
        if (!initialized_ || !device_ || !deviceContext_ || !swapChain_)
        {
            return false;
        }

        // 변경될 클라이언트 영역의 크기가 유효하지 않은 경우 무시
        if (clientWidth == 0 || clientHeight == 0)
        {
            return true;
        }

        // 출력 병합기 단계에 바인딩되어 있던 렌더 타겟 뷰 해제
        deviceContext_->OMSetRenderTargets(0, nullptr, nullptr);

        // 현재 등록된 렌더 타겟 뷰 초기화
        renderTargetView_.Reset();

        // 스왑 체인의 백 버퍼 크기 변경
        const HRESULT resizeResult = swapChain_->ResizeBuffers
        (
            0,                      // 기존 스왑 체인이 가지고 있던 버퍼의 개수 유지
            clientWidth,            // 새로 적용될 클라이언트 영역 너비
            clientHeight,           // 새로 적용될 클라이언트 영역 높이
            DXGI_FORMAT_UNKNOWN,    // 백 버퍼의 픽셀 포맷은 변경하지 않음
            0                       // 기타 특수 플래그 사용하지 않음
        );

        // 백 버퍼 크기 변경에 실패한 경우
        if (FAILED(resizeResult))
        {
            initialized_ = false;

            return false;
        }

        // 변경된 백 버퍼를 기반으로 렌더 타겟 뷰 생성
        if (!CreateRenderTargetView())
        {
            initialized_ = false;

            return false;
        }

        // 변경된 클라이언트 영역을 기반으로 뷰포트 설정 덮어쓰기
        SetFixedAspectRatioViewport(clientWidth, clientHeight);

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

        testQuadMesh_.Shutdown();
        cameraConstantBuffer_.Reset();
        shaderProgram_.Shutdown();
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

    bool D3D11Renderer::CreateGraphicsPipeline()
    {
        // 현재 프로세스의 실행 파일이 위치하는 디렉터리 경로 계산
        const std::filesystem::path executableDirectory = GetExecutableDirectory();

        // 디렉터리가 비어 있는 경우 실패 처리
        if (executableDirectory.empty())
        {
            return false;
        }

        // 셰이더 디렉터리 경로 계산
        const std::filesystem::path shaderDirectory = executableDirectory/L"shaders";

        // 셰이더 파일을 기반으로 셰이더 프로그램 초기화
        if (!shaderProgram_.Initialize(device_.Get(), shaderDirectory/L"VertexShader.hlsl", shaderDirectory/L"PixelShader.hlsl"))
        {
            return false;
        }

        // 고정 카메라 초기화 및 뷰-투영 변환 행렬을 담는 카메라 상수 버퍼 바인딩
        if (!CreateCameraConstantBuffer())
        {
            return false;
        }

        // 화면 중앙에 출력할 그래픽스 파이프라인 검증용 사각형의 정점 목록
        const std::vector<Vertex2D> TestRectangleVertices =
        {
            { { -4.0f,  4.0f }, { 0.10f, 0.75f, 1.00f, 1.00f } },
            { {  4.0f,  4.0f }, { 0.20f, 0.35f, 1.00f, 1.00f } },
            { { -4.0f, -4.0f }, { 0.75f, 0.20f, 1.00f, 1.00f } },
            { {  4.0f, -4.0f }, { 1.00f, 0.75f, 0.20f, 1.00f } }
        };

        // 화면 중앙에 출력할 그래픽스 파이프라인 검증용 사각형의 인덱스 목록
        const std::vector<std::uint16_t> TestRectangleIndices =
        {
            0, 1, 2,
            2, 1, 3
        };

        // 그래픽스 파이프라인 검증용 사각형 메쉬 초기화
        if (!testQuadMesh_.Initialize(device_.Get(), TestRectangleVertices, TestRectangleIndices, InputLayoutDescs, ARRAYSIZE(InputLayoutDescs), shaderProgram_.GetVertexShaderBlob()))
        {
            return false;
        }

        return true;
    }

    bool D3D11Renderer::CreateCameraConstantBuffer()
    {
        if (!device_)
        {
            return false;
        }

        // XY 플레이 평면을 약 60도 각도로 비스듬히 바라보는 고정 카메라 구성
        const FixedCamera3D::CameraConfig cameraConfig =
        {
            { 0.0f, 17.0f, -10.0f },
            { 0.0f,  0.0f,   0.0f },
            { 0.0f,  1.0f,   0.0f },
            DirectX::XM_PIDIV4,
            VirtualScreenAspectRatio,
            0.1f,
            100.0f
        };

        // 고정 카메라의 뷰 및 원근 투영 설정 초기화
        if (!fixedCamera_.Initialize(cameraConfig))
        {
            return false;
        }

        // 상수 버퍼에 전달할 카메라 데이터 구성
        CameraData cameraData = {};

        // 뷰-투영 결합 변환 행렬 구조체에 계산된 변환 행렬 할당
        DirectX::XMStoreFloat4x4
        (
            &cameraData.viewProjectionMatrix,
            fixedCamera_.GetViewProjectionMatrix()
        );

        // 뷰-투영 결합 변환 행렬을 저장할 카메라 상수 버퍼 설명자 구성
        D3D11_BUFFER_DESC constantBufferDesc = {};
        constantBufferDesc.ByteWidth = static_cast<UINT>(sizeof(CameraData));
        constantBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = 0;
        constantBufferDesc.MiscFlags = 0;
        constantBufferDesc.StructureByteStride = 0;

        // 카메라 상수 버퍼를 채울 초기 데이터 구성
        D3D11_SUBRESOURCE_DATA initialData = {};
        initialData.pSysMem = &cameraData;

        // 고정 카메라의 뷰-투영 결합 행렬을 저장하는 상수 버퍼 생성
        const HRESULT createConstantBufferResult = device_->CreateBuffer
        (
            &constantBufferDesc,
            &initialData,
            cameraConstantBuffer_.GetAddressOf()
        );

        // 카메라 상수 버퍼 생성 작업의 성공 여부 반환
        return SUCCEEDED(createConstantBufferResult);
    }

    void D3D11Renderer::SetFixedAspectRatioViewport(std::uint32_t clientWidth, std::uint32_t clientHeight) noexcept
    {
        // 현재 클라이언트 윈도우의 전체 너비
        const float clientWidthFloat = static_cast<float>(clientWidth);
        // 현재 클라이언트 윈도우의 전체 높이
        const float clientHeightFloat = static_cast<float>(clientHeight);

        // 현재 클라이언트 윈도우의 전체 종횡비
        const float clientAspectRatio = clientWidthFloat / clientHeightFloat;

        // 뷰포트의 차원을 정의하는 구조체
        D3D11_VIEWPORT viewport = {};

        // 너비 방향으로 남는 영역이 있는 경우
        if (clientAspectRatio > VirtualScreenAspectRatio)
        {
            // 클라이언트 높이를 기준으로 뷰포트의 가로 픽셀 크기 계산
            viewport.Width = clientHeightFloat * VirtualScreenAspectRatio;
            // 뷰포트의 세로 픽셀 크기 설정
            viewport.Height = clientHeightFloat;

            // 필러 박스를 고려한 뷰포트 영역의 Top-Left X 좌표 계산
            viewport.TopLeftX = (clientWidthFloat - viewport.Width) * 0.5f;
            // 뷰포트 영역의 Top-Left Y 좌표는 0으로 고정
            viewport.TopLeftY = 0.0f;
        }

        // 높이 방향으로 남는 영역이 있거나 게임 화면의 고정 종횡비와 일치하는 경우
        else
        {
            // 뷰포트의 가로 픽셀 크기 설정
            viewport.Width = clientWidthFloat;
            // 클라이언트 너비를 기준으로 뷰포트의 세로 픽셀 크기 계산
            viewport.Height = clientWidthFloat / VirtualScreenAspectRatio;

            // 뷰포트 영역의 Top-Left X 좌표는 0으로 고정
            viewport.TopLeftX = 0.0f;
            // 레터 박스를 고려한 뷰포트 영역의 Top-Left Y 좌표 계산
            viewport.TopLeftY = (clientHeightFloat - viewport.Height) * 0.5f;
        }
        
        // 뷰포트의 최소 깊이 값 설정
        viewport.MinDepth = 0.0f;
        // 뷰포트의 최대 깊이 값 설정
        viewport.MaxDepth = 1.0f;

        // 래스터라이저 단계에 뷰포트를 바인딩
        deviceContext_->RSSetViewports(1, &viewport);
    }
}
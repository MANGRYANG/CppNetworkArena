#include "game_hud_renderer.h"

#include <d2d1_1helper.h>

namespace
{
    // HUD 텍스트에 사용할 시스템 글꼴
    constexpr wchar_t HudFontFamily[] = L"Segoe UI";

    // HUD 텍스트에 사용할 로케일
    constexpr wchar_t HudFontLocale[] = L"en-us";

    // HUD 기본 글꼴 크기
    constexpr float HudFontSize = 20.0f;

    // HUD 배치에 사용할 가상 화면의 너비
    constexpr float HudVirtualScreenWidth = 1280.0f;

    // HUD 배치에 사용할 가상 화면의 높이
    constexpr float HudVirtualScreenHeight = 720.0f;

    // HUD 가상 화면에서 유지할 종횡비
    constexpr float HudVirtualScreenAspectRatio = HudVirtualScreenWidth / HudVirtualScreenHeight;
}

namespace cna::client
{
    GameHudRenderer::~GameHudRenderer()
    {
        Shutdown();
    }

    bool GameHudRenderer::Initialize(ID3D11Device* device, IDXGISwapChain* swapChain)
    {
        // 이미 초기화되었거나 전달된 그래픽 자원이 유효하지 않은 경우 실패 처리
        if (initialized_ || !device || !swapChain)
        {
            return false;
        }

        swapChain_ = swapChain;

        // Direct2D 1.1 팩토리 생성
        HRESULT hr = D2D1CreateFactory
        (
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
            nullptr,
            reinterpret_cast<void**>(direct2DFactory_.GetAddressOf())
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // DirectX 11 디바이스에서 DXGI 디바이스 인터페이스 조회
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;

        hr = device->QueryInterface
        (
            IID_PPV_ARGS(dxgiDevice.GetAddressOf())
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // DirectX 11 디바이스와 연결된 Direct2D 디바이스 생성
        hr = direct2DFactory_->CreateDevice
        (
            dxgiDevice.Get(),
            direct2DDevice_.GetAddressOf()
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // HUD 렌더링 명령을 수행할 Direct2D 디바이스 컨텍스트 생성
        hr = direct2DDevice_->CreateDeviceContext
        (
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            direct2DDeviceContext_.GetAddressOf()
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // DirectWrite 공유 팩토리 생성
        hr = DWriteCreateFactory
        (
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(directWriteFactory_.GetAddressOf())
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // HUD 텍스트 형식 생성
        hr = directWriteFactory_->CreateTextFormat
        (
            HudFontFamily,
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            HudFontSize,
            HudFontLocale,
            textFormat_.GetAddressOf()
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // 텍스트 가운데 정렬
        hr = textFormat_->SetTextAlignment
        (
            DWRITE_TEXT_ALIGNMENT_CENTER
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // 스왑 체인 백 버퍼 기반 Direct2D 자원 생성
        if (!CreateBackBufferResources())
        {
            Shutdown();

            return false;
        }

        // HUD 기본 텍스트 출력에 사용할 흰색 브러시 생성
        hr = direct2DDeviceContext_->CreateSolidColorBrush
        (
            D2D1::ColorF(D2D1::ColorF::White),
            textBrush_.GetAddressOf()
        );

        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        initialized_ = true;

        return true;
    }

    bool GameHudRenderer::BeginFrame()
    {
        // HUD 렌더링 자원이 준비되지 않았거나 이미 프레임이 시작된 경우 실패 처리
        if (!initialized_ || !direct2DDeviceContext_ || !targetBitmap_ || frameActive_)
        {
            return false;
        }

        // Direct2D 렌더링 시작
        direct2DDeviceContext_->BeginDraw();

        // 1280 x 720 HUD 가상 좌표계를 현재 백 버퍼 영역에 맞추는 변환 적용
        direct2DDeviceContext_->SetTransform(hudTransform_);

        frameActive_ = true;

        return true;
    }

    void GameHudRenderer::DrawText(const std::wstring_view text, const float x, const float y, const float width, const float height)
    {
        // HUD 프레임이 진행 중이 아니거나 텍스트 자원이 준비되지 않은 경우 무시
        if (!frameActive_ || !direct2DDeviceContext_ || !textFormat_ || !textBrush_ || text.empty())
        {
            return;
        }

        // 텍스트가 출력될 화면 영역 설정
        const D2D1_RECT_F layoutRect = D2D1::RectF(x, y, x + width, y + height);

        // 현재 텍스트 형식과 브러시를 사용하여 문자열 출력
        direct2DDeviceContext_->DrawText
        (
            text.data(),
            static_cast<UINT32>(text.size()),
            textFormat_.Get(),
            layoutRect,
            textBrush_.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE,
            DWRITE_MEASURING_MODE_NATURAL
        );
    }

    bool GameHudRenderer::EndFrame()
    {
        // HUD 프레임이 시작되지 않은 경우 실패 처리
        if (!initialized_ || !direct2DDeviceContext_ || !targetBitmap_ || !frameActive_)
        {
            return false;
        }

        frameActive_ = false;

        // Direct2D 렌더링 명령 제출
        const HRESULT endDrawResult = direct2DDeviceContext_->EndDraw();

        // Direct2D 렌더링 대상 재생성이 필요한 경우
        if (endDrawResult == D2DERR_RECREATE_TARGET)
        {
            ReleaseBackBufferResources();

            return CreateBackBufferResources();
        }

        return SUCCEEDED(endDrawResult);
    }

    void GameHudRenderer::PrepareForResize() noexcept
    {
        // 크기 변경 전에 기존 백 버퍼를 참조하는 Direct2D 자원 해제
        ReleaseBackBufferResources();
    }

    bool GameHudRenderer::RestoreAfterResize()
    {
        if (!initialized_)
        {
            return false;
        }

        // 변경된 스왑 체인 백 버퍼를 대상으로 Direct2D 자원 재생성
        return CreateBackBufferResources();
    }

    void GameHudRenderer::Shutdown() noexcept
    {
        initialized_ = false;
        frameActive_ = false;

        ReleaseBackBufferResources();

        textBrush_.Reset();
        textFormat_.Reset();

        directWriteFactory_.Reset();

        direct2DDeviceContext_.Reset();
        direct2DDevice_.Reset();
        direct2DFactory_.Reset();

        swapChain_.Reset();

        hudTransform_ = D2D1::Matrix3x2F::Identity();
    }

    bool GameHudRenderer::CreateBackBufferResources()
    {
        if (!swapChain_ || !direct2DDeviceContext_ || targetBitmap_)
        {
            return false;
        }

        // 스왑 체인의 현재 백 버퍼 DXGI Surface 조회
        Microsoft::WRL::ComPtr<IDXGISurface> backBufferSurface;

        HRESULT hr = swapChain_->GetBuffer
        (
            0,
            IID_PPV_ARGS(backBufferSurface.GetAddressOf())
        );

        if (FAILED(hr))
        {
            return false;
        }

        // 스왑 체인 백 버퍼를 Direct2D 렌더링 대상으로 사용하기 위한 비트맵 속성 설정
        const D2D1_BITMAP_PROPERTIES1 bitmapProperties =
            D2D1::BitmapProperties1
            (
                // 비트맵 옵션 설정
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                // 픽셀 포맷 및 알파 모드 설정
                D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                // DPI 설정
                96.0f, 96.0f
            );

        // 스왑 체인 백 버퍼 DXGI Surface와 연결된 Direct2D 타겟 비트맵 생성
        hr = direct2DDeviceContext_->CreateBitmapFromDxgiSurface
        (
            backBufferSurface.Get(),
            &bitmapProperties,
            targetBitmap_.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        // 생성된 비트맵을 Direct2D 디바이스 컨텍스트의 렌더링 대상으로 설정
        direct2DDeviceContext_->SetTarget(targetBitmap_.Get());

        // 현재 백 버퍼 크기를 기준으로 HUD 가상 좌표계 변환 행렬 갱신
        UpdateHudTransform();

        return true;
    }

    void GameHudRenderer::UpdateHudTransform() noexcept
    {
        if (!targetBitmap_)
        {
            hudTransform_ = D2D1::Matrix3x2F::Identity();

            return;
        }

        // 현재 Direct2D 타겟 비트맵의 화면 크기 조회
        const D2D1_SIZE_F targetSize = targetBitmap_->GetSize();

        if (targetSize.width <= 0.0f || targetSize.height <= 0.0f)
        {
            hudTransform_ = D2D1::Matrix3x2F::Identity();

            return;
        }

        // 현재 Direct2D 타겟 비트맵의 종횡비 계산
        const float targetAspectRatio = targetSize.width / targetSize.height;

        float scale = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        // 너비 방향으로 남는 영역이 있는 경우 높이를 기준으로 HUD 크기 조정
        if (targetAspectRatio > HudVirtualScreenAspectRatio)
        {
            scale = targetSize.height / HudVirtualScreenHeight;

            const float scaledHudWidth = HudVirtualScreenWidth * scale;

            // 필러 박스를 고려한 HUD 영역의 X축 시작 위치 계산
            offsetX = (targetSize.width - scaledHudWidth) * 0.5f;
        }

        // 높이 방향으로 남는 영역이 있거나 HUD 가상 화면의 종횡비와 일치하는 경우
        else
        {
            scale = targetSize.width / HudVirtualScreenWidth;

            const float scaledHudHeight = HudVirtualScreenHeight * scale;

            // 레터 박스를 고려한 HUD 영역의 Y축 시작 위치 계산
            offsetY = (targetSize.height - scaledHudHeight) * 0.5f;
        }

        // 1280 x 720 HUD 좌표계를 현재 백 버퍼의 게임 화면 영역에 맞추는 변환 행렬 구성
        hudTransform_ = D2D1::Matrix3x2F
        (
            scale, 0.0f,
            0.0f, scale,
            offsetX, offsetY
        );
    }

    void GameHudRenderer::ReleaseBackBufferResources() noexcept
    {
        frameActive_ = false;

        // 디바이스 컨텍스트와 기존 백 버퍼 비트맵의 연결 해제
        if (direct2DDeviceContext_)
        {
            direct2DDeviceContext_->SetTarget(nullptr);
        }

        // 현재 백 버퍼와 연결된 타겟 비트맵 해제
        targetBitmap_.Reset();
    }
}
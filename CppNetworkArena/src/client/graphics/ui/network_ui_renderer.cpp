#include "network_ui_renderer.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

// Dear ImGui Win32 Platform Backend의 윈도우 메시지 처리 함수 선언
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace cna::client
{
    NetworkUiRenderer::~NetworkUiRenderer()
    {
        Shutdown();
    }

    bool NetworkUiRenderer::Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
    {
        // 이미 초기화되었거나 초기화에 필요한 자원이 유효하지 않은 경우 실패 처리
        if (initialized_ || !windowHandle || !device || !deviceContext)
        {
            return false;
        }

        // Dear ImGui 라이브러리와 헤더 버전 일치 여부 확인
        IMGUI_CHECKVERSION();

        // Dear ImGui Core Context 생성
        ImGui::CreateContext();

        // Dear ImGui 입력 및 설정 정보 조회
        ImGuiIO& io = ImGui::GetIO();

        // 실행 디렉터리에 imgui.ini 파일을 생성하지 않도록 설정
        io.IniFilename = nullptr;

        // 네트워크 상태 UI에 기본 다크 스타일 적용
        ImGui::StyleColorsDark();

        // Win32 Platform Backend 초기화
        if (!ImGui_ImplWin32_Init(windowHandle))
        {
            Shutdown();

            return false;
        }

        win32BackendInitialized_ = true;

        // DirectX 11 Renderer Backend 초기화
        if (!ImGui_ImplDX11_Init(device, deviceContext))
        {
            Shutdown();

            return false;
        }

        directX11BackendInitialized_ = true;

        initialized_ = true;

        return true;
    }

    bool NetworkUiRenderer::BeginFrame()
    {
        // 네트워크 UI 렌더링 자원이 준비되지 않았거나 이미 프레임이 시작된 경우 실패 처리
        if (!initialized_ || frameActive_)
        {
            return false;
        }

        // DirectX 11 Renderer Backend의 새 프레임 준비
        ImGui_ImplDX11_NewFrame();

        // Win32 Platform Backend의 새 프레임 준비
        ImGui_ImplWin32_NewFrame();

        // Dear ImGui 프레임 시작
        ImGui::NewFrame();

        frameActive_ = true;

        return true;
    }

    void NetworkUiRenderer::DrawNetworkWindow()
    {
        if (!frameActive_)
        {
            return;
        }

        // 최초 출력 시 UI 윈도우의 기본 위치 설정
        ImGui::SetNextWindowPos
        (
            ImVec2(20.0f, 20.0f),
            ImGuiCond_FirstUseEver
        );

        // 최초 출력 시 UI 윈도우의 기본 크기 설정
        ImGui::SetNextWindowSize
        (
            ImVec2(360.0f, 180.0f),
            ImGuiCond_FirstUseEver
        );

        // Dear ImGui 렌더링 확인을 위한 UI 윈도우 출력
        ImGui::Begin("Network Monitor");

        ImGui::TextUnformatted("Network UI initialized.");

        ImGui::End();
    }

    bool NetworkUiRenderer::EndFrame()
    {
        // 네트워크 UI 프레임이 시작되지 않은 경우 실패 처리
        if (!initialized_ || !frameActive_)
        {
            return false;
        }

        frameActive_ = false;

        // 현재 Dear ImGui 프레임의 Draw Data 생성
        ImGui::Render();

        // 생성된 Draw Data를 DirectX 11을 사용하여 렌더링
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return true;
    }

    bool NetworkUiRenderer::HandleWindowMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept
    {
        if (!initialized_)
        {
            return false;
        }

        // Win32 메시지를 Dear ImGui Platform Backend에 전달
        return ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam) != 0;
    }

    void NetworkUiRenderer::Shutdown() noexcept
    {
        initialized_ = false;

        // 진행 중인 Dear ImGui 프레임이 존재하는 경우 종료 처리
        if (frameActive_ && ImGui::GetCurrentContext())
        {
            ImGui::EndFrame();
        }

        frameActive_ = false;

        // DirectX 11 Renderer Backend 정리
        if (directX11BackendInitialized_)
        {
            ImGui_ImplDX11_Shutdown();

            directX11BackendInitialized_ = false;
        }

        // Win32 Platform Backend 정리
        if (win32BackendInitialized_)
        {
            ImGui_ImplWin32_Shutdown();

            win32BackendInitialized_ = false;
        }

        // Dear ImGui 컨텍스트 정리
        if (ImGui::GetCurrentContext())
        {
            ImGui::DestroyContext();
        }
    }
}
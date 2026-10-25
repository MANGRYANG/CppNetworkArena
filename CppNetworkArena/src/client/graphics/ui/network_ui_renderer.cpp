#include "network_ui_renderer.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

// Dear ImGui Win32 Platform Backend의 윈도우 메시지 처리 함수 선언
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace
{
    // 게임 진행 상태에 대응하는 문자열을 반환하는 함수
    const char* GetGamePlayStateText(const cna::GamePlayState gamePlayState) noexcept
    {
        switch (gamePlayState)
        {
        case cna::GamePlayState::Waiting:
            return "Waiting";

        case cna::GamePlayState::Playing:
            return "Playing";

        case cna::GamePlayState::Finished:
            return "Finished";

        default:
            return "Unknown";
        }
    }

    // 플레이어 진영에 대응하는 문자열을 반환하는 함수
    const char* GetPlayerSideText(const cna::PlayerSide playerSide) noexcept
    {
        switch (playerSide)
        {
        case cna::PlayerSide::None:
            return "None";

        case cna::PlayerSide::Flame:
            return "Flame";

        case cna::PlayerSide::Frost:
            return "Frost";

        default:
            return "Unknown";
        }
    }

    // 네트워크 메시지 송수신 방향에 대응하는 문자열을 반환하는 함수
    const char* GetPacketDirectionText(const cna::client::NetworkUiPacketDirection direction) noexcept
    {
        switch (direction)
        {
        case cna::client::NetworkUiPacketDirection::Send:
            return "SEND";

        case cna::client::NetworkUiPacketDirection::Receive:
            return "RECV";

        default:
            return "UNKNOWN";
        }
    }

    // 네트워크 메시지 타입에 대응하는 문자열을 반환하는 함수
    const char* GetMessageTypeText(const cna::network::MessageType messageType) noexcept
    {
        switch (messageType)
        {
        case cna::network::MessageType::Ping:
            return "Ping";

        case cna::network::MessageType::Pong:
            return "Pong";

        case cna::network::MessageType::PlayerInput:
            return "PlayerInput";

        case cna::network::MessageType::WorldStateSnapshot:
            return "WorldStateSnapshot";

        case cna::network::MessageType::PlayerIdentity:
            return "PlayerIdentity";

        case cna::network::MessageType::Unknown:
        default:
            return "Unknown";
        }
    }
}

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

    void NetworkUiRenderer::DrawNetworkWindow(const NetworkUiData& data)
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
            ImVec2(520.0f, 380.0f),
            ImGuiCond_FirstUseEver
        );

        // Dear ImGui 렌더링 확인을 위한 UI 윈도우 출력
        ImGui::Begin("Network Monitor");

        // 현재 서버 연결 상태 출력
        ImGui::Text("Connection: %s", data.connectionState);

        // 마지막으로 측정된 서버 왕복 지연 시간 출력
        if (data.rttMilliseconds)
        {
            ImGui::Text("RTT: %.2f ms", *data.rttMilliseconds);
        }
        else
        {
            ImGui::TextUnformatted("RTT: -");
        }

        // 현재 Room ID 출력
        if (data.roomId)
        {
            ImGui::Text("Room ID: %u", static_cast<unsigned int>(*data.roomId));
        }
        else
        {
            ImGui::TextUnformatted("Room ID: -");
        }

        // 현재 로컬 Player ID 출력
        if (data.playerId)
        {
            ImGui::Text("Player ID: %u", static_cast<unsigned int>(*data.playerId));
        }
        else
        {
            ImGui::TextUnformatted("Player ID: -");
        }

        // 수평선 출력
        ImGui::Separator();

        // 마지막으로 적용된 서버 Tick 출력
        if (data.latestServerTick)
        {
            ImGui::Text("Latest Server Tick: %llu", static_cast<unsigned long long>(*data.latestServerTick));
        }
        else
        {
            ImGui::TextUnformatted("Latest Server Tick: -");
        }

        // 현재 보관 중인 월드 상태 스냅샷 개수 출력
        ImGui::Text
        (
            "Snapshot History: %llu / %llu",
            static_cast<unsigned long long>(data.snapshotHistorySize),
            static_cast<unsigned long long>(data.snapshotHistoryCapacity)
        );

        // 수평선 출력
        ImGui::Separator();

        // 최신 월드 상태가 존재하는 경우 게임 상태 정보 출력
        if (data.gamePlayState)
        {
            ImGui::Text("Game State: %s", GetGamePlayStateText(*data.gamePlayState));
            ImGui::Text("Attacking Side: %s", data.attackingSide ? GetPlayerSideText(*data.attackingSide) : "-");
            ImGui::Text("Score: FLAME %u : %u FROST", static_cast<unsigned int>(data.flameScore), static_cast<unsigned int>(data.frostScore));
        }
        else
        {
            ImGui::TextUnformatted("Game State: -");
            ImGui::TextUnformatted("Attacking Side: -");
            ImGui::TextUnformatted("Score: -");
        }

        // 수평선 출력
        ImGui::Separator();

        // 네트워크 메시지 송수신 기록 제목 출력
        ImGui::TextUnformatted("[Packet Log]");

        // 최근 송수신 메시지 정보를 테이블 형태로 출력
        if (ImGui::BeginTable("PacketLogTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0.0f, 0.0f)))
        {
            // 테이블 열 구성
            ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("MessageType");
            ImGui::TableSetupColumn("Payload", ImGuiTableColumnFlags_WidthFixed, 80.0f);

            ImGui::TableHeadersRow();

            // 가장 최근 메시지부터 역순으로 출력
            for (std::size_t index = data.packetLogSize; index > 0; --index)
            {
                const NetworkUiPacketLogEntry& packet = data.packetLog[index - 1];

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(GetPacketDirectionText(packet.direction));

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(GetMessageTypeText(packet.messageType));

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%llu Byte", static_cast<unsigned long long>(packet.payloadSize));
            }

            ImGui::EndTable();
        }

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
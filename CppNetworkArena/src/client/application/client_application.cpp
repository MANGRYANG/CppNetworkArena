#include "client_application.h"

#include <boost/asio/error.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>

namespace
{
    // 클라이언트가 연결할 서버 호스트
    constexpr std::string_view TargetHost = "127.0.0.1";

    // 클라이언트가 연결할 서버 포트
    constexpr std::uint16_t TargetPort = 7777;

    // DirectX 렌더링 영역으로 사용할 기본 클라이언트 너비
    constexpr int InitialClientWidth = 1280;

    // DirectX 렌더링 영역으로 사용할 기본 클라이언트 높이
    constexpr int InitialClientHeight = 720;

    // 렌더링 루프가 추가되기 전 CPU 점유율을 제한하는 메시지 대기 시간
    constexpr DWORD MessageWaitMilliseconds = 1;
}

namespace cna::client
{
    ClientApplication::ClientApplication(const HINSTANCE hInstance)
        : networkClient_(std::make_shared<NetworkClient>(ioContext_)), window_(hInstance)
    {
    }

    ClientApplication::~ClientApplication()
    {
        Shutdown();
    }

    int ClientApplication::Run()
    {
        // 애플리케이션이 이미 실행 중이거나 Win32 윈도우가 이미 생성되어 있는 경우
        if (running_ || window_.IsCreated())
        {
            return 1;
        }

        // DirectX Swap Chain이 사용할 Win32 윈도우 생성
        if (!window_.Create(L"CppNetworkArena", InitialClientWidth, InitialClientHeight))
        {
            // Win32 윈도우 생성 실패 메시지 출력
            std::cerr << "[GameClient] Failed to create Win32 window" << '\n';

            return 1;
        }

        // Win32 윈도우를 화면에 표시 
        window_.Show();

        running_ = true;
        exitCode_ = 0;

        std::cout << "CppNetworkArena GameClient starting..." << '\n';

        // 서버에 클라이언트 연결
        if (!StartConnection())
        {
            // 서버에 연결하지 못한 경우 실패 메시지 출력
            std::cerr << "[NetworkClient] Connection request was rejected." << '\n';

            RequestExit(1);
        }

        // 애플리케이션 실행 루프 시작
        while (running_)
        {
            // OS 메시지 확인 및 처리
            window_.ProcessMessages(running_);

            if (!running_)
            {
                break;
            }

            // 대기 중인 네트워크 완료 이벤트 일괄 처리
            ProcessNetworkEvents();

            if (!running_)
            {
                break;
            }

            // 다음 Win32 메시지를 짧게 대기하여 CPU 과다 점유 방지
            MsgWaitForMultipleObjectsEx(0, nullptr, MessageWaitMilliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }

        // 애플리케이션 종료
        Shutdown();

        return exitCode_;
    }

    bool ClientApplication::StartConnection()
    {
        return networkClient_->Connect
        (
            TargetHost,
            TargetPort,
            [this](const boost::asio::ip::tcp::endpoint& endpoint)
            {
                HandleConnected(endpoint);
            },
            [this](const boost::system::error_code& error)
            {
                HandleConnectionFailed(error);
            },
            [this](const boost::system::error_code& error)
            {
                HandleDisconnected(error);
            },
            [this](const cna::network::PlayerIdentityPayload& identity)
            {
                HandlePlayerIdentity(identity);
            },
            [this](const cna::network::WorldStateSnapshot& snapshot)
            {
                HandleWorldStateSnapshot(snapshot);
            }
        );
    }

    void ClientApplication::ProcessNetworkEvents()
    {
        // 대기 중인 네트워크 완료 이벤트 핸들러 일괄 실행
        ioContext_.poll();
    }

    void ClientApplication::Shutdown() noexcept
    {
        // 애플리케이션 실행 루프 중지
        running_ = false;

        // 네트워크 연결 해제
        if (networkClient_ && (networkClient_->GetConnectionState() != NetworkClient::ConnectionState::Disconnected))
        {
            networkClient_->Disconnect();
        }

        // Win32 플랫폼 윈도우 제거
        window_.Destroy();

        // 종료된 연결의 플레이어 식별 정보 및 월드 상태 초기화
        clientGameState_.Reset();

        // IO 컨텍스트 중지
        ioContext_.stop();
    }

    void ClientApplication::RequestExit(const int exitCode) noexcept
    {
        // 종료 코드 설정
        exitCode_ = exitCode;

        // 애플리케이션 실행 루프 중지
        running_ = false;
    }

    void ClientApplication::HandleConnected(const boost::asio::ip::tcp::endpoint& endpoint)
    {
        // 서버 연결 성공 메시지 출력
        std::cout
            << "[NetworkClient] Connected: endpoint="
            << endpoint.address().to_string()
            << ':' << endpoint.port()
            << '\n';
    }

    void ClientApplication::HandleConnectionFailed(const boost::system::error_code& error)
    {
        // 실패한 연결 시도의 게임 상태 초기화
        clientGameState_.Reset();

        // 서버 연결 실패 메시지 출력
        std::cerr
            << "[NetworkClient] Connection failed: "
            << error.message()
            << '\n';
    }

    void ClientApplication::HandleDisconnected(const boost::system::error_code& error)
    {
        // 종료된 연결의 게임 상태 초기화
        clientGameState_.Reset();

        // 서버가 연결을 정상적으로 종료한 경우
        if (error == boost::asio::error::eof)
        {
            std::cout << "[NetworkClient] Server disconnected." << '\n';

            return;
        }

        // 오류가 발생하여 종료된 경우
        std::cerr
            << "[NetworkClient] Connection terminated: "
            << error.message()
            << '\n';
    }

    void ClientApplication::HandlePlayerIdentity(const cna::network::PlayerIdentityPayload& identity)
    {
        // 서버가 할당한 로컬 플레이어 식별 정보를 게임 상태 계층에 적용
        if (!clientGameState_.ApplyPlayerIdentity(identity))
        {
            std::cerr
                << "[GameClient] PlayerIdentity rejected"
                << ": roomId=" << identity.roomId
                << ", playerId=" << identity.playerId
                << '\n';

            return;
        }

        // 게임 상태 계층에 저장된 Room ID 조회
        const std::optional<cna::RoomId> roomId = clientGameState_.GetRoomId();
        // 게임 상태 계층에 저장된 로컬 Player ID 조회
        const std::optional<cna::PlayerId> localPlayerId = clientGameState_.GetLocalPlayerId();

        // 식별 정보 적용 후 조회할 수 없는 경우
        if (!roomId || !localPlayerId)
        {
            std::cerr << "[GameClient] PlayerIdentity unavailable after apply" << '\n';

            return;
        }

        std::cout
            << "[GameClient] PlayerIdentity received"
            << ": roomId=" << *roomId
            << ", playerId=" << *localPlayerId
            << '\n';
    }

    void ClientApplication::HandleWorldStateSnapshot(const cna::network::WorldStateSnapshot& snapshot)
    {
        // 서버에서 받은 월드 상태 스냅샷을 게임 상태 계층에 적용
        if (!clientGameState_.ApplyWorldStateSnapshot(snapshot))
        {
            std::cerr
                << "[GameClient] WorldStateSnapshot rejected"
                << ": serverTick=" << snapshot.serverTick
                << ", roomId=" << snapshot.roomId
                << '\n';

            return;
        }

        // 게임 상태 계층에 저장된 최신 월드 상태 조회
        const cna::network::WorldStateSnapshot* worldState = clientGameState_.GetWorldState();

        // 월드 상태가 적용되지 않은 경우
        if (!worldState)
        {
            return;
        }

        std::cout
            << "[GameClient] WorldStateSnapshot received"
            << ": serverTick=" << worldState->serverTick
            << ", roomId=" << worldState->roomId
            << ", playerCount=" << worldState->players.size()
            << '\n';
    }
}
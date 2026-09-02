#pragma once

#include "../game/client_game_state.h"
#include "../network/network_client.h"
#include "../platform/win32_window.h"

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <memory>

namespace cna::client
{
    // Win32 창과 네트워크 클라이언트의 실행 생명 주기를 관리하는 애플리케이션 클래스
    class ClientApplication final
    {
    public:
        explicit ClientApplication(HINSTANCE hInstance);
        ~ClientApplication();

        // 복사 생성자 및 복사 대입 연산자 삭제
        ClientApplication(const ClientApplication&) = delete;
        ClientApplication& operator=(const ClientApplication&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        ClientApplication(ClientApplication&&) = delete;
        ClientApplication& operator=(ClientApplication&&) = delete;

        // 윈도우 및 네트워크를 초기화하고 애플리케이션 실행 루프를 시작하는 함수
        int Run();

    private:
        // 비동기 서버 연결을 시작하는 함수
        bool StartConnection();

        // 대기 중인 네트워크 완료 이벤트를 현재 스레드에서 처리하는 함수
        void ProcessNetworkEvents();

        // 애플리케이션의 네트워크 연결과 게임 상태를 정리하는 함수
        void Shutdown() noexcept;

        // 오류 코드와 함께 애플리케이션 실행 루프 종료를 요청하는 함수
        void RequestExit(int exitCode) noexcept;

        // 서버 연결 완료 이벤트를 처리하는 함수
        void HandleConnected(const boost::asio::ip::tcp::endpoint& endpoint);

        // 서버 연결 실패 이벤트를 처리하는 함수
        void HandleConnectionFailed(const boost::system::error_code& error);

        // 서버 연결 종료 이벤트를 처리하는 함수
        void HandleDisconnected(const boost::system::error_code& error);

        // 플레이어 식별 정보 수신 이벤트를 처리하는 함수
        void HandlePlayerIdentity(const cna::network::PlayerIdentityPayload& identity);

        // 월드 상태 스냅샷 수신 이벤트를 처리하는 함수
        void HandleWorldStateSnapshot(const cna::network::WorldStateSnapshot& snapshot);

        // 비동기 네트워크 작업을 실행하는 IO 컨텍스트
        boost::asio::io_context ioContext_;

        // 비동기 통신 중 댕글링 포인터를 방지하기 위한 네트워크 클라이언트
        std::shared_ptr<NetworkClient> networkClient_;

        // 서버에서 수신한 현재 클라이언트 게임 상태
        ClientGameState clientGameState_;

        // GameClient가 표시하는 Win32 플랫폼 윈도우
        Win32Window window_;

        // 애플리케이션 루프의 상태 플래그
        bool running_ = false;

        // 애플리케이션 종료 시 운영체제에 반환할 종료 코드
        int exitCode_ = 1;
    };
}
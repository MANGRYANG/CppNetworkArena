#pragma once

#include "../game/room_manager.h"

#include "session_manager.h"

#include <NetworkTypes.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <memory>

namespace cna::server
{
    class Session;

    // 비동기 TCP 서버 클래스
    class Server final
    {
    public:
        // io_context 및 port를 인자로 받는 생성자
        Server(boost::asio::io_context& ioContext, std::uint16_t port);

        // 서버 종료 시 활성 상태의 연결을 정리하기 위한 소멸자
        ~Server();

        // 복사 생성자 및 복사 대입 연산자 삭제
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;

        // 비동기 TCP 접속 처리 로직을 시작하기 위한 함수
        void Start();

        // 서버 종료 요청을 처리하는 함수
        void Stop();

    private:
        using Tcp = boost::asio::ip::tcp;

        // 다음 클라이언트 연결을 비동기적으로 수락 후 대기하는 함수
        void AcceptNext();

        // 클라이언트 연결 수락 완료 시 호출되는 핸들러 함수
        void HandleAccept(const boost::system::error_code& error, Tcp::socket socket);

        // 기본으로 사용할 Room을 생성하는 함수
        void CreateDefaultRoom();

        // 생성된 세션을 기반으로 하는 플레이어를 기본 Room에 입장시키는 함수
        bool EnterDefaultRoom(std::shared_ptr<Session> session);

        // 종료된 세션을 기반으로 하는 플레이어를 기본 Room에서 퇴장시키는 함수
        void LeaveDefaultRoom(SessionId sessionId);

        // 네트워크 수신을 담당하는 비동기 Acceptor 객체
        Tcp::acceptor acceptor_;

        // 활성 클라이언트 세션 관리자
        SessionManager sessionManager_;

        // 서버에서 사용할 Room 관리자 객체
        RoomManager roomManager_;

        // 클라이언트를 배치할 Room의 기본 ID
        cna::RoomId defaultRoomId_ = 0;

        // 서버 종료 요청이 처리되었는지 나타내는 상태 값
        bool stopped_ = false;
    };
}
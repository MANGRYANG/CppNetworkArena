#include "server.h"

#include "session.h"

#include <boost/asio/error.hpp>
#include <boost/asio/socket_base.hpp>

#include <iostream>
#include <memory>
#include <utility>

namespace cna::server
{
    Server::Server(boost::asio::io_context& ioContext, const std::uint16_t port) : acceptor_(ioContext)
    {
        // port를 통해 들어오는 모든 IPv4 인터페이스의 연결을 수락
        const Tcp::endpoint endpoint(Tcp::v4(), port);

        // OS에 소켓 생성 시스템 콜 전달
        acceptor_.open(endpoint.protocol());
        // TIME_WAIT 상태인 경우에도 port에 바인딩 가능하도록 설정
        acceptor_.set_option(Tcp::acceptor::reuse_address(true));
        // 소켓에 Endpoint 바인딩
        acceptor_.bind(endpoint);
        // 소켓을 리스닝 소켓으로 전환
        acceptor_.listen(boost::asio::socket_base::max_listen_connections);
    }

    Server::~Server()
    {
        // 서버 객체 소멸 시 남아있는 자원 정리
        Stop();
    }

    void Server::Start()
    {
        // 이미 종료 처리된 서버는 시작하지 않음
        if (stopped_)
        {
            return;
        }

        // 현재 바인딩된 엔드포인트 정보 출력
        const Tcp::endpoint localEndpoint = acceptor_.local_endpoint();

        std::cout << "Listening on " << localEndpoint.address().to_string()
            << ':' << localEndpoint.port() << '\n';

        // 클라이언트 접속 대기 로직 호출
        AcceptNext();
    }

    void Server::AcceptNext()
    {
        // 이미 서버가 종료되었거나 리스닝 소켓이 닫힌 경우
        if (stopped_ || !acceptor_.is_open())
        {
            return;
        }

        // 클라이언트 접속 시 익명 람다 함수 콜백
        acceptor_.async_accept
        (
            [this](const boost::system::error_code& error, Tcp::socket socket)
            {
                // 클라이언트 연결 검증 및 처리
                HandleAccept(error, std::move(socket));
            }
        );
    }

    void Server::HandleAccept(const boost::system::error_code& error, Tcp::socket socket)
    {
        // 이미 서버가 종료된 경우
        if (stopped_)
        {
            return;
        }

        // 클라이언트 연결에 성공한 경우
        if (!error)
        {
            // 클라이언트와의 연결을 관리하는 세션에 부여할 ID 생성
            const SessionId sessionId = sessionManager_.GenerateSessionId();

            // 접속된 소켓의 소유권을 Session 객체로 이전
            const std::shared_ptr<Session> session =
                std::make_shared<Session>
                (
                    sessionId,
                    std::move(socket),
                    [this](const SessionId closedSessionId)
                    {
                        // 종료된 세션을 활성 세션 목록에서 제거
                        sessionManager_.Remove(closedSessionId);
                    }
                );

            // 활성 세션 목록에 등록
            sessionManager_.Add(session);

            // 클라이언트 연결 처리 시작
            session->Start();
        }

        // 클라이언트 연결에 실패한 경우
        else if (error != boost::asio::error::operation_aborted)
        {
            // 에러 메시지 출력
            std::cerr << "Accept failed: " << error.message() << '\n';
        }

        // 리스닝 소켓이 열려 있는 경우
        if (acceptor_.is_open())
        {
            // 클라이언트 접속 대기 로직 호출
            AcceptNext();
        }
    }

    void Server::Stop()
    {
        // 이미 서버 종료 처리된 경우
        if (stopped_)
        {
            return;
        }

        stopped_ = true;

        // 리스닝 소켓이 열려 있는 경우 새 연결 수락을 중단
        if (acceptor_.is_open())
        {
            boost::system::error_code closeError;

            acceptor_.close(closeError);

            // 리스닝 소켓 연결 종료 중 에러가 발생한 경우
            if (closeError)
            {
                std::cerr << "Failed to close acceptor: " << closeError.message() << '\n';
            }
        }

        // 현재 활성화된 모든 세션 종료
        sessionManager_.CloseAll();

        // 서버 종료 메시지 출력
        std::cout << "Server stopped" << '\n';
    }
}
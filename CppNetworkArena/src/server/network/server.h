#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>

namespace cna::server
{
    // 비동기 TCP 서버 클래스
    class Server final
    {
    public:
        // io_context 및 port를 인자로 받는 생성자
        Server(boost::asio::io_context& ioContext, std::uint16_t port);

        // 복사 생성자 및 복사 대입 연산자 삭제
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        Server(Server&&) = delete;
        Server& operator=(Server&&) = delete;

        // 비동기 TCP 접속 처리 로직을 시작하기 위한 함수
        void Start();

    private:
        using Tcp = boost::asio::ip::tcp;

        // 다음 클라이언트 연결을 비동기적으로 수락 후 대기하는 함수
        void AcceptNext();

        // 클라이언트 연결 수락 완료 시 호출되는 핸들러 함수
        void HandleAccept(const boost::system::error_code& error, Tcp::socket socket);

        // 네트워크 수신을 담당하는 비동기 Acceptor 객체
        Tcp::acceptor acceptor_;
    };
}
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>

#include "network/server.h"

int main(void)
{
    try
    {
        // IO 컨텍스트 생성
        boost::asio::io_context ioContext;
        // 사용할 포트 번호 정의
        constexpr std::uint16_t serverPort = 7777;

        // 서버 객체 생성
        cna::server::Server server(ioContext, serverPort);

        std::cout << "CppNetworkArena GameServer starting...\n";

        boost::asio::signal_set signals
        (
            ioContext,
            SIGINT,
            SIGTERM
        );

        // 콘솔 종료 신호를 받으면 서버 종료 절차 수행
        signals.async_wait
        (
            [&server](const boost::system::error_code& error, const int signalNumber)
            {
                // 신호 대기 작업이 취소된 경우
                if (error)
                {
                    return;
                }

                std::cout << "Stop signal received: " << signalNumber << '\n';

                // 서버 종료 요청
                server.Stop();
            }
        );

        // 리스닝 모드 진입 및 연결 수락 준비
        server.Start();

        // 이벤트 루프 실행
        ioContext.run();
    }

    // 예외 처리
    catch (const std::exception& exception)
    {
        std::cerr << "Server fatal error: " << exception.what() << '\n';

        return 1;
    }

    return 0;
}
#include "network/network_client.h"

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

int main(void)
{
    try
    {
        // IO 컨텍스트 생성
        boost::asio::io_context ioContext;

        // 클라이언트를 연결할 호스트 정의
        constexpr std::string_view targetHost = "127.0.0.1";

        // 사용할 포트 번호 정의
        constexpr std::uint16_t targetPort = 7777;

        // 네트워크 클라이언트 객체 생성
        const std::shared_ptr<cna::client::NetworkClient> networkClient = std::make_shared<cna::client::NetworkClient>(ioContext);

        // 클라이언트 종료 코드
        int exitCode = 1;

        // 클라이언트 시작 메시지 출력
        std::cout << "CppNetworkArena GameClient starting...\n";

        // 서버에 클라이언트 연결
        const bool connectStarted = networkClient->Connect
        (
            targetHost,
            targetPort,
            [&exitCode](const boost::asio::ip::tcp::endpoint& endpoint)
            {
                // 서버 연결 성공 메시지 출력
                std::cout
                    << "[NetworkClient] Connected: endpoint="
                    << endpoint.address().to_string()
                    << ':' << endpoint.port()
                    << '\n';

                // 연결에 성공하였으므로 종료 코드를 0으로 설정
                exitCode = 0;
            },
            [&exitCode](const boost::system::error_code& error)
            {
                // 서버 연결 실패 메시지 출력
                std::cerr
                    << "[NetworkClient] Connection failed: "
                    << error.message()
                    << '\n';

                // 연결에 실패하였으므로 종료 코드를 1로 설정
                exitCode = 1;
            },
            [&exitCode](const boost::system::error_code& error)
            {
                // 서버가 연결을 정상적으로 종료한 경우
                if (error == boost::asio::error::eof)
                {
                    std::cout << "[NetworkClient] Server disconnected.\n";

                    exitCode = 0;

                    return;
                }

                // 오류가 발생하여 종료된 경우
                std::cerr
                    << "[NetworkClient] Connection terminated: "
                    << error.message()
                    << '\n';

                exitCode = 1;
            },
            [](const cna::network::WorldStateSnapshot& snapshot)
            {
                std::cout
                    << "[GameClient] WorldStateSnapshot received"
                    << ": roomId=" << snapshot.roomId
                    << ", playerCount=" << snapshot.players.size()
                    << '\n';
            }
        );

        // 비동기 연결 작업 자체에 실패한 경우
        if (!connectStarted)
        {
            // 연결 시도 거부 메시지 출력
            std::cerr << "[NetworkClient] Connection request was rejected.\n";

            return 1;
        }

        // 이벤트 루프 실행
        ioContext.run();

        return exitCode;
    }

    // 예외 처리
    catch (const std::exception& exception)
    {
        std::cerr << "Client fatal error: " << exception.what() << '\n';

        return 1;
    }
}
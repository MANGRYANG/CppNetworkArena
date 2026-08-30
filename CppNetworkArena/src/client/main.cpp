#include "game/client_game_state.h"
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

        // 서버에서 전달받은 게임 상태를 관리할 객체 생성
        cna::client::ClientGameState clientGameState;

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
            [&clientGameState, &exitCode](const boost::system::error_code& error)
            {
                // 실패한 연결 시도의 게임 상태 초기화
                clientGameState.Reset();

                // 서버 연결 실패 메시지 출력
                std::cerr
                    << "[NetworkClient] Connection failed: "
                    << error.message()
                    << '\n';

                // 연결에 실패하였으므로 종료 코드를 1로 설정
                exitCode = 1;
            },
            [&clientGameState, &exitCode](const boost::system::error_code& error)
            {
                // 종료된 연결의 게임 상태 초기화
                clientGameState.Reset();

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
            [&clientGameState](const cna::network::PlayerIdentityPayload& identity)
            {
                // 서버가 할당한 로컬 플레이어 식별 정보를 게임 상태 계층에 적용
                if (!clientGameState.ApplyPlayerIdentity(identity))
                {
                    std::cerr
                        << "[GameClient] PlayerIdentity rejected"
                        << ": roomId=" << identity.roomId
                        << ", playerId=" << identity.playerId
                        << '\n';

                    return;
                }

                // 게임 상태 계층에 저장된 Room ID 조회
                const std::optional<cna::RoomId> roomId = clientGameState.GetRoomId();
                // 게임 상태 계층에 저장된 로컬 Player ID 조회
                const std::optional<cna::PlayerId> localPlayerId = clientGameState.GetLocalPlayerId();

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
            },
            [&clientGameState](const cna::network::WorldStateSnapshot& snapshot)
            {
                // 서버에서 받은 월드 상태 스냅샷을 게임 상태 계층에 적용
                if (!clientGameState.ApplyWorldStateSnapshot(snapshot))
                {
                    std::cerr
                        << "[GameClient] WorldStateSnapshot rejected"
                        << ": roomId=" << snapshot.roomId
                        << '\n';

                    return;
                }

                // 게임 상태 계층에 저장된 최신 월드 상태 조회
                const cna::network::WorldStateSnapshot* worldState = clientGameState.GetWorldState();

                // 월드 상태가 적용되지 않은 경우
                if (!worldState)
                {
                    return;
                }

                std::cout
                    << "[GameClient] WorldStateSnapshot received"
                    << ": roomId=" << worldState->roomId
                    << ", playerCount=" << worldState->players.size()
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
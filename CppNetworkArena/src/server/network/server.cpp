#include "server.h"

#include "../game/room.h"

#include "session.h"

#include <network/messages/payloads/player_identity_message.h>
#include <network/messages/payloads/world_state_snapshot_message.h>

#include <boost/asio/error.hpp>
#include <boost/asio/socket_base.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace
{
    // 서버 게임 시뮬레이션을 실행할 기본 Tick 간격
    constexpr std::chrono::milliseconds ServerTickInterval(16);

    // 서버 지연이나 디버깅 중단 이후 한 번에 과도한 시간만큼 시뮬레이션되는 것을 방지하기 위한 최대 delta 시간
    constexpr float MaxServerTickDeltaSeconds = 0.25f;
}

namespace cna::server
{
    Server::Server(boost::asio::io_context& ioContext, const std::uint16_t port) : acceptor_(ioContext), tickTimer_(ioContext)
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

        // 서버 시작 시 사용할 기본 Room 생성
        CreateDefaultRoom();

        // 생성에 실패한 경우
        if (defaultRoomId_ == 0)
        {
            // 에러 메시지 출력 후 반환
            std::cerr << "Failed to create default room" << '\n';
            return;
        }

        // 서버 게임 Tick 루프 시작
        StartTickLoop();

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
            // 접속된 소켓을 기반으로 세션을 생성하고 활성 세션 목록에 등록
            const std::shared_ptr<Session> session = 
                sessionManager_.CreateSession
                (
                    std::move(socket),
                    [this](const SessionId closedSessionId)
                    {
                        // 종료된 세션을 기반으로 하는 플레이어를 기본 Room에서 퇴장 처리
                        LeaveDefaultRoom(closedSessionId);
                    },
                    [this](const SessionId sessionId, const cna::network::PlayerInputPayload& input)
                    {
                        // 세션이 수신한 플레이어 입력을 서버로 전달
                        HandlePlayerInput(sessionId, input);
                    }
                );

            if (session)
            {
                // 생성된 세션을 기반으로 하는 플레이어를 기본 Room에 입장시키는 데 성공한 경우
                if (EnterDefaultRoom(session))
                {
                    // 클라이언트 연결 처리 시작
                    session->Start();
                }
                else
                {
                    // 기본 Room에 입장 실패한 경우 게임에 참여할 수 없으므로 종료 처리
                    session->Stop();
                }
            }
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

    void Server::CreateDefaultRoom()
    {
        // 기본 Room이 이미 생성되어 있는 경우 추가로 생성하지 않음
        if (defaultRoomId_ != 0)
        {
            return;
        }

        // RoomManager를 통해 기본 Room 생성
        const std::shared_ptr<Room> defaultRoom = roomManager_.CreateRoom();

        // 기본 Room 생성에 실패한 경우
        if (!defaultRoom)
        {
            return;
        }

        // 생성된 기본 Room의 ID 보관
        defaultRoomId_ = defaultRoom->GetRoomId();

        // 기본 Room 생성 결과 출력
        std::cout << "[Server] Default room created: roomId=" << defaultRoomId_ << '\n';
    }

    bool Server::EnterDefaultRoom(std::shared_ptr<Session> session)
    {
        // 유효하지 않은 세션인 경우 실패 처리
        if (!session)
        {
            return false;
        }

        // 기본 Room이 생성되지 않은 상태인 경우 실패 처리
        if (defaultRoomId_ == 0)
        {
            return false;
        }

        // 기본 Room 조회
        const std::shared_ptr<Room> defaultRoom = roomManager_.FindRoom(defaultRoomId_);

        // 기본 Room을 찾지 못한 경우 실패 처리
        if (!defaultRoom)
        {
            return false;
        }

        const SessionId sessionId = session->GetId();

        // 플레이어를 등록하고 서버가 발급한 플레이어 ID 획득
        const std::optional<cna::PlayerId> playerId = defaultRoom->Enter(session);

        // 플레이어 등록 또는 플레이어 ID 발급에 실패한 경우
        if (!playerId)
        {
            return false;
        }

        const cna::network::PlayerIdentityPayload identity
        {
            defaultRoom->GetRoomId(),
            *playerId
        };

        std::array<std::byte, cna::network::PlayerIdentityPayloadSize> identityPayload{};

        // 등록된 플레이어의 식별 정보를 클라이언트에 전송하기 위해 직렬화
        if (!cna::network::EncodePlayerIdentityPayload(identity, identityPayload))
        {
            // 직렬화에 실패한 경우 플레이어 퇴장 처리
            defaultRoom->Leave(sessionId);

            return false;
        }

        // 플레이어 식별 정보 전송
        if (!session->Send(cna::network::MessageType::PlayerIdentity, identityPayload))
        {
            // 통지에 실패한 경우 플레이어 퇴장 처리
            defaultRoom->Leave(sessionId);

            return false;
        }

        return true;
    }

    void Server::LeaveDefaultRoom(const SessionId sessionId)
    {
        // 기본 Room이 생성되지 않은 상태인 경우 실패 처리
        if (defaultRoomId_ == 0)
        {
            return;
        }

        // 기본 Room 조회
        const std::shared_ptr<Room> defaultRoom = roomManager_.FindRoom(defaultRoomId_);

        // 기본 Room을 찾지 못한 경우 실패 처리
        if (!defaultRoom)
        {
            return;
        }

        // 세션에 해당하는 플레이어를 기본 Room에서 등록 해제
        defaultRoom->Leave(sessionId);
    }

    void Server::HandlePlayerInput(SessionId sessionId, const cna::network::PlayerInputPayload& input)
    {
        // 기본 Room이 생성되지 않은 상태인 경우 입력을 처리하지 않음
        if (defaultRoomId_ == 0)
        {
            return;
        }

        // 기본 Room 조회
        const std::shared_ptr<Room> defaultRoom = roomManager_.FindRoom(defaultRoomId_);

        // 기본 Room을 찾지 못한 경우 입력을 처리하지 않음
        if (!defaultRoom)
        {
            return;
        }

        // 기본 Room에서 세션 ID에 해당하는 플레이어를 찾아 플레이어 입력 적용
        if (!defaultRoom->ApplyPlayerInput(sessionId, input))
        {
            std::cerr
                << "[Server] Failed to apply PlayerInput: roomId=" << defaultRoom->GetRoomId()
                << ", sessionId=" << sessionId
                << '\n';
        }
    }

    void Server::StartTickLoop()
    {
        // 서버가 종료된 상태이면 Tick 루프를 시작하지 않음
        if (stopped_)
        {
            return;
        }

        // 첫 Tick의 deltaSeconds 계산 기준 시간 저장
        lastTickTime_ = std::chrono::steady_clock::now();

        // 다음 서버 Tick 예약
        ScheduleNextTick();
    }

    void Server::ScheduleNextTick()
    {
        // 서버가 종료된 상태이면 다음 서버 Tick을 예약하지 않음
        if (stopped_)
        {
            return;
        }

        // 지정된 서버 Tick 간격 이후 타이머가 완료되도록 설정
        tickTimer_.expires_after(ServerTickInterval);

        // 타이머 완료 시 서버 Tick 처리 함수 호출
        tickTimer_.async_wait
        (
            [this](const boost::system::error_code& error)
            {
                HandleTick(error);
            }
        );
    }

    void Server::HandleTick(const boost::system::error_code& error)
    {
        // 서버가 종료된 경우 Tick을 처리하지 않음
        if (stopped_)
        {
            return;
        }

        // Stop() 단계에서 타이머가 취소된 경우 정상 종료 흐름으로 처리
        if (error == boost::asio::error::operation_aborted)
        {
            return;
        }

        // 타이머 처리 중 에러가 발생한 경우 로그 메시지 출력 후 Tick 루프 중단
        if (error)
        {
            std::cerr << "[Server] Tick timer failed: " << error.message() << '\n';

            return;
        }

        const auto currentTickTime = std::chrono::steady_clock::now();

        // 이전 Tick 이후 실제로 흐른 시간 계산
        float deltaSeconds = std::chrono::duration<float>(currentTickTime - lastTickTime_).count();

        // 서버 Tick이 실행된 시간 갱신
        lastTickTime_ = currentTickTime;

        // 과도한 지연이 발생한 경우 한 Tick에서 처리할 최대 시간을 제한
        if (deltaSeconds > MaxServerTickDeltaSeconds)
        {
            deltaSeconds = MaxServerTickDeltaSeconds;
        }

        // 기본 Room의 게임 상태 갱신
        TickDefaultRoom(deltaSeconds);

        // 다음 Tick 예약
        ScheduleNextTick();
    }

    void Server::TickDefaultRoom(const float deltaSeconds)
    {
        // 기본 Room이 생성되지 않은 상태인 경우 처리하지 않음
        if (defaultRoomId_ == 0)
        {
            return;
        }

        // 기본 Room 조회
        const std::shared_ptr<Room> defaultRoom = roomManager_.FindRoom(defaultRoomId_);

        // 기본 Room을 찾지 못한 경우 처리하지 않음
        if (!defaultRoom)
        {
            return;
        }

        // 기본 Room의 게임 상태 갱신
        defaultRoom->Tick(deltaSeconds);

        // 갱신된 Room의 현재 상태 스냅샷 생성
        const cna::network::WorldStateSnapshot snapshot = defaultRoom->CaptureSnapshot();

        std::vector<std::byte> payload;

        // 스냅샷 Payload 직렬화에 실패한 경우 현재 Tick의 Broadcast 생략
        if (!cna::network::EncodeWorldStateSnapshotPayload(snapshot, payload))
        {
            std::cerr
                << "[Server] Failed to encode WorldStateSnapshot: roomId="
                << defaultRoom->GetRoomId()
                << '\n';

            return;
        }

        // Room에 등록된 모든 활성 세션에 최신 월드 상태 전송
        defaultRoom->Broadcast(cna::network::MessageType::WorldStateSnapshot, payload);
    }

    void Server::Stop()
    {
        // 이미 서버 종료 처리된 경우
        if (stopped_)
        {
            return;
        }

        stopped_ = true;

        // 예약된 서버 Tick 타이머를 취소
        tickTimer_.cancel();

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
#include "network_client.h"

#include <network/messages/core/message_codec.h>
#include <network/messages/core/message_header.h>
#include <network/messages/payloads/player_identity_message.h>
#include <network/messages/payloads/world_state_snapshot_message.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>

#include <boost/system/errc.hpp>

#include <algorithm>
#include <iostream>
#include <span>
#include <string>
#include <utility>

namespace cna::client
{
    NetworkClient::NetworkClient(boost::asio::io_context& ioContext)
        : resolver_(ioContext), socket_(ioContext)
    {
    }

    NetworkClient::~NetworkClient()
    {
        // 연결 상태를 Disconnect로 설정하고 자원 및 콜백 초기화
        ResetConnection();
    }

    bool NetworkClient::Connect
    (
        const std::string_view host,
        const std::uint16_t port,
        ConnectedCallback onConnected,
        ConnectionFailedCallback onConnectionFailed,
        DisconnectedCallback onDisconnected
    )
    {
        // 연결 해제 상태가 아닌 경우 중복 연결 요청 거부
        if (connectionState_ != ConnectionState::Disconnected)
        {
            return false;
        }

        // 올바르지 않은 접속 대상인 경우 연결 요청 거부
        if (host.empty() || port == 0)
        {
            return false;
        }

        // 비동기 콜백에서 수명을 유지할 수 있도록 shared_ptr 소유 여부 확인
        const std::shared_ptr<NetworkClient> self = weak_from_this().lock();

        // 이미 클라이언트 객체가 소멸된 상태인 경우
        if (!self)
        {
            return false;
        }

        // 이번 연결 시도를 이전에 취소된 작업과 구분하기 위해 연결 세대 값 증가
        const std::uint64_t connectionGeneration = ++connectionGeneration_;

        // 클라이언트 연결 상태 변경 (Disconnected -> Resolving)
        connectionState_ = ConnectionState::Resolving;

        // 연결 성공 및 실패 시 호출할 콜백 설정
        onConnected_ = std::move(onConnected);
        onConnectionFailed_ = std::move(onConnectionFailed);

        // 연결 종료 시 호출할 콜백 설정
        onDisconnected_ = std::move(onDisconnected);

        // TCP 수신 데이터 누적 버퍼 초기화
        accumulatedBuffer_.clear();

        // 새로운 연결에 이전 연결의 식별 정보가 남지 않도록 초기화
        playerIdentity_.reset();

        // DNS 주소 해석 작업을 비동기 요청
        resolver_.async_resolve
        (
            host,
            std::to_string(port),
            [self, connectionGeneration]
            (
                const boost::system::error_code& error,
                Tcp::resolver::results_type results
            )
            {
                self->HandleResolve(error, std::move(results), connectionGeneration);
            }
        );

        return true;
    }

    bool NetworkClient::Disconnect()
    {
        // 이미 종료된 상태이면 중복 종료 요청 거부
        if (connectionState_ == ConnectionState::Disconnected)
        {
            return false;
        }

        // 연결 세대 값 증가를 통해 이전 비동기 작업의 콜백 무효화
        ++connectionGeneration_;

        // 연결 상태를 Disconnect로 설정하고 자원 및 콜백 초기화
        ResetConnection();

        return true;
    }

    NetworkClient::ConnectionState NetworkClient::GetConnectionState() const noexcept
    {
        return connectionState_;
    }

    bool NetworkClient::IsConnected() const noexcept
    {
        return connectionState_ == ConnectionState::Connected;
    }

    std::optional<cna::RoomId> NetworkClient::GetRoomId() const noexcept
    {
        if (!playerIdentity_)
        {
            return std::nullopt;
        }

        return playerIdentity_->roomId;
    }

    std::optional<cna::PlayerId> NetworkClient::GetPlayerId() const noexcept
    {
        if (!playerIdentity_)
        {
            return std::nullopt;
        }

        return playerIdentity_->playerId;
    }

    bool NetworkClient::IsCurrentOperation(std::uint64_t connectionGeneration, ConnectionState expectedState) const noexcept
    {
        return (connectionGeneration == connectionGeneration_) && (connectionState_ == expectedState);
    }

    bool NetworkClient::IsCurrentOperation(std::uint64_t connectionGeneration) const noexcept
    {
        return (connectionGeneration == connectionGeneration_);
    }

    void NetworkClient::HandleResolve
    (
        const boost::system::error_code& error,
        Tcp::resolver::results_type results,
        const std::uint64_t connectionGeneration
    )
    {
        // 취소된 이전 연결 시도의 콜백이거나 주소 해석 단계가 아닌 경우 실패 처리
        if (!IsCurrentOperation(connectionGeneration, ConnectionState::Resolving))
        {
            return;
        }

        // 주소 해석에 실패한 경우 연결 실패 처리
        if (error)
        {
            CompleteConnectionFailure(error, connectionGeneration);

            return;
        }

        // 클라이언트 연결 상태 변경 (Resolving -> Connecting)
        connectionState_ = ConnectionState::Connecting;

        // 비동기 연결이 완료될 때까지 객체 수명 유지
        const std::shared_ptr<NetworkClient> self = shared_from_this();

        // 해석된 엔드포인트를 순서대로 시도해 연결
        boost::asio::async_connect
        (
            socket_,
            results,
            [self, connectionGeneration]
            (
                const boost::system::error_code& connectError,
                const Tcp::endpoint& endpoint
            )
            {
                self->HandleConnect(connectError, endpoint, connectionGeneration);
            }
        );
    }

    void NetworkClient::HandleConnect
    (
        const boost::system::error_code& error,
        const Tcp::endpoint& endpoint,
        const std::uint64_t connectionGeneration
    )
    {
        // 취소된 이전 연결 시도의 콜백이거나 연결 시도 단계가 아닌 경우 실패 처리
        if (!IsCurrentOperation(connectionGeneration, ConnectionState::Connecting))
        {
            return;
        }

        // 서버 연결에 실패한 경우 실패 상태 정리
        if (error)
        {
            CompleteConnectionFailure(error, connectionGeneration);

            return;
        }

        // 클라이언트 연결 상태 변경 (Connecting -> Connected)
        connectionState_ = ConnectionState::Connected;

        // 연결 성공 콜백 설정
        ConnectedCallback connectedCallback = std::move(onConnected_);

        // 일회성 콜백을 위한 콜백 변수 초기화
        onConnected_ = {};
        onConnectionFailed_ = {};

        // 연결 성공 콜백 호출
        if (connectedCallback)
        {
            connectedCallback(endpoint);
        }

        // 비동기 수신 작업 등록
        if (IsCurrentOperation(connectionGeneration, ConnectionState::Connected))
        {
            ReadNext(connectionGeneration);
        }
    }

    void NetworkClient::ReadNext(const std::uint64_t connectionGeneration)
    {
        // 소켓이 열려 있는지 확인
        if (!socket_.is_open())
        {
            return;
        }

        // 현재 연결에 대해서만 새로운 수신 작업 등록
        if (!IsCurrentOperation(connectionGeneration, ConnectionState::Connected))
        {
            return;
        }

        // 비동기 수신이 완료될 때까지 객체 수명 유지
        const std::shared_ptr<NetworkClient> self = shared_from_this();

        // 서버가 전송하는 데이터를 비동기적으로 수신
        socket_.async_read_some
        (
            boost::asio::buffer(receiveBuffer_),
            [self, connectionGeneration]
            (
                const boost::system::error_code& error,
                const std::size_t bytesTransferred
                )
            {
                // 데이터 수신 결과 처리
                self->HandleRead(error, bytesTransferred, connectionGeneration);
            }
        );
    }

    void NetworkClient::HandleRead
    (
        const boost::system::error_code& error,
        const std::size_t bytesTransferred,
        const std::uint64_t connectionGeneration
    )
    {
        // 취소되거나 종료된 이전 연결의 수신 콜백인지 확인
        if (!IsCurrentOperation(connectionGeneration, ConnectionState::Connected))
        {
            return;
        }

        // 이번 비동기 읽기에서 수신한 데이터가 존재하는 경우
        if (bytesTransferred > 0)
        {
            // 수신 데이터를 누적 버퍼에 추가하고 메시지 단위로 처리
            if (!ProcessReceivedData(bytesTransferred, connectionGeneration))
            {
                return;
            }
        }

        // 데이터를 정상적으로 수신한 경우
        if (!error)
        {
            // 다음 데이터 수신 작업 등록
            ReadNext(connectionGeneration);

            return;
        }

        // 연결 종료 처리
        CompleteDisconnection(error, connectionGeneration);
    }

    bool NetworkClient::ProcessReceivedData
    (
        const std::size_t bytesTransferred,
        const std::uint64_t connectionGeneration
    )
    {
        // 이번 비동기 읽기에서 수신한 데이터를 누적 버퍼에 추가
        accumulatedBuffer_.insert
        (
            accumulatedBuffer_.end(),
            receiveBuffer_.begin(),
            receiveBuffer_.begin() + bytesTransferred
        );

        // 누적 버퍼에서 완전한 메시지 분리
        return ProcessMessages(connectionGeneration);
    }

    bool NetworkClient::ProcessMessages(const std::uint64_t connectionGeneration)
    {
        // 누적 버퍼에 메시지 헤더 크기보다 큰 데이터가 존재하는 경우 반복
        while (accumulatedBuffer_.size() >= cna::network::MessageHeaderSize)
        {
            // 누적 버퍼의 첫 번째 메시지 헤더 복원
            cna::network::MessageHeader header;

            const std::span<const std::byte> accumulatedData
            (
                accumulatedBuffer_.data(),
                accumulatedBuffer_.size()
            );

            // 메시지 헤더 역직렬화에 실패한 경우 다음 수신 대기
            if (!cna::network::DecodeMessageHeader(accumulatedData, header))
            {
                return true;
            }

            // 메시지 크기 범위 검사에 실패한 경우
            if (header.size < cna::network::MessageHeaderSize || header.size > cna::network::MaxMessageSize)
            {
                // 연결 종료 처리
                CompleteDisconnection
                (
                    boost::asio::error::make_error_code(boost::asio::error::message_size),
                    connectionGeneration
                );

                return false;
            }

            // 전체 메시지가 아직 도착하지 않은 경우 다음 수신 대기
            if (accumulatedBuffer_.size() < header.size)
            {
                return true;
            }

            // 메시지 헤더 뒤의 Payload 영역 생성
            const std::size_t payloadSize = header.size - cna::network::MessageHeaderSize;

            const std::span<const std::byte> payload
            (
                accumulatedBuffer_.data() + cna::network::MessageHeaderSize,
                payloadSize
            );

            // 메시지 타입을 확인해 전용 핸들러 함수를 호출
            if (!DispatchMessage(header, payload))
            {
                CompleteDisconnection
                (
                    boost::system::errc::make_error_code(boost::system::errc::protocol_error),
                    connectionGeneration
                );

                return false;
            }

            // 처리한 메시지를 누적 버퍼에서 제거
            accumulatedBuffer_.erase
            (
                accumulatedBuffer_.begin(),
                accumulatedBuffer_.begin() + header.size
            );
        }

        return true;
    }

    bool NetworkClient::DispatchMessage(const cna::network::MessageHeader& header, std::span<const std::byte> payload)
    {
        // 메시지 타입에 따라 전용 핸들러 함수 호출
        switch (header.type)
        {
        case cna::network::MessageType::TestResponse:
            return HandleTestResponse(payload);

        case cna::network::MessageType::PlayerIdentity:
            return HandlePlayerIdentity(payload);

        case cna::network::MessageType::WorldStateSnapshot:
            return HandleWorldStateSnapshot(payload);

        case cna::network::MessageType::Unknown:
        case cna::network::MessageType::TestRequest:
        case cna::network::MessageType::PlayerInput:
        default:
            std::cerr
                << "[NetworkClient] Unsupported message type"
                << ": " << cna::network::MessageTypeValue(header.type)
                << '\n';

            return false;
        }
    }

    bool NetworkClient::HandleTestResponse(std::span<const std::byte> payload)
    {
        // 테스트 응답 메시지의 전달 경로 검증용 메시지 출력
        std::cout
            << "[NetworkClient] TestResponse dispatched: payload="
            << payload.size()
            << '\n';

        return true;
    }

    bool NetworkClient::HandlePlayerIdentity(std::span<const std::byte> payload)
    {
        cna::network::PlayerIdentityPayload identity;

        // PlayerIdentity Payload 역직렬화 및 식별자 유효성 검사
        if (!cna::network::DecodePlayerIdentityPayload(payload, identity))
        {
            std::cerr
                << "[NetworkClient] Invalid PlayerIdentity payload"
                << ": payload=" << payload.size()
                << '\n';

            return false;
        }

        // 검증이 완료된 현재 연결의 식별 정보 저장
        playerIdentity_ = identity;

        std::cout
            << "[NetworkClient] PlayerIdentity received"
            << ": roomId=" << identity.roomId
            << ", playerId=" << identity.playerId
            << '\n';

        return true;
    }

    bool NetworkClient::HandleWorldStateSnapshot(std::span<const std::byte> payload)
    {
        cna::network::WorldStateSnapshot snapshot;

        // WorldStateSnapshot Payload 역직렬화 및 데이터 유효성 검사
        if (!cna::network::DecodeWorldStateSnapshotPayload(payload, snapshot))
        {
            std::cerr
                << "[NetworkClient] Invalid WorldStateSnapshot payload"
                << ": payload=" << payload.size()
                << '\n';

            return false;
        }

        // 플레이어 식별 정보를 받기 전에 월드 스냅샷이 도착한 경우
        if (!playerIdentity_)
        {
            std::cerr
                << "[NetworkClient] WorldStateSnapshot received before PlayerIdentity"
                << ": roomId=" << snapshot.roomId
                << '\n';

            return false;
        }

        // 현재 플레이어가 입장한 Room과 스냅샷의 Room이 다른 경우
        if (snapshot.roomId != playerIdentity_->roomId)
        {
            std::cerr
                << "[NetworkClient] WorldStateSnapshot room mismatch"
                << ": expectedRoomId=" << playerIdentity_->roomId
                << ", receivedRoomId=" << snapshot.roomId
                << '\n';

            return false;
        }

        // 스냅샷에서 현재 클라이언트에 할당된 로컬 플레이어 검색
        const auto localPlayerIterator = std::find_if
        (
            snapshot.players.cbegin(),
            snapshot.players.cend(),
            [this](const cna::network::PlayerStateSnapshot& player)
            {
                return player.playerId == playerIdentity_->playerId;
            }
        );

        // 현재 클라이언트의 플레이어가 스냅샷에 포함되지 않은 경우
        if (localPlayerIterator == snapshot.players.cend())
        {
            std::cerr
                << "[NetworkClient] Local player missing from WorldStateSnapshot"
                << ": roomId=" << snapshot.roomId
                << ", playerId=" << playerIdentity_->playerId
                << '\n';

            return false;
        }

        const cna::network::PlayerStateSnapshot& localPlayer = *localPlayerIterator;

        // 현재 클라이언트의 플레이어 월드 상태 출력
        std::cout
            << "[NetworkClient] WorldStateSnapshot received"
            << ": roomId=" << snapshot.roomId
            << ", playerCount=" << snapshot.players.size()
            << ", localPlayerId=" << localPlayer.playerId
            << ", position=("
            << localPlayer.positionX << ", "
            << localPlayer.positionY << ", "
            << localPlayer.positionZ << ')'
            << ", velocity=("
            << localPlayer.velocityX << ", "
            << localPlayer.velocityY << ", "
            << localPlayer.velocityZ << ')'
            << '\n';

        return true;
    }

    void NetworkClient::CompleteConnectionFailure
    (
        const boost::system::error_code& error,
        const std::uint64_t connectionGeneration
    )
    {
        // 현재 연결 시도의 실패만 처리
        if (!IsCurrentOperation(connectionGeneration))
        {
            return;
        }

        // 클라이언트 연결 상태 변경 (-> Disconnected)
        connectionState_ = ConnectionState::Disconnected;

        // 소켓 종료
        CloseSocket();

        // 실패한 연결 시도의 식별 정보 초기화
        playerIdentity_.reset();

        // 연결 실패 콜백 보관
        ConnectionFailedCallback connectionFailedCallback = std::move(onConnectionFailed_);

        // 일회성 콜백을 위한 콜백 변수 초기화
        onConnectionFailed_ = {};
        onConnected_ = {};
        onDisconnected_ = {};

        // 연결 실패 콜백 호출
        if (connectionFailedCallback)
        {
            connectionFailedCallback(error);
        }
    }

    void NetworkClient::CompleteDisconnection
    (
        const boost::system::error_code& error,
        const std::uint64_t connectionGeneration
    )
    {
        // 현재 연결의 종료만 한 번 처리
        if (!IsCurrentOperation(connectionGeneration, ConnectionState::Connected))
        {
            return;
        }

        // 클라이언트 연결 상태 변경 (Connected -> Disconnected)
        connectionState_ = ConnectionState::Disconnected;

        // 소켓 종료
        CloseSocket();

        // 종료된 연결에서 남은 수신 데이터 제거
        accumulatedBuffer_.clear();

        // 종료된 연결에 할당됐던 식별 정보 초기화
        playerIdentity_.reset();

        // 연결 종료 콜백 보관
        DisconnectedCallback disconnectedCallback = std::move(onDisconnected_);

        // 일회성 콜백을 위한 콜백 변수 초기화
        onDisconnected_ = {};

        // 연결 종료 콜백 호출
        if (disconnectedCallback)
        {
            disconnectedCallback(error);
        }
    }

    void NetworkClient::ResetConnection()
    {
        // 클라이언트 연결 상태 변경(-> Disconnected)
        connectionState_ = ConnectionState::Disconnected;

        // DNS 주소 해석 작업 취소
        resolver_.cancel();

        // 소켓 종료
        CloseSocket();

        // TCP 수신 데이터 누적 버퍼 초기화
        accumulatedBuffer_.clear();

        // 현재 연결에 할당된 식별 정보 초기화
        playerIdentity_.reset();

        // 콜백 변수 초기화
        onConnected_ = {};
        onConnectionFailed_ = {};
        onDisconnected_ = {};
    }

    void NetworkClient::CloseSocket() noexcept
    {
        // 소켓이 닫혀 있는 경우 무시
        if (!socket_.is_open())
        {
            return;
        }

        boost::system::error_code ignoredError;

        // 소켓에서 대기 중인 모든 비동기 작업 취소
        socket_.cancel(ignoredError);
        // 서버와의 연결 종료
        socket_.shutdown(Tcp::socket::shutdown_both, ignoredError);
        // 소켓 종료
        socket_.close(ignoredError);
    }
}
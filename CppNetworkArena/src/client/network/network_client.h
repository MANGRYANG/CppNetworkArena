#pragma once

#include <network/messages/core/message_header.h>
#include <network/messages/payloads/player_identity_message.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cna::client
{
    // 서버와의 비동기 TCP 연결을 관리하는 클라이언트 네트워크 클래스
    class NetworkClient final : public std::enable_shared_from_this<NetworkClient>
    {
    public:
        // 클라이언트 연결 상태를 정의하는 열거형
        enum class ConnectionState
        {
            Disconnected,
            Resolving,
            Connecting,
            Connected
        };

        // 연결 성공 시 호출할 콜백 시그니처
        using ConnectedCallback = std::function<void(const boost::asio::ip::tcp::endpoint&)>;
        // 연결 실패 시 호출할 콜백 시그니처
        using ConnectionFailedCallback = std::function<void(const boost::system::error_code&)>;

        // 연결된 서버와의 통신이 예기치 않게 종료된 경우 호출할 콜백 시그니처
        using DisconnectedCallback = std::function<void(const boost::system::error_code&)>;

        explicit NetworkClient(boost::asio::io_context& ioContext);
        ~NetworkClient();

        // 복사 생성자 및 복사 대입 연산자 삭제
        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        NetworkClient(NetworkClient&&) = delete;
        NetworkClient& operator=(NetworkClient&&) = delete;

        // 클라이언트를 서버에 비동기 연결하는 함수
        bool Connect
        (
            std::string_view host,
            std::uint16_t port,
            ConnectedCallback onConnected,
            ConnectionFailedCallback onConnectionFailed,
            DisconnectedCallback onDisconnected
        );

        // 진행 중인 연결 작업을 취소하거나 연결된 소켓을 종료하는 함수
        bool Disconnect();

        // 현재 클라이언트 연결 상태를 반환하는 함수
        ConnectionState GetConnectionState() const noexcept;

        // 서버 연결 완료 여부를 반환하는 함수
        bool IsConnected() const noexcept;

        // 서버가 현재 연결에 할당한 Room ID를 반환하는 함수
        std::optional<cna::RoomId> GetRoomId() const noexcept;

        // 서버가 현재 연결에 할당한 Player ID를 반환하는 함수
        std::optional<cna::PlayerId> GetPlayerId() const noexcept;

    private:
        using Tcp = boost::asio::ip::tcp;

        // 한 번의 비동기 수신에 사용할 임시 버퍼 크기
        static constexpr std::size_t ReceiveBufferSize = 1024;

        // 현재 연결 세대 및 클라이언트 연결 상태가 일치하는지 검증하는 내부 헬퍼
        bool IsCurrentOperation(std::uint64_t connectionGeneration, ConnectionState expectedState) const noexcept;

        // 현재 연결 세대가 일치하는지 검증하는 내부 헬퍼
        bool IsCurrentOperation(std::uint64_t connectionGeneration) const noexcept;

        // 비동기 호스트 해석 결과 처리
        void HandleResolve
        (
            const boost::system::error_code& error,
            Tcp::resolver::results_type results,
            std::uint64_t connectionGeneration
        );

        // 비동기 서버 연결 결과 처리
        void HandleConnect
        (
            const boost::system::error_code& error,
            const Tcp::endpoint& endpoint,
            std::uint64_t connectionGeneration
        );

        // 다음 비동기 수신 작업을 등록하는 함수
        void ReadNext(std::uint64_t connectionGeneration);

        // 비동기 수신 결과 처리 함수
        void HandleRead
        (
            const boost::system::error_code& error,
            std::size_t bytesTransferred,
            std::uint64_t connectionGeneration
        );

        // 새로 수신한 데이터를 누적 버퍼에 추가하고 메시지 단위로 처리하는 함수
        bool ProcessReceivedData
        (
            std::size_t bytesTransferred,
            std::uint64_t connectionGeneration
        );

        // 누적 버퍼에서 완전한 메시지를 반복적으로 분리하는 함수
        bool ProcessMessages(std::uint64_t connectionGeneration);

        // 메시지 타입에 따라 전용 핸들러 함수를 호출하는 디스패치 함수
        bool DispatchMessage(const cna::network::MessageHeader& header, std::span<const std::byte> payload);

        // 테스트 요청 메시지 전용 핸들러 함수
        bool HandleTestResponse(std::span<const std::byte> payload);

        // 플레이어 식별 정보 메시지 전용 핸들러 함수
        bool HandlePlayerIdentity(std::span<const std::byte> payload);

        // 월드 상태 스냅샷 메시지 전용 핸들러 함수
        bool HandleWorldStateSnapshot(std::span<const std::byte> payload);

        // 연결 실패 상태를 정리한 뒤 실패 콜백 호출
        void CompleteConnectionFailure
        (
            const boost::system::error_code& error,
            std::uint64_t connectionGeneration
        );

        // 연결 종료 상태를 정리한 뒤 종료 콜백 호출
        void CompleteDisconnection
        (
            const boost::system::error_code& error,
            std::uint64_t connectionGeneration
        );

        // 연결 상태와 자원 및 콜백을 초기화하는 함수
        void ResetConnection();

        // 소켓에 등록된 작업을 취소하고 소켓 종료
        void CloseSocket() noexcept;

        // 서버 주소 해석을 담당하는 Resolver
        Tcp::resolver resolver_;

        // 서버와 통신할 TCP 소켓
        Tcp::socket socket_;

        // 현재 클라이언트 연결 상태
        ConnectionState connectionState_ = ConnectionState::Disconnected;

        // 취소된 이전 비동기 작업의 콜백을 구분하기 위한 세대 값
        std::uint64_t connectionGeneration_ = 0;

        // 서버 연결 성공 시 호출할 콜백
        ConnectedCallback onConnected_;

        // 서버 연결 실패 시 호출할 콜백
        ConnectionFailedCallback onConnectionFailed_;

        // 연결된 서버와의 통신이 예기치 않게 종료된 경우 호출할 콜백
        DisconnectedCallback onDisconnected_;

        // 서버에서 받은 데이터를 임시로 저장하는 수신 버퍼
        std::array<std::byte, ReceiveBufferSize> receiveBuffer_{};

        // 여러 번 나뉘거나 합쳐져 수신된 TCP 데이터를 저장하는 누적 버퍼
        std::vector<std::byte> accumulatedBuffer_;

        // 서버가 현재 연결에 할당한 Room 및 Player 식별 정보
        std::optional<cna::network::PlayerIdentityPayload> playerIdentity_;
    };
}
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

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
            ConnectionFailedCallback onConnectionFailed
        );

        // 진행 중인 연결 작업을 취소하거나 연결된 소켓을 종료하는 함수
        bool Disconnect();

        // 현재 클라이언트 연결 상태를 반환하는 함수
        ConnectionState GetConnectionState() const noexcept;

        // 서버 연결 완료 여부를 반환하는 함수
        bool IsConnected() const noexcept;

    private:
        using Tcp = boost::asio::ip::tcp;

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

        // 연결 실패 상태를 정리한 뒤 실패 콜백 호출
        void CompleteConnectionFailure
        (
            const boost::system::error_code& error,
            std::uint64_t connectionGeneration
        );

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
    };
}
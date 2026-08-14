#include "network_client.h"

#include <boost/asio/connect.hpp>

#include <utility>

namespace cna::client
{
    NetworkClient::NetworkClient(boost::asio::io_context& ioContext)
        : resolver_(ioContext), socket_(ioContext)
    {
    }

    NetworkClient::~NetworkClient()
    {
        // 클라이언트 연결 상태 변경(-> Disconnected)
        connectionState_ = ConnectionState::Disconnected;

        // 콜백 변수 초기화
        onConnected_ = {};
        onConnectionFailed_ = {};

        // DNS 주소 해석 작업 취소
        resolver_.cancel();

        // 소켓 종료
        CloseSocket();
    }

    bool NetworkClient::Connect
    (
        std::string_view host,
        std::uint16_t port,
        ConnectedCallback onConnected,
        ConnectionFailedCallback onConnectionFailed
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

        // 클라이언트 연결 상태 변경(-> Disconnected)
        connectionState_ = ConnectionState::Disconnected;

        // 콜백 변수 초기화
        onConnected_ = {};
        onConnectionFailed_ = {};

        // DNS 주소 해석 작업 취소
        resolver_.cancel();

        // 소켓 종료
        CloseSocket();

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

    void NetworkClient::HandleResolve
    (
        const boost::system::error_code& error,
        Tcp::resolver::results_type results,
        const std::uint64_t connectionGeneration
    )
    {
        // 취소된 이전 연결 시도의 콜백이거나 주소 해석 단계가 아닌 경우 실패 처리
        if ((connectionGeneration != connectionGeneration_) || (connectionState_ != ConnectionState::Resolving))
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
        if (connectionGeneration != connectionGeneration_ || connectionState_ != ConnectionState::Connecting)
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
    }

    void NetworkClient::CompleteConnectionFailure
    (
        const boost::system::error_code& error,
        const std::uint64_t connectionGeneration
    )
    {
        // 현재 연결 시도의 실패만 처리
        if (connectionGeneration != connectionGeneration_)
        {
            return;
        }

        // 클라이언트 연결 상태 변경 (-> Disconnected)
        connectionState_ = ConnectionState::Disconnected;

        // 소켓 종료
        CloseSocket();

        // 연결 실패 콜백 설정
        ConnectionFailedCallback connectionFailedCallback = std::move(onConnectionFailed_);

        // 일회성 콜백을 위한 콜백 변수 초기화
        onConnectionFailed_ = {};
        onConnected_ = {};

        // 연결 실패 콜백 호출
        if (connectionFailedCallback)
        {
            connectionFailedCallback(error);
        }
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
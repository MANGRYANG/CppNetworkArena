#include "session.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>

#include <iostream>
#include <utility>

namespace cna::server
{
    Session::Session(Tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void Session::Start()
    {
        // 연결된 클라이언트의 엔드포인트 정보 가져오기
        CacheRemoteEndpoint();

        // 클라이언트 연결 정보 출력
        std::cout << "Client connected: " << remoteEndpoint_ << '\n';

        // 첫 번째 비동기 데이터 수신 작업 등록
        ReadNext();
    }

    void Session::CacheRemoteEndpoint()
    {
        // 클라이언트 엔드포인트 정보 확인
        boost::system::error_code endpointError;
        const Tcp::endpoint remoteEndpoint = socket_.remote_endpoint(endpointError);

        // 엔드포인트 확인 중 에러가 발생한 경우
        if (endpointError)
        {
            // 에러 메시지 출력
            std::cerr << "Failed to read remote endpoint: " << endpointError.message() << '\n';

            return;
        }

        // 클라이언트 IP 주소를 문자열로 변환
        const std::string remoteAddress = remoteEndpoint.address().to_string();
        // 클라이언트 port 정보를 문자열로 변환
        const std::string remotePort = std::to_string(remoteEndpoint.port());

        // 엔드포인트 정보 문자열 조합
        remoteEndpoint_ = remoteAddress + ':' + remotePort;
    }

    void Session::ReadNext()
    {
        // 비동기 작업이 완료될 때까지 Session 객체의 수명 유지
        const std::shared_ptr<Session> self = shared_from_this();

        // 클라이언트가 전송하는 데이터를 비동기적으로 수신
        socket_.async_read_some
        (
            boost::asio::buffer(receiveBuffer_),
            [self](const boost::system::error_code& error, const std::size_t bytesTransferred)
            {
                // 데이터 수신 결과 처리
                self->HandleRead(error, bytesTransferred);
            }
        );
    }

    void Session::HandleRead(const boost::system::error_code& error, const std::size_t bytesTransferred)
    {
        // 데이터를 정상적으로 수신한 경우
        if (!error)
        {
            // 수신한 데이터 크기와 클라이언트 정보 출력
            std::cout << "Received " << bytesTransferred
                << " bytes from " << remoteEndpoint_ << ": ";

            // 수신 버퍼에서 실제 수신한 크기만큼 출력
            std::cout.write
            (
                receiveBuffer_.data(),
                static_cast<std::streamsize>(bytesTransferred)
            );

            std::cout << '\n';

            // 다음 데이터 수신 작업 등록
            ReadNext();

            return;
        }

        // 클라이언트가 정상적으로 연결을 종료한 경우
        if (error == boost::asio::error::eof)
        {
            // 클라이언트 연결 종료 정보 출력
            std::cout << "Client disconnected: " << remoteEndpoint_ << '\n';
        }
        // 클라이언트 연결이 강제로 종료된 경우
        else if (error == boost::asio::error::connection_reset)
        {
            // 클라이언트 연결 종료 정보 출력
            std::cout << "Client disconnected: " << remoteEndpoint_ << '\n';
        }
        // 서버에서 비동기 작업을 취소한 경우가 아닌 경우
        else if (error != boost::asio::error::operation_aborted)
        {
            // 데이터 수신 에러 메시지 출력
            std::cerr << "Receive failed from " << remoteEndpoint_
                << ": " << error.message() << '\n';
        }

        // 클라이언트 소켓 종료
        Close();
    }

    void Session::Close()
    {
        // 클라이언트 소켓이 이미 닫혀 있는 경우
        if (!socket_.is_open())
        {
            return;
        }

        // 클라이언트와의 연결 종료
        boost::system::error_code shutdownError;
        socket_.shutdown(Tcp::socket::shutdown_both, shutdownError);

        // 소켓 종료
        boost::system::error_code closeError;
        socket_.close(closeError);

        // 소켓 종료 중 에러가 발생한 경우
        if (closeError)
        {
            // 에러 메시지 출력
            std::cerr << "Failed to close client socket: " << closeError.message() << '\n';
        }
    }
}
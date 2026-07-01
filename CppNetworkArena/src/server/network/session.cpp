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
        // 이번 비동기 읽기에서 수신한 데이터가 존재하는 경우
        if (bytesTransferred > 0)
        {
            // 수신 데이터를 누적 버퍼에 추가하고 메시지 단위로 처리
            if (!ProcessReceivedData(bytesTransferred))
            {
                return;
            }
        }

        // 데이터를 정상적으로 수신한 경우
        if (!error)
        {
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

    bool Session::ProcessReceivedData(const std::size_t bytesTransferred)
    {
        // 이번 비동기 읽기에서 수신한 데이터를 누적 버퍼에 추가
        accumulatedBuffer_.insert
        (
            accumulatedBuffer_.end(),
            receiveBuffer_.begin(),
            receiveBuffer_.begin() + bytesTransferred
        );

        // 누적 버퍼에서 완전한 메시지 분리
        return ProcessMessages();
    }

    bool Session::ProcessMessages()
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
                std::cerr << "Invalid message size from " << remoteEndpoint_
                    << ": " << header.size << '\n';

                // 잘못된 메시지를 전송한 클라이언트 연결 종료
                Close();

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

            // 완전한 메시지 처리
            HandleMessage(header, payload);

            // 처리한 메시지를 누적 버퍼에서 제거
            accumulatedBuffer_.erase
            (
                accumulatedBuffer_.begin(),
                accumulatedBuffer_.begin() + header.size
            );
        }

        return true;
    }

    void Session::HandleMessage(const cna::network::MessageHeader& header, const std::span<const std::byte> payload)
    {
        // 헤더 및 Payload 크기 출력
        std::cout << "Message received from " << remoteEndpoint_
            << " | type=" << header.type
            << " | size=" << header.size
            << " | payload=" << payload.size()
            << " bytes\n";
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
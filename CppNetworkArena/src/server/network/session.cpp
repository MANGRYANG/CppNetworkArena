#include "session.h"

#include <network/message_codec.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/write.hpp>

#include <iostream>
#include <utility>

namespace cna::server
{
    Session::Session(const SessionId id, Tcp::socket socket, SessionClosedCallback onClosed)
        : id_(id), socket_(std::move(socket)), onClosed_(std::move(onClosed))
    {
    }

    void Session::Start()
    {
        // 이미 종료된 세션인지 확인
        if (closed_)
        {
            return;
        }

        // 연결된 클라이언트의 엔드포인트 정보 가져오기
        CacheRemoteEndpoint();

        // 클라이언트 연결 정보 출력
        std::cout
            << "Client connected: " << remoteEndpoint_
            << ", sessionId=" << id_ << '\n';

        // 첫 번째 비동기 데이터 수신 작업 등록
        ReadNext();
    }

    void Session::Stop()
    {
        // 외부의 세션 종료 요청에 따라 종료 처리
        Close();
    }

    SessionId Session::GetId() const noexcept
    {
        return id_;
    }

    bool Session::Send(cna::network::MessageType type, std::span<const std::byte> payload)
    {
        // 이미 종료된 세션인지 확인
        if (closed_)
        {
            return false;
        }

        // 클라이언트 소켓이 이미 닫혀 있는 경우
        if (!socket_.is_open())
        {
            return false;
        }

        // 서버가 클라이언트로 전송할 수 있는 메시지 타입인지 확인
        switch (type)
        {
        case cna::network::MessageType::TestResponse:
            break;
        case cna::network::MessageType::Unknown:
        case cna::network::MessageType::TestRequest:
        default:
            std::cerr
                << "[Session] Non-sendable message type to " << remoteEndpoint_
                << ": " << cna::network::MessageTypeValue(type)
                << '\n';
            return false;
        }

        // 직렬화된 메시지를 담을 송신 버퍼
        std::vector<std::byte> message;

        // 메시지 직렬화에 실패한 경우 전송 중단
        if (!cna::network::EncodeMessage(type, payload, message))
        {
            std::cerr << "[Session] Failed to encode message to " << remoteEndpoint_ << '\n';

            return false;
        }

        // 기존 송신 작업이 진행 중인지 확인
        const bool writeInProgress = !sendQueue_.empty();

        // 전송할 메시지를 큐에 추가
        sendQueue_.push(std::move(message));

        // 진행 중인 송신 작업이 없는 경우 첫 메시지 송신 작업 등록
        if (!writeInProgress)
        {
            WriteNext();
        }

        return true;
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
        // 이미 종료된 세션인지 확인
        if (closed_)
        {
            return;
        }

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
        // 이미 종료된 세션인지 확인
        if (closed_)
        {
            return;
        }

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

            // 메시지 타입을 확인해 전용 핸들러 함수를 호출
            if (!DispatchMessage(header, payload))
            {
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

    bool Session::DispatchMessage(const cna::network::MessageHeader& header, std::span<const std::byte> payload)
    {
        // 수신된 메시지의 메타데이터 로깅
        std::cout
            << "[Session] Message received from " << remoteEndpoint_
            << ": type=" << cna::network::MessageTypeValue(header.type)
            << ", size=" << header.size
            << ", payload=" << payload.size()
            << '\n';

        // 메시지 타입에 따라 전용 핸들러 함수 호출
        switch (header.type)
        {
        case cna::network::MessageType::TestRequest:
            return HandleTestRequest(payload);

        case cna::network::MessageType::Unknown:
        case cna::network::MessageType::TestResponse:
        default:
            std::cerr
                << "[Session] Unsupported message type from " << remoteEndpoint_
                << ": " << cna::network::MessageTypeValue(header.type)
                << '\n';

            Close();
            return false;
        }
    }

    bool Session::HandleTestRequest(std::span<const std::byte> payload)
    {
        // 테스트 요청 메시지의 전달 경로 검증용 메시지 출력
        std::cout
            << "[Session] TestRequest dispatched: payload="
            << payload.size()
            << '\n';

        // TestRequest 메시지에 대한 응답 메시지 송신 (TestResponse)
        return Send(cna::network::MessageType::TestResponse, payload);
    }

    void Session::WriteNext()
    {
        // 이미 종료된 세션인지 확인
        if (closed_)
        {
            return;
        }

        // 전송할 메시지가 비어 있거나 소켓이 닫힌 경우
        if(sendQueue_.empty() || !socket_.is_open())
        {
            return;
        }

        const std::shared_ptr<Session> self = shared_from_this();

        // 큐의 첫 번째 메시지에 대한 비동기 쓰기 작업 시작
        boost::asio::async_write
        (
            socket_,
            boost::asio::buffer(sendQueue_.front()),
            [self](const boost::system::error_code& error, const std::size_t bytesTransferred)
            {
                // 메시지 송신 결과 처리
                self->HandleWrite(error, bytesTransferred);
            }
        );
    }

    void Session::HandleWrite(const boost::system::error_code& error, const std::size_t bytesTransferred)
    {
        // 이미 종료된 세션인지 확인
        if (closed_)
        {
            return;
        }

        // 비동기 메시지 송신 중 에러가 발생한 경우
        if (error)
        {
            // 서버에서 비동기 작업을 취소한 경우가 아닌 경우
            if(error != boost::asio::error::operation_aborted)
            {
                // 데이터 송신 에러 메시지 출력
                std::cerr
                    << "Send failed to " << remoteEndpoint_
                    << ": " << error.message() << '\n';
            }

            // 메시지 큐 비우기
            sendQueue_ = std::queue<std::vector<std::byte>>{};

            // 클라이언트 소켓 종료
            if (socket_.is_open())
            {
                Close();
            }

            return;
        }

        // 비동기 쓰기 작업 중 연결 종료로 인해 큐가 비워진 경우
        if (sendQueue_.empty())
        {
            return;
        }

        // 메시지 큐의 첫 번째 메시지 크기
        const std::size_t expectedBytes = sendQueue_.front().size();

        // 요청한 전체 메시지 크기와 실제 송신 크기가 다른 경우
        if (bytesTransferred != expectedBytes)
        {
            std::cerr
                << "Incomplete message write to " << remoteEndpoint_
                << ": expected=" << expectedBytes
                << ", transferred=" << bytesTransferred
                << '\n';

            // 메시지 큐 비우기
            sendQueue_ = std::queue<std::vector<std::byte>>{};

            // 클라이언트 소켓 종료
            Close();

            return;
        }

        // 송신이 완료된 첫 번째 메시지를 큐에서 제거
        sendQueue_.pop();

        // 대기 중인 다음 메시지가 있는 경우 송신 시작
        if (!sendQueue_.empty())
        {
            WriteNext();
        }
    }

    void Session::Close()
    {
        // Close가 이미 처리된 경우
        if (closed_)
        {
            return;
        }

        closed_ = true;

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
            std::cerr << "[Session] Failed to close client socket: " << closeError.message() << '\n';
        }

        // 세션 관리자에 세션이 종료됨을 전달
        if (onClosed_)
        {
            onClosed_(id_);
        }
    }
}
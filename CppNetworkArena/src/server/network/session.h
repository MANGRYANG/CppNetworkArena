#pragma once

#include <network/message_header.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <queue>
#include <span>
#include <string>
#include <vector>

namespace cna::server
{
    // 연결된 클라이언트와의 통신을 관리하기 위한 세션 클래스
    class Session final : public std::enable_shared_from_this<Session>  // CRTP
    {
    public:
        explicit Session(boost::asio::ip::tcp::socket socket);

        // 복사 생성자 및 복사 대입 연산자 삭제
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        // 클라이언트 연결 처리 시작
        void Start();

    private:
        using Tcp = boost::asio::ip::tcp;

        // 한 번의 비동기 수신에서 사용할 버퍼 크기
        static constexpr std::size_t ReceiveBufferSize = 1024;

        // 연결된 클라이언트의 엔드포인트 정보를 가져오는 함수
        void CacheRemoteEndpoint();

        // 다음 비동기 데이터 수신 작업을 등록하는 함수
        void ReadNext();

        // 비동기 데이터 수신 결과를 처리하는 함수
        void HandleRead(const boost::system::error_code& error, std::size_t bytesTransferred);

        // 새로 수신한 데이터를 누적 버퍼에 추가하고 메시지 단위로 처리하는 함수
        bool ProcessReceivedData(std::size_t bytesTransferred);

        // 누적 버퍼에서 완전한 메시지를 반복적으로 분리하는 함수
        bool ProcessMessages();

        // 메시지 타입에 따라 전용 핸들러 함수를 호출하는 디스패치 함수
        bool DispatchMessage(const cna::network::MessageHeader& header, std::span<const std::byte> payload);

        // 테스트 요청 메시지 전용 핸들러 함수
        bool HandleTestRequest(std::span<const std::byte> payload);

        // 메시지를 직렬화하여 송신 큐에 등록하는 함수
        bool Send(cna::network::MessageType type, std::span<const std::byte> payload);

        // 다음 비동기 메시지 송신 작업을 등록하는 함수
        void WriteNext();

        // 비동기 메시지 송신 결과를 처리하는 함수
        void HandleWrite(const boost::system::error_code& error, std::size_t bytesTransferred);

        // 클라이언트 소켓을 닫는 함수
        void Close();

        // 연결된 클라이언트와 통신하는 소켓
        Tcp::socket socket_;

        // 클라이언트가 전송한 데이터를 저장하는 임시 버퍼
        std::array<std::byte, ReceiveBufferSize> receiveBuffer_{};

        // 여러 번 나뉘어 수신된 TCP 데이터를 저장하는 누적 버퍼
        std::vector<std::byte> accumulatedBuffer_;

        // 서버가 전송할 메시지를 보관하는 메시지 큐
        std::queue<std::vector<std::byte>> sendQueue_;

        // 로그 출력에 사용할 클라이언트 엔드포인트 정보
        std::string remoteEndpoint_ = "unknown";
    };
}
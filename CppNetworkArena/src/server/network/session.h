#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string>

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

        // 클라이언트 소켓을 닫는 함수
        void Close();

        // 연결된 클라이언트와 통신하는 소켓
        Tcp::socket socket_;

        // 클라이언트가 전송한 데이터를 저장하는 수신 버퍼
        std::array<char, ReceiveBufferSize> receiveBuffer_{};

        // 로그 출력에 사용할 클라이언트 엔드포인트 정보
        std::string remoteEndpoint_ = "unknown";
    };
}
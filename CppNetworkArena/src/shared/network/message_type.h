#pragma once

#include <cstdint>

namespace cna::network
{
    // 서버와 클라이언트가 주고받는 메시지 타입
    enum class MessageType : std::uint16_t
    {
        // 유효한 메시지 타입이 지정되지 않은 상태
        Unknown = 0,

        // 클라이언트가 서버에 전송하는 테스트 요청
        TestRequest = 1,

        // 서버가 클라이언트에 전송하는 테스트 응답
        TestResponse = 2
    };

    // MessageType을 네트워크 헤더에 저장할 정수 값으로 변환하는 인라인 함수
    inline constexpr std::uint16_t MessageTypeValue(const MessageType type) noexcept
    {
        return static_cast<std::uint16_t>(type);
    }
}
#pragma once

#include <network/message_type.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace cna::network
{
    // TCP 스트림에서 애플리케이션 메시지의 경계와 타입을 식별하기 위한 공용 헤더
    struct MessageHeader
    {
        // 헤더를 포함한 전체 메시지 크기
        std::uint16_t size = 0;

        // 메시지의 타입을 구분하기 위한 식별자
        MessageType type = MessageType::Unknown;
    };

    // 메시지 헤더가 네트워크 스트림에서 차지하는 크기
    inline constexpr std::size_t MessageHeaderSize = sizeof(std::uint16_t) * 2;

    // 서버에서 허용하는 단일 메시지의 최대 크기
    inline constexpr std::uint16_t MaxMessageSize = 4096;
}
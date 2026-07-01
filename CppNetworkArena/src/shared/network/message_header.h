#pragma once

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
        std::uint16_t type = 0;
    };

    // 메시지 헤더가 네트워크 스트림에서 차지하는 크기
    inline constexpr std::size_t MessageHeaderSize = sizeof(std::uint16_t) * 2;

    // 서버에서 허용하는 단일 메시지의 최대 크기
    inline constexpr std::uint16_t MaxMessageSize = 4096;

    // 네트워크 바이트 순서로 저장된 16비트 값을 호스트 값으로 변환하는 인라인 함수
    inline std::uint16_t ReadUint16(const std::span<const std::byte> data, const std::size_t offset)
    {
        const std::uint16_t high = std::to_integer<std::uint16_t>(data[offset]);
        const std::uint16_t low = std::to_integer<std::uint16_t>(data[offset + 1]);

        return static_cast<std::uint16_t>((high << 8) | low);
    }

    // 수신 데이터에서 메시지 헤더를 역직렬화하는 인라인 함수
    inline bool DecodeMessageHeader(const std::span<const std::byte> data, MessageHeader& header)
    {
        // 메시지 헤더 크기보다 수신 데이터가 작은 경우
        if (data.size() < MessageHeaderSize)
        {
            return false;
        }

        // 네트워크 바이트 순서로 저장된 헤더 값 복원
        header.size = ReadUint16(data, 0);
        header.type = ReadUint16(data, sizeof(std::uint16_t));

        return true;
    }

    // 메시지 헤더를 네트워크 바이트 순서로 직렬화하는 인라인 함수
    inline std::array<std::byte, MessageHeaderSize> EncodeMessageHeader(const MessageHeader& header)
    {
        return
        {
            std::byte
            {
                static_cast<unsigned char>((header.size >> 8) & 0xFF)
            },
            std::byte
            {
                static_cast<unsigned char>(header.size & 0xFF)
            },
            std::byte
            {
                static_cast<unsigned char>((header.type >> 8) & 0xFF)
            },
            std::byte
            {
                static_cast<unsigned char>(header.type & 0xFF)
            }
        };
    }
}
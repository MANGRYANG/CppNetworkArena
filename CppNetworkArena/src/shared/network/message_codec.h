#pragma once

#include <network/message_header.h>
#include <network/message_type.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cna::network
{
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
        header.type = static_cast<MessageType>(ReadUint16(data, sizeof(std::uint16_t)));

        return true;
    }

    // 메시지 헤더를 네트워크 바이트 순서로 직렬화하는 인라인 함수
    inline std::array<std::byte, MessageHeaderSize> EncodeMessageHeader(const MessageHeader& header)
    {
        const std::uint16_t type = MessageTypeValue(header.type);

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
                static_cast<unsigned char>((type >> 8) & 0xFF)
            },
            std::byte
            {
                static_cast<unsigned char>(type & 0xFF)
            }
        };
    }

    // 메시지 타입과 Payload를 하나의 송신 바이트 버퍼로 직렬화하는 함수
    inline bool EncodeMessage(const MessageType type, const std::span<const std::byte> payload, std::vector<std::byte>& encodedMessage)
    {
        // 직렬화된 메시지를 저장할 출력 매개변수 초기화
        encodedMessage.clear();

        // 메시지 전체 크기 계산
        const std::size_t totalSize = MessageHeaderSize + payload.size();

        // 메시지 전체 크기가 허용 가능한 최대 크기를 초과한 경우 실패 처리
        if (totalSize > MaxMessageSize)
        {
            return false;
        }

        // 메시지 헤더 구성
        const MessageHeader header
        {
            static_cast<std::uint16_t>(totalSize),
            type
        };

        // 메시지 헤더를 네트워크 바이트 순서로 직렬화
        const auto encodedHeader = EncodeMessageHeader(header);

        // 전체 메시지 크기만큼 송신 버퍼 공간을 미리 확보
        encodedMessage.reserve(totalSize);

        // 직렬화된 메시지 헤더를 송신 버퍼에 추가
        encodedMessage.insert
        (
            encodedMessage.end(),
            encodedHeader.begin(),
            encodedHeader.end()
        );

        // 메시지 헤더 뒤에 Payload 추가
        encodedMessage.insert
        (
            encodedMessage.end(),
            payload.begin(),
            payload.end()
        );

        return true;
    }
}
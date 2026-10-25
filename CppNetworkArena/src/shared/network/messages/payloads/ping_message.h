#pragma once

#include <network/messages/core/message_codec.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/*
*
Ping 타입 메시지 Payload wire format :
[clientPingTxTimestampNanoseconds: uint64]
*
*/

namespace cna::network
{
    // 클라이언트가 서버에 전달하는 Ping 타입 메시지 Payload
    struct PingPayload final
    {
        // Ping 송신 시점에서 클라이언트에서 측정한 타임스탬프
        std::uint64_t clientPingTxTimestampNanoseconds = 0;
    };

    // Ping Payload가 네트워크 스트림에서 차지하는 크기
    inline constexpr std::size_t PingPayloadSize = sizeof(std::uint64_t);

    // Ping Payload를 직렬화하는 함수
    inline std::array<std::byte, PingPayloadSize> EncodePingPayload(const PingPayload& ping)
    {
        std::array<std::byte, PingPayloadSize> payload{};

        WriteUint64(payload, 0, ping.clientPingTxTimestampNanoseconds);

        return payload;
    }

    // Ping Payload를 역직렬화하는 함수
    inline bool DecodePingPayload(const std::span<const std::byte> payload, PingPayload& decodedPing)
    {
        // Payload 크기 검증
        if (payload.size() != PingPayloadSize)
        {
            return false;
        }

        decodedPing.clientPingTxTimestampNanoseconds = ReadUint64(payload, 0);

        return true;
    }
}
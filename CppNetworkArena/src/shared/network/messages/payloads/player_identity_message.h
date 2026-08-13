#pragma once

#include <NetworkTypes.h>
#include <network/messages/core/message_codec.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

/*
*
PlayerIdentity 타입 메시지 Payload wire format :
[roomId: uint32]
[playerId: uint32]
*
*/

namespace cna::network
{
    // 서버가 클라이언트에 전달하는 PlayerIdentity 타입 메시지 Payload
    struct PlayerIdentityPayload
    {
        // 플레이어가 입장한 Room의 ID
        cna::RoomId roomId = 0;

        // 서버가 발급한 Room 내부 플레이어 ID
        cna::PlayerId playerId = 0;
    };

    // PlayerIdentity Payload가 네트워크 스트림에서 차지하는 크기
    inline constexpr std::size_t PlayerIdentityPayloadSize = sizeof(std::uint32_t) * 2;

    // PlayerIdentity Payload를 직렬화하는 함수
    inline bool EncodePlayerIdentityPayload(const PlayerIdentityPayload& identity, std::array<std::byte, PlayerIdentityPayloadSize>& payload)
    {
        // 유효하지 않은 Room ID 또는 플레이어 ID인 경우
        if (identity.roomId == 0 || identity.playerId == 0)
        {
            return false;
        }

        // Room ID 직렬화 후 출력 배열에 기록
        WriteUint32(payload, 0, identity.roomId);

        // 플레이어 ID 직렬화 후 출력 배열에 기록
        WriteUint32(payload, sizeof(std::uint32_t), identity.playerId);

        return true;
    }

    // PlayerIdentity Payload를 역직렬화하는 함수
    inline bool DecodePlayerIdentityPayload(const std::span<const std::byte> payload, PlayerIdentityPayload& identity)
    {
        // Payload 크기 검증
        if (payload.size() != PlayerIdentityPayloadSize)
        {
            return false;
        }

        PlayerIdentityPayload decodedIdentity;

        // Room ID 역직렬화
        decodedIdentity.roomId = ReadUint32(payload, 0);

        // 플레이어 ID 역직렬화
        decodedIdentity.playerId = ReadUint32(payload, sizeof(std::uint32_t));

        // 유효하지 않은 Room ID 또는 플레이어 ID인 경우
        if (decodedIdentity.roomId == 0 || decodedIdentity.playerId == 0)
        {
            return false;
        }

        // 출력 객체 갱신
        identity = decodedIdentity;

        return true;
    }
}
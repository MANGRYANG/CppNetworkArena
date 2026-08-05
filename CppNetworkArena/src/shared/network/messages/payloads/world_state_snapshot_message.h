#pragma once

#include <NetworkTypes.h>
#include <network/messages/core/message_codec.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

/*
* 
WorldStateSnapshot 타입 메시지 Payload wire format :
[roomId: uint32]
[playerCount: uint16]
[players: PlayerStateSnapshot x playerCount]
*
*/
namespace cna::network
{
    // WorldStateSnapshot에 포함될 플레이어 내부 상태
    struct PlayerStateSnapshot
    {
        // 클라이언트가 플레이어를 식별하기 위한 고유 ID
        cna::PlayerId playerId = 0;

        // 플레이어의 현재 월드 위치
        float positionX = 0.0f;
        float positionY = 0.0f;
        float positionZ = 0.0f;

        // 플레이어의 현재 이동 속도
        float velocityX = 0.0f;
        float velocityY = 0.0f;
        float velocityZ = 0.0f;
    };

    // 서버가 클라이언트로 전송하는 Room의 게임 상태 스냅샷의 논리 데이터
    struct WorldStateSnapshot
    {
        // 스냅샷이 생성된 Room의 ID
        cna::RoomId roomId = 0;

        // 스냅샷에 포함된 플레이어 상태 목록
        std::vector<PlayerStateSnapshot> players;
    };

    // 플레이어 목록을 제외한 WorldStateSnapshot Payload의 고정 크기
    inline constexpr std::size_t WorldStateSnapshotFixedPayloadSize = sizeof(std::uint32_t) + sizeof(std::uint16_t);

    // 플레이어 한 명의 상태가 네트워크 Payload에서 차지하는 크기
    inline constexpr std::size_t PlayerStateSnapshotSize = sizeof(std::uint32_t) + sizeof(float) * 6;

    // 하나의 WorldStateSnapshot 메시지에 포함할 수 있는 최대 플레이어 수
    inline constexpr std::size_t MaxWorldStateSnapshotPlayerCount =
        (MaxMessageSize - (MessageHeaderSize + WorldStateSnapshotFixedPayloadSize)) /
        PlayerStateSnapshotSize;

    // 플레이어 상태의 모든 실수 값이 정상적인 유한 값인지 확인하는 함수
    inline bool IsFinitePlayerStateSnapshot(const PlayerStateSnapshot& state) noexcept
    {
        return
            std::isfinite(state.positionX) && std::isfinite(state.positionY) && std::isfinite(state.positionZ) &&
            std::isfinite(state.velocityX) && std::isfinite(state.velocityY) && std::isfinite(state.velocityZ);
    }

    // WorldStateSnapshot Payload를 직렬화하는 함수
    inline bool EncodeWorldStateSnapshotPayload(const WorldStateSnapshot& snapshot, std::vector<std::byte>& payload)
    {
        // 이전 직렬화 결과 초기화
        payload.clear();

        // 유효하지 않은 Room ID인 경우 실패 처리
        if (snapshot.roomId == 0)
        {
            return false;
        }

        // 허용 가능한 최대 플레이어 수를 초과한 경우 실패 처리
        if (snapshot.players.size() > MaxWorldStateSnapshotPlayerCount)
        {
            return false;
        }

        // Payload 크기 계산
        const std::size_t payloadSize = WorldStateSnapshotFixedPayloadSize + snapshot.players.size() * PlayerStateSnapshotSize;

        // 계산된 Payload 크기만큼의 직렬화 버퍼 공간 확보
        payload.resize(payloadSize);

        // Room ID 직렬화
        WriteUint32(payload, 0, snapshot.roomId);

        // 플레이어 수 직렬화
        WriteUint16(payload, sizeof(std::uint32_t), static_cast<std::uint16_t>(snapshot.players.size()));

        std::size_t offset = WorldStateSnapshotFixedPayloadSize;

        // 모든 플레이어를 순회하면서 플레이어 상태 직렬화
        for (const PlayerStateSnapshot& state : snapshot.players)
        {
            // 유효하지 않은 플레이어 ID나 실수 값이 포함된 경우 실패 처리
            if (state.playerId == 0 || !IsFinitePlayerStateSnapshot(state))
            {
                payload.clear();

                return false;
            }

            WriteUint32(payload, offset, state.playerId);
            offset += sizeof(std::uint32_t);

            WriteFloat32(payload, offset, state.positionX);
            offset += sizeof(std::uint32_t);

            WriteFloat32(payload, offset, state.positionY);
            offset += sizeof(std::uint32_t);

            WriteFloat32(payload, offset, state.positionZ);
            offset += sizeof(std::uint32_t);

            WriteFloat32(payload, offset, state.velocityX);
            offset += sizeof(std::uint32_t);

            WriteFloat32(payload, offset, state.velocityY);
            offset += sizeof(std::uint32_t);

            WriteFloat32(payload, offset, state.velocityZ);
            offset += sizeof(std::uint32_t);
        }

        return true;
    }

    // WorldStateSnapshot Payload를 역직렬화하는 함수
    inline bool DecodeWorldStateSnapshotPayload(const std::span<const std::byte> payload, WorldStateSnapshot& snapshot)
    {
        // Payload 크기가 고정 크기보다 작은 경우 실패 처리
        if (payload.size() < WorldStateSnapshotFixedPayloadSize)
        {
            return false;
        }

        // 역직렬화가 완료되기 전까지 기존 출력 객체를 변경하지 않기 위한 임시 객체
        WorldStateSnapshot decodedSnapshot;

        decodedSnapshot.roomId = ReadUint32(payload, 0);

        const std::uint16_t playerCount = ReadUint16(payload, sizeof(std::uint32_t));

        // 유효하지 않은 Room ID인 경우 실패 처리
        if (decodedSnapshot.roomId == 0)
        {
            return false;
        }

        // 허용 가능한 최대 플레이어 수를 초과한 경우 실패 처리
        if (playerCount > MaxWorldStateSnapshotPlayerCount)
        {
            return false;
        }

        // 플레이어 수를 기반으로 예상 Payload 크기 계산
        const std::size_t expectedPayloadSize = WorldStateSnapshotFixedPayloadSize + static_cast<std::size_t>(playerCount) * PlayerStateSnapshotSize;

        // 계산한 예상 Payload 크기와 실제 Payload 크기가 일치하지 않는 경우 실패 처리
        if (payload.size() != expectedPayloadSize)
        {
            return false;
        }

        // 플레이어 수만큼 플레이어 목록 공간 예약
        decodedSnapshot.players.reserve(playerCount);

        std::size_t offset = WorldStateSnapshotFixedPayloadSize;

        // 각 플레이어의 스냅샷을 순차적으로 역직렬화
        for (std::uint16_t index = 0; index < playerCount; ++index)
        {
            PlayerStateSnapshot state;

            state.playerId = ReadUint32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.positionX = ReadFloat32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.positionY = ReadFloat32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.positionZ = ReadFloat32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.velocityX = ReadFloat32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.velocityY = ReadFloat32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.velocityZ = ReadFloat32(payload, offset);
            offset += sizeof(std::uint32_t);

            // 유효하지 않은 플레이어 ID나 실수 값이 포함된 경우 실패 처리
            if (state.playerId == 0 || !IsFinitePlayerStateSnapshot(state))
            {
                return false;
            }

            decodedSnapshot.players.push_back(state);
        }

        // 모든 필드가 정상적으로 복원된 경우 출력 객체 갱신
        snapshot = std::move(decodedSnapshot);

        return true;
    }
}
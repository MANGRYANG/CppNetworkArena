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
[serverTick: uint64]
[roomId: uint32]
[gamePlayState: uint8]
[attackingSide: uint8]
[winningSide: uint8]
[gameRemainingSeconds: float32]
[attackRoleSwitchRemainingSeconds: float32]
[flameScore: uint32]
[frostScore: uint32]
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

        // 플레이어의 현재 진영
        cna::PlayerSide side = cna::PlayerSide::None;

        // 플레이어의 현재 월드 위치
        float positionX = 0.0f;
        float positionY = 0.0f;
        float positionZ = 0.0f;

        // 정규화된 플레이어의 현재 이동 방향
        float directionX = 0.0f;
        float directionY = 0.0f;
    };

    // 서버가 클라이언트로 전송하는 Room의 게임 상태 스냅샷의 논리 데이터
    struct WorldStateSnapshot
    {
        // 스냅샷이 생성된 서버 게임 시뮬레이션 Tick
        cna::ServerTick serverTick = 0;

        // 스냅샷이 생성된 Room의 ID
        cna::RoomId roomId = 0;

        // 현재 게임 진행 상태
        cna::GamePlayState gamePlayState = cna::GamePlayState::Waiting;

        // 현재 공격 역할을 가진 플레이어 진영
        cna::PlayerSide attackingSide = cna::PlayerSide::None;

        // 종료된 게임의 승리 진영
        cna::PlayerSide winningSide = cna::PlayerSide::None;

        // 게임 종료까지 남은 시간
        float gameRemainingSeconds = 0.0f;

        // 다음 공격 진영 교대까지 남은 시간
        float attackRoleSwitchRemainingSeconds = 0.0f;

        // Flame 진영의 현재 점수
        std::uint32_t flameScore = 0;

        // Frost 진영의 현재 점수
        std::uint32_t frostScore = 0;

        // 스냅샷에 포함된 플레이어 상태 목록
        std::vector<PlayerStateSnapshot> players;
    };

    // 플레이어 목록을 제외한 WorldStateSnapshot Payload의 고정 크기
    inline constexpr std::size_t WorldStateSnapshotFixedPayloadSize =
        sizeof(std::uint64_t) +
        sizeof(std::uint32_t) +
        sizeof(std::uint8_t) * 3 +
        sizeof(float) * 2 +
        sizeof(std::uint32_t) * 2 +
        sizeof(std::uint16_t);

    // 플레이어 한 명의 상태가 네트워크 Payload에서 차지하는 크기
    inline constexpr std::size_t PlayerStateSnapshotSize = sizeof(std::uint32_t) + sizeof(std::uint8_t) + sizeof(float) * 5;

    // 하나의 WorldStateSnapshot 메시지에 포함할 수 있는 최대 플레이어 수
    inline constexpr std::size_t MaxWorldStateSnapshotPlayerCount =
        (MaxMessageSize - (MessageHeaderSize + WorldStateSnapshotFixedPayloadSize)) /
        PlayerStateSnapshotSize;

    // 네트워크로 전달 가능한 게임 진행 상태인지 확인하는 함수
    inline bool IsValidGamePlayState(const cna::GamePlayState gamePlayState) noexcept
    {
        return
            gamePlayState == cna::GamePlayState::Waiting ||
            gamePlayState == cna::GamePlayState::Playing ||
            gamePlayState == cna::GamePlayState::Finished;
    }

    // 네트워크로 전달 가능한 플레이어 진영인지 확인하는 함수
    inline bool IsValidPlayerSide(const cna::PlayerSide side) noexcept
    {
        return
            side == cna::PlayerSide::None ||
            side == cna::PlayerSide::Flame ||
            side == cna::PlayerSide::Frost;
    }

    // 실제 플레이어에게 할당된 진영인지 확인하는 함수
    inline bool IsAssignedPlayerSide(const cna::PlayerSide side) noexcept
    {
        return
            side == cna::PlayerSide::Flame ||
            side == cna::PlayerSide::Frost;
    }

    // 월드 상태의 게임 관련 실수 값이 정상적인 유한 값인지 확인하는 함수
    inline bool IsFiniteWorldStateSnapshot(const WorldStateSnapshot& snapshot) noexcept
    {
        return
            std::isfinite(snapshot.gameRemainingSeconds) &&
            std::isfinite(snapshot.attackRoleSwitchRemainingSeconds);
    }

    // 플레이어 상태의 모든 실수 값이 정상적인 유한 값인지 확인하는 함수
    inline bool IsFinitePlayerStateSnapshot(const PlayerStateSnapshot& state) noexcept
    {
        return
            std::isfinite(state.positionX) && std::isfinite(state.positionY) && std::isfinite(state.positionZ) &&
            std::isfinite(state.directionX) && std::isfinite(state.directionY);
    }

    // WorldStateSnapshot Payload를 직렬화하는 함수
    inline bool EncodeWorldStateSnapshotPayload(const WorldStateSnapshot& snapshot, std::vector<std::byte>& payload)
    {
        // 이전 직렬화 결과 초기화
        payload.clear();

        // 유효하지 않은 서버 Tick 또는 Room ID인 경우 실패 처리
        if (snapshot.serverTick == 0 || snapshot.roomId == 0)
        {
            return false;
        }

        // 유효하지 않은 게임 상태가 포함된 경우 실패 처리
        if (
            !IsValidGamePlayState(snapshot.gamePlayState) ||
            !IsValidPlayerSide(snapshot.attackingSide) ||
            !IsValidPlayerSide(snapshot.winningSide) ||
            !IsFiniteWorldStateSnapshot(snapshot) ||
            snapshot.gameRemainingSeconds < 0.0f ||
            snapshot.attackRoleSwitchRemainingSeconds < 0.0f
        )
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

        std::size_t offset = 0;

        // 서버 Tick 직렬화
        WriteUint64(payload, 0, snapshot.serverTick);
        offset += sizeof(std::uint64_t);

        // Room ID 직렬화
        WriteUint32(payload, offset, snapshot.roomId);
        offset += sizeof(std::uint32_t);

        // 현재 게임 진행 상태 직렬화
        payload[offset] = static_cast<std::byte>(static_cast<std::uint8_t>(snapshot.gamePlayState));
        offset += sizeof(std::uint8_t);

        // 현재 공격 진영 직렬화
        payload[offset] = static_cast<std::byte>(static_cast<std::uint8_t>(snapshot.attackingSide));
        offset += sizeof(std::uint8_t);

        // 현재 승리 진영 직렬화
        payload[offset] = static_cast<std::byte>(static_cast<std::uint8_t>(snapshot.winningSide));
        offset += sizeof(std::uint8_t);

        // 전체 게임 잔여 시간 직렬화
        WriteFloat32(payload, offset, snapshot.gameRemainingSeconds);
        offset += sizeof(float);

        // 공수 전환 잔여 시간 직렬화
        WriteFloat32(payload, offset, snapshot.attackRoleSwitchRemainingSeconds);
        offset += sizeof(float);

        // Flame 진영 점수 직렬화
        WriteUint32(payload, offset, snapshot.flameScore);
        offset += sizeof(std::uint32_t);

        // Frost 진영 점수 직렬화
        WriteUint32(payload, offset, snapshot.frostScore);
        offset += sizeof(std::uint32_t);

        // 플레이어 수 직렬화
        WriteUint16(payload, offset, static_cast<std::uint16_t>(snapshot.players.size()));
        offset += sizeof(std::uint16_t);

        // 모든 플레이어를 순회하면서 플레이어 상태 직렬화
        for (const PlayerStateSnapshot& state : snapshot.players)
        {
            // 유효하지 않은 플레이어 ID나 진영, 실수 값이 포함된 경우 실패 처리
            if (state.playerId == 0 || !IsAssignedPlayerSide(state.side) || !IsFinitePlayerStateSnapshot(state))
            {
                payload.clear();

                return false;
            }

            WriteUint32(payload, offset, state.playerId);
            offset += sizeof(std::uint32_t);

            // 플레이어 진영을 1바이트 값으로 직렬화
            payload[offset] = static_cast<std::byte>(static_cast<std::uint8_t>(state.side));
            offset += sizeof(std::uint8_t);

            WriteFloat32(payload, offset, state.positionX);
            offset += sizeof(float);

            WriteFloat32(payload, offset, state.positionY);
            offset += sizeof(float);

            WriteFloat32(payload, offset, state.positionZ);
            offset += sizeof(float);

            WriteFloat32(payload, offset, state.directionX);
            offset += sizeof(float);

            WriteFloat32(payload, offset, state.directionY);
            offset += sizeof(float);
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

        std::size_t offset = 0;

        // 서버 Tick 역직렬화
        decodedSnapshot.serverTick = ReadUint64(payload, 0);
        offset += sizeof(std::uint64_t);

        // Room ID 역직렬화
        decodedSnapshot.roomId = ReadUint32(payload, offset);
        offset += sizeof(std::uint32_t);

        // 게임 진행 상태 역직렬화
        decodedSnapshot.gamePlayState = static_cast<cna::GamePlayState>(std::to_integer<std::uint8_t>(payload[offset]));
        offset += sizeof(std::uint8_t);

        // 현재 공격 진영 역직렬화
        decodedSnapshot.attackingSide = static_cast<cna::PlayerSide>(std::to_integer<std::uint8_t>(payload[offset]));
        offset += sizeof(std::uint8_t);

        // 현재 승리 진영 역직렬화
        decodedSnapshot.winningSide = static_cast<cna::PlayerSide>(std::to_integer<std::uint8_t>(payload[offset]));
        offset += sizeof(std::uint8_t);

        // 전체 게임 잔여 시간 역직렬화
        decodedSnapshot.gameRemainingSeconds = ReadFloat32(payload, offset);
        offset += sizeof(float);

        // 공수 전환 잔여 시간 역직렬화
        decodedSnapshot.attackRoleSwitchRemainingSeconds = ReadFloat32(payload, offset);
        offset += sizeof(float);

        // Flame 진영 점수 역직렬화
        decodedSnapshot.flameScore = ReadUint32(payload, offset);
        offset += sizeof(std::uint32_t);

        // Frost 진영 점수 역직렬화
        decodedSnapshot.frostScore = ReadUint32(payload, offset);
        offset += sizeof(std::uint32_t);

        // 플레이어 수 역직렬화
        const std::uint16_t playerCount = ReadUint16(payload, offset);
        offset += sizeof(std::uint16_t);

        // 유효하지 않은 서버 Tick 또는 Room ID인 경우 실패 처리
        if (decodedSnapshot.serverTick == 0 || decodedSnapshot.roomId == 0)
        {
            return false;
        }

        // 유효하지 않은 게임 상태가 포함된 경우 실패 처리
        if (
            !IsValidGamePlayState(decodedSnapshot.gamePlayState) ||
            !IsValidPlayerSide(decodedSnapshot.attackingSide) ||
            !IsValidPlayerSide(decodedSnapshot.winningSide) ||
            !IsFiniteWorldStateSnapshot(decodedSnapshot) ||
            decodedSnapshot.gameRemainingSeconds < 0.0f ||
            decodedSnapshot.attackRoleSwitchRemainingSeconds < 0.0f
        )
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

        // 각 플레이어의 스냅샷을 순차적으로 역직렬화
        for (std::uint16_t index = 0; index < playerCount; ++index)
        {
            PlayerStateSnapshot state;

            state.playerId = ReadUint32(payload, offset);
            offset += sizeof(std::uint32_t);

            state.side = static_cast<cna::PlayerSide>(std::to_integer<std::uint8_t>(payload[offset]));
            offset += sizeof(std::uint8_t);

            state.positionX = ReadFloat32(payload, offset);
            offset += sizeof(float);

            state.positionY = ReadFloat32(payload, offset);
            offset += sizeof(float);

            state.positionZ = ReadFloat32(payload, offset);
            offset += sizeof(float);

            state.directionX = ReadFloat32(payload, offset);
            offset += sizeof(float);

            state.directionY = ReadFloat32(payload, offset);
            offset += sizeof(float);

            // 유효하지 않은 플레이어 ID나 진영, 실수 값이 포함된 경우 실패 처리
            if (state.playerId == 0 || !IsAssignedPlayerSide(state.side) || !IsFinitePlayerStateSnapshot(state))
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
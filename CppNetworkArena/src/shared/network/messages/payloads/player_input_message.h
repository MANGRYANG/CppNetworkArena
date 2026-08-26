#pragma once

#include <network/messages/core/message_codec.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

/*
*
PlayerInput 타입 메시지 Payload wire format :
[moveX: int16]
[moveY: int16]
[moveZ: int16]
*
*/

namespace cna::network
{
    // 클라이언트가 서버에 전달하는 플레이어 입력 Payload
    struct PlayerInputPayload
    {
        // 플레이어의 X축 이동 입력 세기 원시값
        std::int16_t moveX = 0;
        // 플레이어의 Y축 이동 입력 세기 원시값
        std::int16_t moveY = 0;
        // 플레이어의 Z축 이동 입력 세기 원시값
        std::int16_t moveZ = 0;
    };

    // PlayerInput Payload가 네트워크 스트림에서 차지하는 크기
    inline constexpr std::size_t PlayerInputPayloadSize = sizeof(std::int16_t) * 3;

    // 각 축에 보낼 수 있는 입력 세기의 최대 원시값
    inline constexpr std::int16_t MaxPlayerInputAxisRawValue = 1000;

    // 각 축에 대한 플레이어 입력이 허용 세기 범위 안에 있는지 확인하는 함수
    inline bool IsValidPlayerInputAxisRawValue(const std::int16_t value) noexcept
    {
        return (value >= -MaxPlayerInputAxisRawValue && value <= MaxPlayerInputAxisRawValue);
    }

    // 플레이어 입력이 허용 세기 범위 내인지 확인하는 함수
    inline bool IsValidPlayerInput(const PlayerInputPayload& input) noexcept
    {
        return
            IsValidPlayerInputAxisRawValue(input.moveX) &&
            IsValidPlayerInputAxisRawValue(input.moveY) &&
            IsValidPlayerInputAxisRawValue(input.moveZ);
    }

    // PlayerInput Payload를 직렬화하는 함수
    inline bool EncodePlayerInputPayload(const PlayerInputPayload& input, std::array<std::byte, PlayerInputPayloadSize>& payload)
    {
        // 출력 버퍼 초기화
        payload.fill(std::byte{ 0 });

        // 플레이어 입력 값이 허용 범위를 벗어난 경우
        if (!IsValidPlayerInput(input))
        {
            return false;
        }

        WriteUint16(payload, 0, std::bit_cast<std::uint16_t>(input.moveX));
        WriteUint16(payload, sizeof(std::int16_t), std::bit_cast<std::uint16_t>(input.moveY));
        WriteUint16(payload, sizeof(std::int16_t) * 2, std::bit_cast<std::uint16_t>(input.moveZ));

        return true;
    }

    // PlayerInput Payload를 역직렬화하는 함수
    inline bool DecodePlayerInputPayload(const std::span<const std::byte> payload, PlayerInputPayload& input)
    {
        // PlayerInput Payload 크기와 일치하지 않는 경우 실패 처리
        if (payload.size() != PlayerInputPayloadSize)
        {
            return false;
        }

        // 역직렬화 작업이 끝나기 전까지 출력 객체를 변경하지 않기 위한 임시 객체
        PlayerInputPayload decodedInput;

        decodedInput.moveX = std::bit_cast<std::int16_t>(ReadUint16(payload, 0));
        decodedInput.moveY = std::bit_cast<std::int16_t>(ReadUint16(payload, sizeof(std::int16_t)));
        decodedInput.moveZ = std::bit_cast<std::int16_t>(ReadUint16(payload, sizeof(std::int16_t) * 2));

        // 플레이어 입력 값이 허용 범위를 벗어난 경우
        if (!IsValidPlayerInput(decodedInput))
        {
            return false;
        }

        input = decodedInput;

        return true;
    }
}
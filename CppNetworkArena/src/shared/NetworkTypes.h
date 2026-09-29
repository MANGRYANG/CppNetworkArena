#pragma once

#include <cstdint>

namespace cna
{
    // 서버가 접속한 클라이언트에 발급하는 플레이어 ID
    using PlayerId = std::uint32_t;

    // 플레이어가 참가한 Room을 식별하기 위한 ID
    using RoomId = std::uint32_t;

    // 서버 게임 시뮬레이션의 실행 순서를 식별하기 위한 Tick
    using ServerTick = std::uint64_t;

    // 플레이어의 현재 진영을 구분하는 열거형
    enum class PlayerSide : std::uint8_t
    {
        None = 0,
        Flame,
        Frost
    };
}
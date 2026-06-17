#pragma once

#include <cstdint>

namespace cna
{
    // 서버가 접속한 클라이언트에 발급하는 플레이어 ID
    using PlayerId = std::uint32_t;

    // 플레이어가 참가한 Room을 식별하기 위한 ID
    using RoomId = std::uint32_t;
}
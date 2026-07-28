#pragma once

#include <cstdint>
#include <functional>

namespace cna::network
{
    struct PlayerInputPayload;
}

namespace cna::server
{
    // 서버에서 각 클라이언트 연결 세션을 구분하기 위한 식별자
    using SessionId = std::uint64_t;

    // 세션 종료 시 SessionManager에 제거를 요청하기 위한 콜백 타입
    using SessionClosedCallback = std::function<void(SessionId)>;

    // 세션이 수신한 플레이어 입력을 서버로 전달하기 위한 콜백 타입
    using PlayerInputCallback = std::function<void(SessionId, const cna::network::PlayerInputPayload&)>;
}
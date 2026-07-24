#pragma once

#include "../network/session_types.h"

#include <memory>

namespace cna::server
{
    class Session;

    // 플레이어 상태 관리를 위한 구조체
    struct PlayerState
    {
        // 플레이어 X 좌표 위치
        float positionX = 0.0f;
        // 플레이어 Y 좌표 위치
        float positionY = 0.0f;
        // 플레이어 Z 좌표 위치
        float positionZ = 0.0f;

        // X축 방향 플레이어 속도
        float velocityX = 0.0f;
        // Y축 방향 플레이어 속도
        float velocityY = 0.0f;
        // Z축 방향 플레이어 속도
        float velocityZ = 0.0f;
    };

    // Room에 입장할 플레이어를 나타내는 클래스
    class Player final
    {
    public:
        explicit Player(std::shared_ptr<Session> session);

        // 플레이어와 연결된 세션 ID를 반환하는 함수
        SessionId GetSessionId() const noexcept;

        // 플레이어의 상태를 반환하는 함수
        PlayerState& GetState() noexcept;

        // 플레이어의 상태를 읽기 전용으로 반환하는 함수
        const PlayerState& GetState() const noexcept;

        // 플레이어와 연결된 세션 객체 획득을 시도하는 함수
        std::shared_ptr<Session> LockSession() const;

    private:
        // 플레이어와 연결된 세션 ID
        SessionId sessionId_ = 0;

        // 세션에 대한 weak_ptr을 보관하기 위한 멤버
        std::weak_ptr<Session> session_;

        // 플레이어 상태
        PlayerState state_;
    };
}
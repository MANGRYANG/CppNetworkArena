#pragma once

#include "session_types.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace cna::server
{
    class Session;

    // 현재 서버에 연결된 활성 세션 객체들을 관리하는 모듈
    class SessionManager final
    {
    public:
        SessionManager() = default;

        // 복사 생성자 및 복사 대입 연산자 삭제
        SessionManager(const SessionManager&) = delete;
        SessionManager& operator=(const SessionManager&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        SessionManager(SessionManager&&) = delete;
        SessionManager& operator=(SessionManager&&) = delete;

        // 새 Session에 부여할 고유 ID를 생성하는 함수
        SessionId GenerateSessionId() noexcept;

        // 활성 Session 목록에 세션을 등록하는 함수
        void Add(std::shared_ptr<Session> session);

        // 활성 세션 목록에서 특정 ID의 세션을 제거하는 함수
        void Remove(SessionId sessionId);

        // 현재 활성 세션의 개수를 반환하는 함수
        std::size_t Count() const noexcept;

    private:
        // 다음에 생성할 세션에 할당할 ID
        SessionId nextSessionId_ = 1;

        // 현재 서버에 연결된 활성 세션 목록
        std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    };
}
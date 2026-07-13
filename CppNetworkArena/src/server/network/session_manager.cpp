#include "session_manager.h"

#include "session.h"

#include <iostream>
#include <utility>

namespace cna::server
{
    SessionId SessionManager::GenerateSessionId() noexcept
    {
        return nextSessionId_++;
    }

    void SessionManager::Add(std::shared_ptr<Session> session)
    {
        if (!session)
        {
            return;
        }

        const SessionId sessionId = session->GetId();

        // 활성 세션 목록에 새 세션 등록
        sessions_[sessionId] = std::move(session);

        // 현재 활성 세션 개수 출력
        std::cout
            << "[SessionManager] Session added: id=" << sessionId
            << ", active=" << sessions_.size() << '\n';
    }

    void SessionManager::Remove(const SessionId sessionId)
    {
        // 활성 세션 목록에서 세션 제거
        const std::size_t removedCount = sessions_.erase(sessionId);

        // 제거할 세션이 목록에 없어 제거하지 못한 경우
        if (removedCount == 0)
        {
            return;
        }

        // 현재 활성 세션 개수 출력
        std::cout
            << "[SessionManager] Session removed: id=" << sessionId
            << ", active=" << sessions_.size() << '\n';
    }

    std::size_t SessionManager::Count() const noexcept
    {
        return sessions_.size();
    }
}
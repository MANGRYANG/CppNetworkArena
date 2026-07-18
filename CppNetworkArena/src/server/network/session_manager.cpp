#include "session_manager.h"

#include "session.h"

#include <iostream>
#include <utility>
#include <vector>

namespace cna::server
{
    std::shared_ptr<Session> SessionManager::CreateSession(Tcp::socket socket)
    {
        // 새 세션에 부여할 고유 ID 생성
        const SessionId sessionId = GenerateSessionId();

        // 세션 종료 시 SessionManager의 활성 세션 목록에서 제거
        std::shared_ptr<Session> session =
            std::make_shared<Session>
            (
                sessionId,
                std::move(socket),
                [this](const SessionId closedSessionId)
                {
                    // 종료된 세션을 활성 세션 목록에서 제거
                    RemoveSession(closedSessionId);
                }
            );

        // 활성 세션 목록에 새 세션 등록
        auto [sessionIterator, inserted] = sessions_.emplace(sessionId, std::move(session));

        // 세션 ID가 중복되어 등록에 실패한 경우
        if (!inserted)
        {
            std::cerr
                << "[SessionManager] Failed to add session: id="
                << sessionId << '\n';

            return nullptr;
        }

        // 현재 활성 세션 개수 출력
        std::cout
            << "[SessionManager] Session added: id=" << sessionId
            << ", active=" << sessions_.size() << '\n';

        return sessionIterator->second;
    }

    void SessionManager::RemoveSession(const SessionId sessionId)
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

    void SessionManager::CloseAll()
    {
        // 종료할 세션 목록을 별도 컨테이너에 복사
        std::vector<std::shared_ptr<Session>> sessionsToClose;
        sessionsToClose.reserve(sessions_.size());

        for (const auto& [sessionId, session] : sessions_)
        {
            if (session)
            {
                sessionsToClose.push_back(session);
            }
        }

        // 복사된 목록을 기준으로 모든 세션 종료
        for (const std::shared_ptr<Session>& session : sessionsToClose)
        {
            session->Stop();
        }

        // 관리 대상 세션들의 참조 제거
        sessions_.clear();

        // 전체 세션 종료 결과 출력
        std::cout << "[SessionManager] All sessions closed: active=" << sessions_.size() << '\n';
    }

    std::size_t SessionManager::Count() const noexcept
    {
        return sessions_.size();
    }

    SessionId SessionManager::GenerateSessionId() noexcept
    {
        return nextSessionId_++;
    }
}
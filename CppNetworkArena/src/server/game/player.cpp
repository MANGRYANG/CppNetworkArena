#include "player.h"

#include "../network/session.h"

namespace cna::server
{
    Player::Player(std::shared_ptr<Session> session)
    {
        // 유효하지 않은 세션이면 기본 상태 유지
        if (!session)
        {
            return;
        }

        sessionId_ = session->GetId();
        session_ = session;
    }

    SessionId Player::GetSessionId() const noexcept
    {
        return sessionId_;
    }

    PlayerState& Player::GetState() noexcept
    {
        return state_;
    }

    const PlayerState& Player::GetState() const noexcept
    {
        return state_;
    }

    std::shared_ptr<Session> Player::LockSession() const
    {
        return session_.lock();
    }
}
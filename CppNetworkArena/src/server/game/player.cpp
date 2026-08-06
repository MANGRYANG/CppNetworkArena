#include "player.h"

#include "../network/session.h"

namespace cna::server
{
    Player::Player(const cna::PlayerId playerId, std::shared_ptr<Session> session)
    {
        // 플레이어 식별자나 세션이 유효하지 않은 경우 기본 상태 유지
        if (playerId == 0 || !session)
        {
            return;
        }

        playerId_ = playerId;
        sessionId_ = session->GetId();
        session_ = session;
    }

    cna::PlayerId Player::GetPlayerId() const noexcept
    {
        return playerId_;
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
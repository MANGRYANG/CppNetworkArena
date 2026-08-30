#include "client_game_state.h"

#include <algorithm>

namespace cna::client
{
    bool ClientGameState::ApplyPlayerIdentity(const cna::network::PlayerIdentityPayload& identity)
    {
        // 유효하지 않은 Room 또는 Player 식별 정보인 경우 적용 거부
        if (identity.roomId == 0 || identity.playerId == 0)
        {
            return false;
        }

        // 새 식별 정보에 이전 연결의 월드 상태가 남지 않도록 초기화
        worldState_.reset();

        // 검증된 로컬 플레이어 식별 정보 저장
        playerIdentity_ = identity;

        return true;
    }

    bool ClientGameState::ApplyWorldStateSnapshot(const cna::network::WorldStateSnapshot& snapshot)
    {
        // 로컬 플레이어 식별 정보를 받기 전인 경우 스냅샷 적용 거부
        if (!playerIdentity_)
        {
            return false;
        }

        // 현재 플레이어가 입장한 Room과 스냅샷의 Room이 다른 경우 적용 거부
        if (snapshot.roomId != playerIdentity_->roomId)
        {
            return false;
        }

        // 검증된 월드 상태 스냅샷 저장
        worldState_ = snapshot;

        return true;
    }

    void ClientGameState::Reset() noexcept
    {
        // 현재 연결에 할당된 로컬 플레이어 식별 정보 제거
        playerIdentity_.reset();

        // 현재 연결에서 마지막으로 적용된 월드 상태 제거
        worldState_.reset();
    }

    bool ClientGameState::HasPlayerIdentity() const noexcept
    {
        return playerIdentity_.has_value();
    }

    bool ClientGameState::HasWorldState() const noexcept
    {
        return worldState_.has_value();
    }

    std::optional<cna::RoomId> ClientGameState::GetRoomId() const noexcept
    {
        if (!playerIdentity_)
        {
            return std::nullopt;
        }

        return playerIdentity_->roomId;
    }

    std::optional<cna::PlayerId> ClientGameState::GetLocalPlayerId() const noexcept
    {
        // 로컬 플레이어 식별 정보를 받기 전인 경우
        if (!playerIdentity_)
        {
            return std::nullopt;
        }

        return playerIdentity_->playerId;
    }

    const cna::network::WorldStateSnapshot* ClientGameState::GetWorldState() const noexcept
    {
        // 월드 상태 스냅샷이 적용되지 않은 상태인 경우
        if (!worldState_)
        {
            return nullptr;
        }

        return &(*worldState_);
    }

    const cna::network::PlayerStateSnapshot* ClientGameState::FindPlayer(const cna::PlayerId playerId) const noexcept
    {
        // 스냅샷이 적용되어 있지 않거나 player Id가 유효하지 않은 경우
        if (!worldState_ || playerId == 0)
        {
            return nullptr;
        }

        // 스냅샷에서 식별 정보의 player Id에 해당하는 로컬 플레이어 탐색
        const auto playerIterator = std::find_if
        (
            worldState_->players.cbegin(),
            worldState_->players.cend(),
            [playerId](const cna::network::PlayerStateSnapshot& player)
            {
                return player.playerId == playerId;
            }
        );

        // 조건과 일치하는 플레이어가 스냅샷에 존재하지 않는 경우
        if (playerIterator == worldState_->players.cend())
        {
            return nullptr;
        }

        return &(*playerIterator);
    }

    const cna::network::PlayerStateSnapshot* ClientGameState::GetLocalPlayer() const noexcept
    {
        // 로컬 플레이어 식별 정보를 받기 전인 경우
        if (!playerIdentity_)
        {
            return nullptr;
        }

        // 식별 정보를 기반으로 플레이어 탐색
        return FindPlayer(playerIdentity_->playerId);
    }
}
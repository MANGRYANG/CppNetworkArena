#pragma once

#include <network/messages/payloads/player_identity_message.h>
#include <network/messages/payloads/world_state_snapshot_message.h>

#include <optional>

namespace cna::client
{
    // 서버에서 전달받은 식별 정보와 월드 상태를 관리하는 클라이언트 게임 상태 클래스
    class ClientGameState final
    {
    public:
        // 서버가 할당한 로컬 플레이어 식별 정보를 적용하는 함수
        bool ApplyPlayerIdentity(const cna::network::PlayerIdentityPayload& identity);

        // 서버에서 수신한 월드 상태 스냅샷을 적용하는 함수
        bool ApplyWorldStateSnapshot(const cna::network::WorldStateSnapshot& snapshot);

        // 현재 연결에 대한 모든 게임 상태를 초기화하는 함수
        void Reset() noexcept;

        // 로컬 플레이어 식별 정보 보유 여부를 반환하는 함수
        bool HasPlayerIdentity() const noexcept;

        // 월드 상태 스냅샷 적용 여부를 반환하는 함수
        bool HasWorldState() const noexcept;

        // 현재 Room ID를 반환하는 함수
        std::optional<cna::RoomId> GetRoomId() const noexcept;

        // 현재 로컬 Player ID를 반환하는 함수
        std::optional<cna::PlayerId> GetLocalPlayerId() const noexcept;

        // 현재 적용된 월드 상태 스냅샷을 반환하는 함수
        const cna::network::WorldStateSnapshot* GetWorldState() const noexcept;

        // 현재 스냅샷에서 지정한 플레이어 상태를 탐색하는 함수
        const cna::network::PlayerStateSnapshot* FindPlayer(cna::PlayerId playerId) const noexcept;

        // 현재 스냅샷에서 로컬 플레이어 상태를 반환하는 함수
        const cna::network::PlayerStateSnapshot* GetLocalPlayer() const noexcept;

    private:
        // 서버가 현재 연결에 할당한 Room 및 로컬 Player 식별 정보
        std::optional<cna::network::PlayerIdentityPayload> playerIdentity_;

        // 서버에서 마지막으로 전달받아 적용한 월드 상태 스냅샷
        std::optional<cna::network::WorldStateSnapshot> worldState_;
    };
}
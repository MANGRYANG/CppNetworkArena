#pragma once

#include "player.h"

#include "network/session_types.h"

#include <NetworkTypes.h>
#include <network/messages/core/message_type.h>
#include <network/messages/payloads/world_state_snapshot_message.h>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <optional>
#include <unordered_map>

namespace cna::server
{
    class Session;

    // 하나의 게임 공간에 속한 플레이어 목록을 관리하는 클래스
    class Room final
    {
    public:
        explicit Room(cna::RoomId id);

        // 복사 생성자 및 복사 대입 연산자 삭제
        Room(const Room&) = delete;
        Room& operator=(const Room&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        Room(Room&&) = delete;
        Room& operator=(Room&&) = delete;

        // Room의 고유 ID 반환
        cna::RoomId GetRoomId() const noexcept;

        // 세션을 기반으로 Room에 플레이어를 입장시키고 발급한 플레이어 ID를 반환하는 함수
        std::optional<cna::PlayerId> Enter(std::shared_ptr<Session> session);

        // 세션 ID에 해당하는 플레이어를 Room에서 퇴장시키는 함수
        void Leave(SessionId sessionId);

        // 특정 플레이어에게 입력을 적용하는 함수
        bool ApplyPlayerInput(SessionId sessionId, const cna::network::PlayerInputPayload& input);

        // 현재 Room의 게임 상태를 지정한 시간만큼 진행하는 함수
        void Tick(float deltaSeconds);

        // 현재 Room의 게임 상태 스냅샷을 생성하는 함수
        cna::network::WorldStateSnapshot CaptureSnapshot(cna::ServerTick serverTick) const;

        // Room에 등록된 모든 활성 세션에게 메시지를 전송하는 함수
        void Broadcast(cna::network::MessageType type, std::span<const std::byte> payload);

        // 현재 Room에 남아 있는 플레이어 수를 반환
        std::size_t GetPlayerCount() const noexcept;

        // 현재 Room에 새로운 플레이어가 입장할 공간이 있는지 반환
        bool HasCapacity() const noexcept;

    private:
        // 하나의 Room에 입장할 수 있는 최대 플레이어 수
        static constexpr std::size_t MaxPlayerCount = 2;

        // Room 내부에서 사용할 새로운 플레이어 ID를 발급하는 함수
        std::optional<cna::PlayerId> GeneratePlayerId() noexcept;

        // 현재 Room의 두 플레이어가 서로 충돌하고 있는지 반환하는 함수
        static bool ArePlayersColliding(const PlayerState& firstPlayerState, const PlayerState& secondPlayerState) noexcept;

        // 플레이어의 위치를 스폰 위치로 이동시키는 함수
        static void ResetPlayerPosition(PlayerState& playerState) noexcept;

        // 특정 진영에 속한 플레이어가 현재 Room에 존재하는지 반환하는 함수
        bool HasPlayerOnSide(cna::PlayerSide side) const noexcept;

        // 게임을 진행할 수 있는 진영 구성이 갖춰졌는지 반환하는 함수
        bool IsAttackDefenseReady() const noexcept;

        // 게임을 시작하는 함수
        void StartGame() noexcept;

        // 게임을 중지하는 함수
        void StopGame() noexcept;

        // 경과 시간을 기준으로 공격/수비 상태를 갱신하는 함수
        void UpdateAttackDefenseState(float deltaSeconds) noexcept;

        // 현재 공격 진영을 반대 진영으로 교체하는 함수
        void SwitchAttackingSide() noexcept;

        // 플레이어 진영의 인덱스를 반환하는 함수
        static std::optional<std::size_t> GetPlayerSideIndex(cna::PlayerSide side) noexcept;

        // 서버에서 Room을 구분하기 위해 사용할 고유 ID
        cna::RoomId roomId_ = 0;

        // 다음 플레이어에게 발급할 Room 내부 플레이어 ID
        cna::PlayerId nextPlayerId_ = 1;

        // 현재 공격 역할을 가진 플레이어 진영
        cna::PlayerSide attackingSide_ = cna::PlayerSide::None;

        // 다음 공격 진영 교대까지 남은 시간
        float attackRoleSwitchRemainingSeconds_ = 0.0f;

        // 각 진영별 현재 점수를 보관하기 위한 배열
        std::array<std::size_t, 2> sideScores_{};

        // Room에 입장한 플레이어를 관리하기 위한 컨테이너
        std::unordered_map<SessionId, Player> players_;
    };
}
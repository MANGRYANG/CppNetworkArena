#pragma once

#include "player.h"

#include "../network/session_types.h"

#include <NetworkTypes.h>
#include <network/messages/core/message_type.h>
#include <network/messages/payloads/world_state_snapshot_message.h>

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

    private:
        // Room 내부에서 사용할 새로운 플레이어 ID를 발급하는 함수
        std::optional<cna::PlayerId> GeneratePlayerId() noexcept;

        // 서버에서 Room을 구분하기 위해 사용할 고유 ID
        cna::RoomId roomId_ = 0;

        // 다음 플레이어에게 발급할 Room 내부 플레이어 ID
        cna::PlayerId nextPlayerId_ = 1;

        // Room에 입장한 플레이어를 관리하기 위한 컨테이너
        std::unordered_map<SessionId, Player> players_;
    };
}
#pragma once

#include "player.h"

#include "../network/session_types.h"

#include <NetworkTypes.h>
#include <network/message_type.h>

#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>

namespace cna::server
{
    class Session;

    // 하나의 게임 공간에 속한 세션 목록을 관리하는 클래스
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
        cna::RoomId GetId() const noexcept;

        // 세션을 기반으로 Room에 플레이어를 입장시키는 함수
        bool Enter(std::shared_ptr<Session> session);

        // 세션 ID에 해당하는 플레이어를 Room에서 퇴장시키는 함수
        void Leave(SessionId sessionId);

        // Room에 등록된 모든 활성 세션에게 메시지를 전송하는 함수
        void Broadcast(cna::network::MessageType type, std::span<const std::byte> payload);

        // 현재 Room에 남아 있는 플레이어 수를 반환
        std::size_t GetPlayerCount() const noexcept;

    private:
        // 서버에서 Room을 구분하기 위해 사용할 고유 ID
        cna::RoomId id_ = 0;

        // Room에 입장한 플레이어를 관리하기 위한 컨테이너
        std::unordered_map<SessionId, Player> players_;
    };
}
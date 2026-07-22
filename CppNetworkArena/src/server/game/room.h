#pragma once

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

        // Room에 세션을 등록하는 함수
        void AddSession(std::shared_ptr<Session> session);

        // Room에서 특정 세션을 제거하는 함수
        void RemoveSession(SessionId sessionId);

        // Room에 등록된 모든 활성 세션에게 메시지를 전송하는 함수
        void Broadcast(cna::network::MessageType type, std::span<const std::byte> payload);

        // 현재 Room에 남아 있는 활성 세션 수를 반환
        std::size_t GetSessionCount() const noexcept;

    private:
        // 서버에서 Room을 구분하기 위해 사용할 고유 ID
        cna::RoomId id_ = 0;

        // Room에 소속된 세션을 관리하기 위한 컨테이너
        std::unordered_map<SessionId, std::weak_ptr<Session>> sessions_;
    };
}
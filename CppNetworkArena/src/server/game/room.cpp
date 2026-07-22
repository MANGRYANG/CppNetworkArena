#include "room.h"

#include "../network/session.h"

#include <iostream>
#include <utility>

namespace cna::server
{
    Room::Room(const cna::RoomId id) : id_(id)
    {
    }

    cna::RoomId Room::GetId() const noexcept
    {
        return id_;
    }

    void Room::AddSession(std::shared_ptr<Session> session)
    {
        // 유효하지 않은 세션인 경우 등록하지 않음
        if (!session)
        {
            return;
        }

        const SessionId sessionId = session->GetId();

        // Room에서 관리하는 세션 목록에 세션 등록
        sessions_[sessionId] = std::move(session);

        // 현재 Room에 등록된 세션 개수 출력
        std::cout
            << "[Room] Session added: roomId=" << id_
            << ", sessionId=" << sessionId
            << ", active=" << GetSessionCount() << '\n';
    }

    void Room::RemoveSession(const SessionId sessionId)
    {
        // Room에 등록된 세션 목록에서 세션 제거
        const std::size_t removedCount = sessions_.erase(sessionId);

        // 제거할 세션이 목록에 없어 제거하지 못한 경우
        if (removedCount == 0)
        {
            return;
        }

        // 현재 Room에 등록된 세션 개수 출력
        std::cout
            << "[Room] Session removed: roomId=" << id_
            << ", sessionId=" << sessionId
            << ", active=" << GetSessionCount() << '\n';
    }

    void Room::Broadcast(const cna::network::MessageType type, const std::span<const std::byte> payload)
    {
        // Room에 등록된 세션 목록을 순회
        auto sessionIterator = sessions_.begin();

        while (sessionIterator != sessions_.end())
        {
            // 약한 참조에서 실제 세션 객체 획득 시도
            const std::shared_ptr<Session> session = sessionIterator->second.lock();

            // 이미 만료된 세션인 경우 Room 목록에서 제거
            if (!session)
            {
                sessionIterator = sessions_.erase(sessionIterator);

                continue;
            }

            // 활성 세션에게 메시지 전송
            session->Send(type, payload);

            ++sessionIterator;
        }
    }

    std::size_t Room::GetSessionCount() const noexcept
    {
        std::size_t activeSessionCount = 0;

        // 세션이 유효한 경우 활성 세션 카운트에 포함
        for (const auto& sessionEntry : sessions_)
        {
            if (!sessionEntry.second.expired())
            {
                ++activeSessionCount;
            }
        }

        return activeSessionCount;
    }
}
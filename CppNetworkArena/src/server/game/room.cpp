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

    bool Room::Enter(std::shared_ptr<Session> session)
    {
        // 유효하지 않은 세션인 경우 입장시키지 않음
        if (!session)
        {
            return false;
        }

        const SessionId sessionId = session->GetId();

        // 유효하지 않은 세션 ID인 경우 입장시키지 않음
        if (sessionId == 0)
        {
            return false;
        }

        // 세션을 기반으로 Room 안에서 사용할 Player 생성
        Player player(session);

        // Room에서 관리하는 플레이어 목록에 등록
        const auto [playerIterator, inserted] = players_.emplace(sessionId, std::move(player));

        // 이미 같은 세션 ID를 가진 플레이어가 존재하는 경우 입장 실패 처리
        if (!inserted)
        {
            return false;
        }

        // 현재 Room에 입장한 플레이어 수 출력
        std::cout
            << "[Room] Player entered: roomId=" << id_
            << ", sessionId=" << playerIterator->second.GetSessionId()
            << ", activePlayers=" << GetPlayerCount() << '\n';

        return true;
    }

    void Room::Leave(const SessionId sessionId)
    {
        // Room에 입장한 플레이어 목록에서 플레이어 제거
        const std::size_t removedCount = players_.erase(sessionId);

        // 제거할 플레이어가 없는 경우
        if (removedCount == 0)
        {
            return;
        }

        // 현재 Room에 입장한 플레이어 수 출력
        std::cout
            << "[Room] Player left: roomId=" << id_
            << ", sessionId=" << sessionId
            << ", activePlayers=" << GetPlayerCount() << '\n';
    }

    void Room::Broadcast(const cna::network::MessageType type, const std::span<const std::byte> payload)
    {
        // Room에 등록된 플레이어 목록을 순회
        auto playerIterator = players_.begin();

        while (playerIterator != players_.end())
        {
            // 플레이어가 참조하는 실제 세션 객체 획득 시도
            const std::shared_ptr<Session> session = playerIterator->second.LockSession();

            // 이미 만료된 세션인 경우 해당 플레이어를 Room에서 제거
            if (!session)
            {
                playerIterator = players_.erase(playerIterator);

                continue;
            }

            // 활성 세션에게 메시지 전송
            session->Send(type, payload);

            ++playerIterator;
        }
    }

    std::size_t Room::GetPlayerCount() const noexcept
    {
        return players_.size();
    }
}
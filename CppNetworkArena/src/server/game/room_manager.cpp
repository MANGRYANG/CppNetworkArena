#include "room_manager.h"

#include "room.h"

#include <iostream>
#include <memory>
#include <utility>

namespace cna::server
{
    std::shared_ptr<Room> RoomManager::CreateRoom()
    {
        // 새 Room에 부여할 고유 ID 생성
        const cna::RoomId roomId = GenerateRoomId();

        // Room 객체 생성
        std::shared_ptr<Room> room = std::make_shared<Room>(roomId);

        // 생성된 Room을 활성 Room 목록에 등록
        rooms_.emplace(roomId, room);

        // Room 생성 메시지 출력
        std::cout
            << "[RoomManager] Room created: roomId=" << roomId
            << ", activeRooms=" << Count() << '\n';

        return room;
    }

    std::shared_ptr<Room> RoomManager::FindRoom(const cna::RoomId roomId) const
    {
        // 요청한 ID에 해당하는 Room 검색
        const auto roomIterator = rooms_.find(roomId);

        // 해당 Room이 없는 경우 빈 shared_ptr 반환
        if (roomIterator == rooms_.end())
        {
            return nullptr;
        }

        // 요청한 ID에 해당하는 Room의 shared_ptr 반환
        return roomIterator->second;
    }

    void RoomManager::RemoveRoom(const cna::RoomId roomId)
    {
        // Room 목록에서 요청한 ID의 Room 제거
        const std::size_t removedCount = rooms_.erase(roomId);

        // 제거할 Room이 목록에 없는 경우
        if (removedCount == 0)
        {
            return;
        }

        // Room 제거 메시지 출력
        std::cout
            << "[RoomManager] Room removed: roomId=" << roomId
            << ", activeRooms=" << Count() << '\n';
    }

    std::size_t RoomManager::Count() const noexcept
    {
        return rooms_.size();
    }

    cna::RoomId RoomManager::GenerateRoomId() noexcept
    {
        return nextRoomId_++;
    }
}
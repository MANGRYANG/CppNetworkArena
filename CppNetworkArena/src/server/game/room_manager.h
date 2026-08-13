#pragma once

#include <NetworkTypes.h>

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace cna::server
{
    class Room;

    // 서버에서 생성된 Room 객체들을 관리하는 모듈
    class RoomManager final
    {
    public:
        RoomManager() = default;

        // 복사 생성자 및 복사 대입 연산자 삭제
        RoomManager(const RoomManager&) = delete;
        RoomManager& operator=(const RoomManager&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        RoomManager(RoomManager&&) = delete;
        RoomManager& operator=(RoomManager&&) = delete;

        // 새로운 Room을 생성하고 Room 목록에 등록하는 함수
        std::shared_ptr<Room> CreateRoom();

        // 특정 ID의 Room을 조회하는 함수
        std::shared_ptr<Room> FindRoom(cna::RoomId roomId) const;

        // 특정 Room을 목록에서 제거하는 함수
        void RemoveRoom(cna::RoomId roomId);

        // 현재 관리 중인 Room 개수를 반환하는 함수
        std::size_t Count() const noexcept;

    private:
        // 새 Room에 부여할 고유 ID를 생성하는 함수
        cna::RoomId GenerateRoomId() noexcept;

        // 다음에 생성할 Room에 부여할 ID
        cna::RoomId nextRoomId_ = 1;

        // 현재 서버에서 관리 중인 Room 목록
        std::unordered_map<cna::RoomId, std::shared_ptr<Room>> rooms_;
    };
}
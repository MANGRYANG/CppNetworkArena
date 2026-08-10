#include "room.h"

#include "../network/session.h"

#include <network/messages/payloads/player_input_message.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <utility>

namespace
{
    // 플레이어가 최대 세기의 입력으로 이동할 때의 초당 이동 속도
    constexpr float PlayerMoveSpeed = 5.0f;

    // 서버에서의 플레이어 이동을 위해 계산된 값을 담을 구조체
    struct NormalizedPlayerInput
    {
        float moveX = 0.0f;
        float moveY = 0.0f;
        float moveZ = 0.0f;
    };

    // PlayerInput 타입 메시지로 전달받은 원시 세기 값을 -1.0f ~ 1.0f 범위의 이동 입력 벡터로 변환하는 함수
    NormalizedPlayerInput NormalizePlayerInput(const cna::network::PlayerInputPayload& input)
    {
        // 축 별 세기를 -1.0f ~ 1.0f 범위로 정규화
        NormalizedPlayerInput normalizedInput
        {
            static_cast<float>(input.moveX) / static_cast<float>(cna::network::MaxPlayerInputAxisRawValue),
            static_cast<float>(input.moveY) / static_cast<float>(cna::network::MaxPlayerInputAxisRawValue),
            static_cast<float>(input.moveZ) / static_cast<float>(cna::network::MaxPlayerInputAxisRawValue)
        };

        // 정규화된 입력 방향 벡터의 길이 제곱
        const float lengthSquared =
            normalizedInput.moveX * normalizedInput.moveX +
            normalizedInput.moveY * normalizedInput.moveY +
            normalizedInput.moveZ * normalizedInput.moveZ;

        // 방향 벡터의 길이가 1.0f를 넘는 경우 방향 벡터의 길이로 나누어 단위벡터화
        if (lengthSquared > 1.0f)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);

            normalizedInput.moveX *= inverseLength;
            normalizedInput.moveY *= inverseLength;
            normalizedInput.moveZ *= inverseLength;
        }

        return normalizedInput;
    }
}

namespace cna::server
{
    Room::Room(const cna::RoomId id) : roomId_(id)
    {
    }

    cna::RoomId Room::GetRoomId() const noexcept
    {
        return roomId_;
    }

    std::optional<cna::PlayerId> Room::Enter(std::shared_ptr<Session> session)
    {
        // 유효하지 않은 세션인 경우 입장시키지 않음
        if (!session)
        {
            return std::nullopt;
        }

        const SessionId sessionId = session->GetId();

        // 유효하지 않은 세션 ID인 경우 입장시키지 않음
        if (sessionId == 0)
        {
            return std::nullopt;
        }

        // 플레이어 ID 발급
        const std::optional<cna::PlayerId> playerId = GeneratePlayerId();

        // 플레이어 ID 공간을 모두 소진한 경우 입장 실패 처리
        if (!playerId)
        {
            return std::nullopt;
        }

        // Room에서 관리하는 플레이어 목록에 등록
        const auto [playerIterator, inserted] = players_.try_emplace(sessionId, *playerId, session);

        // 이미 같은 세션 ID를 가진 플레이어가 존재하는 경우 입장 실패 처리
        if (!inserted)
        {
            return std::nullopt;
        }

        // 현재 Room에 입장한 플레이어 수 출력
        std::cout
            << "[Room] Player entered: roomId=" << roomId_
            << ", playerId=" << playerIterator->second.GetPlayerId()
            << ", activePlayers=" << GetPlayerCount() << '\n';

        return playerId;
    }

    void Room::Leave(const SessionId sessionId)
    {
        // Room에서 퇴장할 플레이어 탐색
        const auto playerIterator = players_.find(sessionId);

        // 세션 ID를 통해 퇴장할 플레이어를 찾지 못한 경우
        if (playerIterator == players_.end())
        {
            return;
        }

        // 제거할 플레이어의 플레이어 ID 추출
        const cna::PlayerId playerId = playerIterator->second.GetPlayerId();

        // Room에 입장한 플레이어 목록에서 플레이어 제거
        players_.erase(playerIterator);

        // 현재 Room에 입장한 플레이어 수 출력
        std::cout
            << "[Room] Player left: roomId=" << roomId_
            << ", playerId=" << playerId
            << ", activePlayers=" << GetPlayerCount() << '\n';
    }

    bool Room::ApplyPlayerInput(SessionId sessionId, const cna::network::PlayerInputPayload& input)
    {
        // 세션 ID에 해당하는 플레이어 검색
        const auto playerIterator = players_.find(sessionId);

        // 입력을 적용할 플레이어가 없는 경우 실패 처리
        if (playerIterator == players_.end())
        {
            return false;
        }

        // 전달된 원시 입력 세기 값을 서버에서 사용할 이동 입력 벡터로 정규화
        const NormalizedPlayerInput normalizedInput = NormalizePlayerInput(input);

        // 플레이어 상태를 수정 가능하도록 참조
        PlayerState& state = playerIterator->second.GetState();

        // 이동 입력 벡터에 최대 세기의 입력으로 이동할 때의 초당 이동 속도를 곱해 플레이어 속도 갱신
        state.velocityX = normalizedInput.moveX * PlayerMoveSpeed;
        state.velocityY = normalizedInput.moveY * PlayerMoveSpeed;
        state.velocityZ = normalizedInput.moveZ * PlayerMoveSpeed;

        return true;
    }

    void Room::Tick(float deltaSeconds)
    {
        // 유효하지 않은 시간 값이면 위치를 갱신하지 않음
        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        // Room에 등록된 모든 플레이어의 위치를 현재 플레이어 속도 값 기준으로 갱신
        for (auto& playerEntry : players_)
        {
            Player& player = playerEntry.second;
            PlayerState& state = player.GetState();

            state.positionX += state.velocityX * deltaSeconds;
            state.positionY += state.velocityY * deltaSeconds;
            state.positionZ += state.velocityZ * deltaSeconds;
        }
    }

    cna::network::WorldStateSnapshot Room::CaptureSnapshot() const
    {
        // 현재 Room의 게임 상태 스냅샷
        cna::network::WorldStateSnapshot snapshot;
        snapshot.roomId = roomId_;
        snapshot.players.reserve(players_.size());

        // Room에 등록된 모든 플레이어의 현재 상태를 스냅샷에 추가
        for (const auto& playerEntry : players_)
        {
            const Player& player = playerEntry.second;
            const PlayerState& state = player.GetState();

            snapshot.players.push_back
            (
                cna::network::PlayerStateSnapshot
                {
                    player.GetPlayerId(),
                    state.positionX,
                    state.positionY,
                    state.positionZ,
                    state.velocityX,
                    state.velocityY,
                    state.velocityZ
                }
            );
        }

        // unordered_map 순회 순서와 무관하게 플레이어 ID 기준으로 정렬
        std::sort
        (
            snapshot.players.begin(),
            snapshot.players.end(),
            [](const auto& left, const auto& right)
            {
                return left.playerId < right.playerId;
            }
        );

        return snapshot;
    }

    void Room::Broadcast(const cna::network::MessageType type, const std::span<const std::byte> payload)
    {
        // 송신 실패 세션을 순회가 끝난 뒤 종료하도록 설정하여 반복자 무효화 방지
        std::vector<std::shared_ptr<Session>> failedSessions;
        failedSessions.reserve(players_.size());

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
            if (!session->Send(type, payload))
            {
                // 전송 실패 시 송신 실패 세션 목록에 추가
                failedSessions.push_back(session);
            }

            ++playerIterator;
        }

        // 순회 완료 후 송신 실패 세션 일괄 종료
        for (const std::shared_ptr<Session>& session : failedSessions)
        {
            session->Stop();
        }
    }

    std::size_t Room::GetPlayerCount() const noexcept
    {
        return players_.size();
    }

    std::optional<cna::PlayerId> Room::GeneratePlayerId() noexcept
    {
        // ID 공간을 모두 소진한 상태인 경우
        if (nextPlayerId_ == 0)
        {
            return std::nullopt;
        }

        const cna::PlayerId playerId = nextPlayerId_;

        // 마지막 유효 ID 발급 후 값을 0을 유지
        if (nextPlayerId_ == std::numeric_limits<cna::PlayerId>::max())
        {
            nextPlayerId_ = 0;
        }
        else
        {
            ++nextPlayerId_;
        }

        return playerId;
    }
}
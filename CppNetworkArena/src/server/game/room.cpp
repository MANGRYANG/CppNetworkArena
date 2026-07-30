#include "room.h"

#include "../network/session.h"

#include <network/messages/payloads/player_input_message.h>

#include <cmath>
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

        // 갱신한 플레이어의 축 별 속도 값을 로그로 출력
        std::cout
            << "[Room] Player velocity updated: roomId=" << id_
            << ", sessionId=" << sessionId
            << ", velocityX=" << state.velocityX
            << ", velocityY=" << state.velocityY
            << ", velocityZ=" << state.velocityZ
            << '\n';

        return true;
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
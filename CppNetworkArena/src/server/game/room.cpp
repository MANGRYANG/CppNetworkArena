#include "room.h"

#include "../network/session.h"

#include <network/messages/payloads/player_input_message.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <iterator>
#include <utility>

namespace
{
    // 플레이어가 최대 세기의 입력으로 이동할 때의 초당 이동 속도
    constexpr float PlayerMoveSpeed = 5.0f;

    // 공격 진영과 수비 진영이 교체되는 시간 간격
    constexpr float AttackRoleSwitchIntervalSeconds = 3.0f;

    // 진영별 점수 배열에서 사용하는 인덱스
    constexpr std::size_t FlameSideIndex = 0;
    constexpr std::size_t FrostSideIndex = 1;

    // XY 게임 평면에서 사용하는 아레나 게임 공간 경계
    constexpr float ArenaMinX = -3.0f;
    constexpr float ArenaMaxX = 3.0f;
    constexpr float ArenaMinY = -2.75f;
    constexpr float ArenaMaxY = 2.75f;

    // 플레이어 모델이 아레나 맵 경계를 넘어가지 않도록 적용할 여유 범위
    constexpr float PlayerBoundaryMargin = 0.15f;

    // 플레이어 간 충돌 판정에 사용할 원형 충돌 영역의 반지름
    constexpr float PlayerCollisionRadius = 0.15f;

    // 서버에서의 플레이어 이동을 위해 계산된 값을 담을 구조체
    struct NormalizedPlayerInput
    {
        float moveX = 0.0f;
        float moveY = 0.0f;
        float moveZ = 0.0f;
    };

    struct SpawnPosition
    {
        float x;
        float y;
        float z;
    };

    constexpr std::array<SpawnPosition, 2> PlayerSpawnPositions =
    {
        SpawnPosition{-1.5f, 0.0f, 0.0f},
        SpawnPosition{ 1.5f, 0.0f, 0.0f}
    };

    // PlayerInput 타입 메시지로 전달받은 원시 세기 값을 -1.0f ~ 1.0f 범위의 이동 입력 벡터로 변환하는 함수
    NormalizedPlayerInput NormalizePlayerInput(const cna::network::PlayerInputPayload& input)
    {
        // 축 별 세기를 -1.0f ~ 1.0f 범위로 정규화
        NormalizedPlayerInput normalizedInput
        {
            static_cast<float>(input.moveX) / static_cast<float>(cna::network::MaxPlayerInputAxisRawValue),
            static_cast<float>(input.moveY) / static_cast<float>(cna::network::MaxPlayerInputAxisRawValue),
            0
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

        // 새로운 플레이어가 입장할 공간이 없는 경우 입장시키지 않음
        if (!HasCapacity())
        {
            std::cout
                << "[Room] Player entry rejected: roomId=" << roomId_
                << ", activePlayers=" << GetPlayerCount()
                << ", maxPlayers=" << MaxPlayerCount
                << '\n';

            return std::nullopt;
        }

        bool flameSideOccupied = false;
        bool frostSideOccupied = false;

        // 현재 Room에서 사용 중인 플레이어 진영 확인
        for (const auto& [existingSessionId, existingPlayer] : players_)
        {
            const PlayerState& existingPlayerState = existingPlayer.GetState();

            if (existingPlayerState.side == cna::PlayerSide::Flame)
            {
                flameSideOccupied = true;
            }
            else if (existingPlayerState.side == cna::PlayerSide::Frost)
            {
                frostSideOccupied = true;
            }
        }

        // 비어 있는 진영을 새로운 플레이어에게 할당
        cna::PlayerSide playerSide = cna::PlayerSide::None;

        if (!flameSideOccupied)
        {
            playerSide = cna::PlayerSide::Flame;
        }
        else if (!frostSideOccupied)
        {
            playerSide = cna::PlayerSide::Frost;
        }

        // 유효한 진영을 할당할 수 없는 경우 입장 실패 처리
        if (playerSide == cna::PlayerSide::None)
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

        PlayerState& playerState = playerIterator->second.GetState();

        // Room에서 할당한 플레이어 진영 적용
        playerState.side = playerSide;

        // 할당된 진영에 해당하는 시작 위치 적용
        ResetPlayerPosition(playerState);

        // 게임을 진행할 수 있는 진영 구성이 갖춰져 있고 게임이 중지된 상태인 경우 게임 시작
        if (attackingSide_ == cna::PlayerSide::None && IsAttackDefenseReady())
        {
            StartGame();
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

        // 게임을 진행할 수 없는 진영 구성이 된 경우 게임 중지
        if (!IsAttackDefenseReady())
        {
            StopGame();
        }

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

        // 게임이 진행 중인 경우
        if (attackingSide_ != cna::PlayerSide::None)
        {
            // Room에 등록된 모든 플레이어의 위치를 현재 플레이어 속도 값 기준으로 갱신
            for (auto& playerEntry : players_)
            {
                Player& player = playerEntry.second;
                PlayerState& state = player.GetState();

                state.positionX += state.velocityX * deltaSeconds;
                state.positionY += state.velocityY * deltaSeconds;

                // 플레이어 메쉬가 아레나 경계를 넘어가지 않도록 위치 제한
                state.positionX =
                    std::clamp
                    (
                        state.positionX,
                        ArenaMinX + PlayerBoundaryMargin,
                        ArenaMaxX - PlayerBoundaryMargin
                    );

                state.positionY =
                    std::clamp
                    (
                        state.positionY,
                        ArenaMinY + PlayerBoundaryMargin,
                        ArenaMaxY - PlayerBoundaryMargin
                    );

                // 현재 게임은 XY 플레이 평면만 사용하므로 Z축 위치 고정
                state.positionZ = 0.0f;
            }

            // Room에 등록된 플레이어의 모든 조합에 대해 충돌 검사
            for (auto firstPlayerIterator = players_.begin(); firstPlayerIterator != players_.end(); ++firstPlayerIterator)
            {
                for (auto secondPlayerIterator = std::next(firstPlayerIterator); secondPlayerIterator != players_.end(); ++secondPlayerIterator)
                {
                    PlayerState& firstPlayerState = firstPlayerIterator->second.GetState();
                    PlayerState& secondPlayerState = secondPlayerIterator->second.GetState();

                    // 두 플레이어가 충돌하지 않은 경우 다음 조합 검사
                    if (!ArePlayersColliding(firstPlayerState, secondPlayerState))
                    {
                        continue;
                    }

                    // 현재 공격 역할이 할당된 진영의 인덱스
                    const std::optional<std::size_t> attackingSideIndex = GetPlayerSideIndex(attackingSide_);

                    if (attackingSideIndex)
                    {
                        // 현재 공격 역할이 할당된 진영에 득점 처리
                        ++sideScores_[*attackingSideIndex];

                        std::cout
                            << "[Room] Score updated"
                            << ": roomId=" << roomId_
                            << ", flameScore=" << sideScores_[FlameSideIndex]
                            << ", frostScore=" << sideScores_[FrostSideIndex]
                            << '\n';
                    }

                    // 충돌한 두 플레이어를 각자의 시작 위치로 재배치
                    ResetPlayerPosition(firstPlayerState);
                    ResetPlayerPosition(secondPlayerState);
                }
            }

            UpdateAttackDefenseState(deltaSeconds);
        }
    }

    cna::network::WorldStateSnapshot Room::CaptureSnapshot(cna::ServerTick serverTick) const
    {
        // 현재 Room의 게임 상태 스냅샷
        cna::network::WorldStateSnapshot snapshot;
        snapshot.serverTick = serverTick;
        snapshot.roomId = roomId_;
        snapshot.players.reserve(players_.size());

        // 현재 게임 진행 여부
        const bool gameRunning = attackingSide_ != cna::PlayerSide::None;

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
                    state.side,
                    state.positionX,
                    state.positionY,
                    state.positionZ,
                    gameRunning ? state.velocityX : 0.0f,
                    gameRunning ? state.velocityY : 0.0f,
                    gameRunning ? state.velocityZ : 0.0f
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
        // 만료된 플레이어를 순회가 끝난 뒤 퇴장 처리하도록 설정하여 반복자 무효화 방지
        std::vector<SessionId> expiredSessionIds;
        expiredSessionIds.reserve(players_.size());

        // 송신 실패 세션을 순회가 끝난 뒤 종료하도록 설정하여 반복자 무효화 방지
        std::vector<std::shared_ptr<Session>> failedSessions;
        failedSessions.reserve(players_.size());

        // Room에 등록된 플레이어 목록을 순회
        for (auto playerIterator = players_.begin(); playerIterator != players_.end(); ++playerIterator)
        {
            // 플레이어가 참조하는 실제 세션 객체 획득 시도
            const std::shared_ptr<Session> session = playerIterator->second.LockSession();

            // 이미 만료된 세션인 경우 해당 플레이어를 Room에서 퇴장 처리
            if (!session)
            {
                // 해당 플레이어를 순회 완료 후 퇴장 처리할 수 있게 기록
                expiredSessionIds.push_back(playerIterator->first);

                continue;
            }

            // 활성 세션에게 메시지 전송
            if (!session->Send(type, payload))
            {
                // 전송 실패 시 송신 실패 세션 목록에 추가
                failedSessions.push_back(session);
            }
        }

        // 순회 완료 후 만료된 세션에 해당하는 플레이어 퇴장 처리
        for (const SessionId sessionId : expiredSessionIds)
        {
            Leave(sessionId);
        }

        // 순회 완료 후 송신 실패 세션에 해당하는 플레이어 퇴장 처리 및 세션 일괄 종료
        for (const std::shared_ptr<Session>& session : failedSessions)
        {
            Leave(session->GetId());
            session->Stop();
        }
    }

    std::size_t Room::GetPlayerCount() const noexcept
    {
        return players_.size();
    }

    bool Room::HasCapacity() const noexcept
    {
        return GetPlayerCount() < MaxPlayerCount;
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

    bool Room::ArePlayersColliding(const PlayerState& firstPlayerState, const PlayerState& secondPlayerState) noexcept
    {
        // XY 플레이 평면에서 두 플레이어 중심 사이의 거리 벡터 계산
        const float deltaX = secondPlayerState.positionX - firstPlayerState.positionX;
        const float deltaY = secondPlayerState.positionY - firstPlayerState.positionY;

        // 플레이어 사이 거리 제곱 계산
        const float distanceSquared = deltaX * deltaX + deltaY * deltaY;

        // 두 플레이어의 원형 충돌 영역이 닿거나 겹친 경우 충돌로 판정
        return distanceSquared <= PlayerCollisionRadius * PlayerCollisionRadius * 4.0f;
    }

    void Room::ResetPlayerPosition(PlayerState& playerState) noexcept
    {
        const SpawnPosition* spawnPosition = nullptr;

        // 플레이어 진영에 해당하는 시작 위치 선택
        if (playerState.side == cna::PlayerSide::Flame)
        {
            spawnPosition = &PlayerSpawnPositions[0];
        }
        else if (playerState.side == cna::PlayerSide::Frost)
        {
            spawnPosition = &PlayerSpawnPositions[1];
        }

        // 유효한 진영에 해당하는 시작 위치가 없는 경우 위치를 변경하지 않음
        if (!spawnPosition)
        {
            return;
        }

        // 플레이어 위치를 스폰 위치로 이동
        playerState.positionX = spawnPosition->x;
        playerState.positionY = spawnPosition->y;
        playerState.positionZ = spawnPosition->z;
    }

    bool Room::HasPlayerOnSide(cna::PlayerSide side) const noexcept
    {
        // 지정한 진영에 속한 플레이어 탐색
        for (const auto& playerEntry : players_)
        {
            // 플레이어 탐색에 성공한 경우 true 반환
            if (playerEntry.second.GetState().side == side)
            {
                return true;
            }
        }

        // 플레이어 탐색에 실패한 경우 false 반환
        return false;
    }

    bool Room::IsAttackDefenseReady() const noexcept
    {
        // Flame과 Frost 양쪽 진영에 플레이어가 존재해야 게임 가능
        return HasPlayerOnSide(cna::PlayerSide::Flame) && HasPlayerOnSide(cna::PlayerSide::Frost);
    }

    void Room::StartGame() noexcept
    {
        // 득점 배열 초기화
        sideScores_ = {};

        // 최초 공격 진영은 Flame으로 지정
        attackingSide_ = cna::PlayerSide::Flame;

        // 최초 공격 진영의 유지 시간 설정
        attackRoleSwitchRemainingSeconds_ = AttackRoleSwitchIntervalSeconds;
    }

    void Room::StopGame() noexcept
    {
        // 현재 공격 진영 제거
        attackingSide_ = cna::PlayerSide::None;

        // 공격 진영 교대 타이머 초기화
        attackRoleSwitchRemainingSeconds_ = 0.0f;
    }

    void Room::UpdateAttackDefenseState(float deltaSeconds) noexcept
    {
        // 다음 공격 진영 교대까지 남은 시간 카운팅
        attackRoleSwitchRemainingSeconds_ -= deltaSeconds;

        // 하나의 Tick에서 교대 시점을 지난 만큼 공격 진영 교대
        while (attackRoleSwitchRemainingSeconds_ <= 0.0f)
        {
            SwitchAttackingSide();

            attackRoleSwitchRemainingSeconds_ += AttackRoleSwitchIntervalSeconds;
        }
    }

    void Room::SwitchAttackingSide() noexcept
    {
        // Flame이 공격 중인 경우 Frost로 공격 역할 교대
        if (attackingSide_ == cna::PlayerSide::Flame)
        {
            attackingSide_ = cna::PlayerSide::Frost;

            std::cout
                << "[Room] Attacking side switched"
                << ": roomId=" << roomId_
                << ", attackingSide=Frost"
                << '\n';

            return;
        }

        // Frost가 공격 중인 경우 Flame으로 공격 역할 교대
        if (attackingSide_ == cna::PlayerSide::Frost)
        {
            attackingSide_ = cna::PlayerSide::Flame;

            std::cout
                << "[Room] Attacking side switched"
                << ": roomId=" << roomId_
                << ", attackingSide=Flame"
                << '\n';
        }
    }

    std::optional<std::size_t> Room::GetPlayerSideIndex(cna::PlayerSide side) noexcept
    {
        // Flame 진영인 경우
        if (side == cna::PlayerSide::Flame)
        {
            return FlameSideIndex;
        }

        // Frost 진영인 경우
        if (side == cna::PlayerSide::Frost)
        {
            return FrostSideIndex;
        }

        return std::nullopt;
    }
}
#include "room.h"

#include "../network/session.h"

#include <network/messages/payloads/player_input_message.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <iterator>
#include <utility>
#include <vector>

namespace
{
    // 플레이어가 최대 세기의 입력으로 이동할 때의 초당 이동 속도
    constexpr float PlayerMoveSpeed = 5.0f;

    // Dash 사용 중인 플레이어의 초당 이동 속도
    constexpr float PlayerDashSpeed = 15.0f;

    // 한 번의 Dash가 유지되는 시간
    constexpr float PlayerDashDurationSeconds = 0.16f;

    // Dash 사용 후 다음 Dash까지 필요한 대기 시간
    constexpr float PlayerDashCooldownSeconds = 1.0f;

    // 공격 진영과 수비 진영이 교체되는 시간 간격
    constexpr float AttackRoleSwitchIntervalSeconds = 3.0f;

    // 게임이 진행되는 전체 시간
    constexpr float GameDurationSeconds = 30.0f;

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

    // 등속 이동하는 플레이어의 초기 위치와 초당 속도를 저장할 구조체
    struct PlayerMovement
    {
        float startX = 0.0f;
        float startY = 0.0f;

        float velocityX = 0.0f;
        float velocityY = 0.0f;
    };

    // 한 Tick 내에서 감지된 플레이어 충돌 정보를 저장할 구조체
    struct PlayerCollision
    {
        float timeInTick = 0.0f;

        cna::server::PlayerState* firstPlayerState = nullptr;
        cna::server::PlayerState* secondPlayerState = nullptr;
    };

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

    // 한 Tick 내에서 두 플레이어의 Dash 경계를 수집하는 함수
    std::vector<float> CollectPlayerMovementBoundaries(const cna::server::PlayerState& firstPlayerState, const cna::server::PlayerState& secondPlayerState, const float deltaSeconds)
    {
        std::vector<float> movementBoundaries;

        // Tick 시작 시점
        movementBoundaries.push_back(0.0f);

        // 첫 번째 플레이어의 Dash가 현재 Tick 내부에서 종료되는 경우
        if (firstPlayerState.dashRemainingSeconds > 0.0f && firstPlayerState.dashRemainingSeconds < deltaSeconds)
        {
            movementBoundaries.push_back(firstPlayerState.dashRemainingSeconds);
        }

        // 두 번째 플레이어의 Dash가 현재 Tick 내부에서 종료되는 경우
        if (secondPlayerState.dashRemainingSeconds > 0.0f && secondPlayerState.dashRemainingSeconds < deltaSeconds)
        {
            movementBoundaries.push_back(secondPlayerState.dashRemainingSeconds);
        }

        // Tick 종료 시점
        movementBoundaries.push_back(deltaSeconds);

        // 시간 순서대로 정렬
        std::sort(movementBoundaries.begin(), movementBoundaries.end());

        // 두 플레이어의 Dash 종료 시점이 동일한 경우 중복 경계 제거
        movementBoundaries.erase(std::unique(movementBoundaries.begin(), movementBoundaries.end()), movementBoundaries.end());

        return movementBoundaries;
    }

    // 특정 시간 동안 두 플레이어가 충돌하는 가장 빠른 시각을 계산하는 함수
    std::optional<float> FindPlayerCollisionTime(const PlayerMovement& firstPlayerMovement, const PlayerMovement& secondPlayerMovement, const float deltaSeconds) noexcept
    {
        // 시작 시점에서 첫 번째 플레이어에 대한 두 번째 플레이어의 상대 위치
        const float relativeStartX = secondPlayerMovement.startX - firstPlayerMovement.startX;
        const float relativeStartY = secondPlayerMovement.startY - firstPlayerMovement.startY;

        // 첫 번째 플레이어에 대한 두 번째 플레이어의 상대 속도
        const float relativeVelocityX = secondPlayerMovement.velocityX - firstPlayerMovement.velocityX;
        const float relativeVelocityY = secondPlayerMovement.velocityY - firstPlayerMovement.velocityY;

        // 두 플레이어 충돌 영역의 반지름 합
        const float collisionDistance = PlayerCollisionRadius * 2.0f;

        const float c = relativeStartX * relativeStartX + relativeStartY * relativeStartY - collisionDistance * collisionDistance;

        // 시작 시점부터 이미 충돌 상태인 경우
        if (c <= 0.0f)
        {
            return 0.0f;
        }

        const float a = relativeVelocityX * relativeVelocityX + relativeVelocityY * relativeVelocityY;

        // 상대 이동이 거의 없는 경우
        constexpr float kEpsilon = 1e-6f;
        if (a < kEpsilon)
        {
            return std::nullopt;
        }

        const float b = 2.0f * (relativeStartX * relativeVelocityX + relativeStartY * relativeVelocityY);

        // 상대 이동 선분과 충돌 원 사이의 교차 여부를 판단하기 위한 판별식
        const float discriminant = b * b - 4.0f * a * c;

        // 상대 이동 선분과 충돌 원이 교차하거나 접하지 않는 경우
        if (discriminant < 0.0f)
        {
            return std::nullopt;
        }

        // 현재 이동 유지 시 가장 먼저 충돌 영역에 진입하는 시간 계산
        const float collisionTime = (-b - std::sqrt(discriminant)) / (2.0f * a);

        // 충돌 시점이 현재 이동 구간 외부에 있는 경우
        if (collisionTime < 0.0f || collisionTime > deltaSeconds)
        {
            return std::nullopt;
        }

        // 시작 시점부터 실제 충돌까지 경과한 시간 반환
        return collisionTime;
    }

    std::optional<float> FindPlayerCollisionTimeDuringTick(const cna::server::PlayerState& firstPlayerState, const cna::server::PlayerState& secondPlayerState, const float deltaSeconds)
    {
        // 한 Tick 내의 두 플레이어의 Dash 경계 수집
        std::vector<float> movementBoundaries = CollectPlayerMovementBoundaries(firstPlayerState, secondPlayerState, deltaSeconds);

        // Tick 시작 시점에서의 첫 번째 플레이어 위치
        float firstStartX = firstPlayerState.positionX;
        float firstStartY = firstPlayerState.positionY;

        // Tick 시작 시점에서의 두 번째 플레이어 위치
        float secondStartX = secondPlayerState.positionX;
        float secondStartY = secondPlayerState.positionY;

        // Dash 경계로 구분한 모든 구간 순회
        for (std::size_t i = 0; i + 1 < movementBoundaries.size(); ++i)
        {
            const float segmentStartTime = movementBoundaries[i];
            const float segmentEndTime = movementBoundaries[i + 1];

            const float segmentDeltaSeconds = segmentEndTime - segmentStartTime;

            // 현재 구간에서 각 플레이어가 Dash 상태인지 판단
            const bool firstPlayerDashing = firstPlayerState.dashRemainingSeconds > segmentStartTime;
            const bool secondPlayerDashing = secondPlayerState.dashRemainingSeconds > segmentStartTime;

            const float firstVelocityX = firstPlayerDashing
                ? firstPlayerState.dashDirectionX * PlayerDashSpeed
                : firstPlayerState.directionX * PlayerMoveSpeed;

            const float firstVelocityY = firstPlayerDashing
                ? firstPlayerState.dashDirectionY * PlayerDashSpeed
                : firstPlayerState.directionY * PlayerMoveSpeed;

            const float secondVelocityX = secondPlayerDashing
                ? secondPlayerState.dashDirectionX * PlayerDashSpeed
                : secondPlayerState.directionX * PlayerMoveSpeed;

            const float secondVelocityY = secondPlayerDashing
                ? secondPlayerState.dashDirectionY * PlayerDashSpeed
                : secondPlayerState.directionY * PlayerMoveSpeed;

            const PlayerMovement firstMovement{ firstStartX, firstStartY, firstVelocityX, firstVelocityY };
            const PlayerMovement secondMovement{ secondStartX, secondStartY, secondVelocityX, secondVelocityY };

            // 현재 구간에서의 플레이어 간 충돌 검사
            const std::optional<float> collisionTime = FindPlayerCollisionTime(firstMovement, secondMovement, segmentDeltaSeconds);

            // 충돌이 감지된 경우 Tick 시작 시점에서 충돌까지 걸리는 시간 반환
            if (collisionTime)
            {
                return segmentStartTime + *collisionTime;
            }

            // 충돌하지 않은 경우 다음 구간에서의 플레이어 시작 위치 계산
            firstStartX += firstVelocityX * segmentDeltaSeconds;
            firstStartY += firstVelocityY * segmentDeltaSeconds;

            secondStartX += secondVelocityX * segmentDeltaSeconds;
            secondStartY += secondVelocityY * segmentDeltaSeconds;
        }

        return std::nullopt;
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

        // 플레이어 방향 갱신
        state.directionX = normalizedInput.moveX;
        state.directionY = normalizedInput.moveY;

        // 현재 Dash 입력의 상승 에지 확인
        const bool dashRequested = input.dash && !state.dashInputActive;

        // 현재 Dash 입력 상태 저장
        state.dashInputActive = input.dash;

        // 게임이 중지 상태인 경우 Dash를 시작하지 않음
        if (attackingSide_ == cna::PlayerSide::None)
        {
            return true;
        }

        // 새로운 Dash 요청이 아닌 경우 Dash를 시작하지 않음
        if (!dashRequested)
        {
            return true;
        }

        // Dash 쿨다운이 남아 있는 경우 Dash를 시작하지 않음
        if (state.dashCooldownRemainingSeconds > 0.0f)
        {
            return true;
        }

        // 이동 방향 입력이 없는 경우 Dash를 시작하지 않음
        if (normalizedInput.moveX == 0.0f && normalizedInput.moveY == 0.0f)
        {
            return true;
        }

        // Dash 방향 저장
        state.dashDirectionX = normalizedInput.moveX;
        state.dashDirectionY = normalizedInput.moveY;

        // Dash 지속 시간 및 쿨다운 시작
        state.dashRemainingSeconds = PlayerDashDurationSeconds;
        state.dashCooldownRemainingSeconds = PlayerDashCooldownSeconds;

        return true;
    }

    void Room::Tick(float deltaSeconds)
    {
        // 유효하지 않은 시간 값이면 위치를 갱신하지 않음
        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        // 게임이 중지 상태인 경우 플레이어 이동 및 충돌 검사 생략
        if (attackingSide_ == cna::PlayerSide::None)
        {
            return;
        }

        // 현재 Tick에서 실제 게임 진행에 사용할 시간
        const float simulationDeltaSeconds = std::min(deltaSeconds, gameRemainingSeconds_);

        // 현재 Tick에서 수집된 플레이어 충돌 목록
        std::vector<PlayerCollision> playerCollisions;

        // Room에 등록된 플레이어의 모든 조합에 대해 충돌 검사
        for (auto firstPlayerIterator = players_.begin(); firstPlayerIterator != players_.end(); ++firstPlayerIterator)
        {
            for (auto secondPlayerIterator = std::next(firstPlayerIterator); secondPlayerIterator != players_.end(); ++secondPlayerIterator)
            {
                PlayerState& firstPlayerState = firstPlayerIterator->second.GetState();
                PlayerState& secondPlayerState = secondPlayerIterator->second.GetState();

                // 플레이어 충돌 검사
                const std::optional<float> currentCollisionTime =
                    FindPlayerCollisionTimeDuringTick
                    (
                        firstPlayerState,
                        secondPlayerState,
                        simulationDeltaSeconds
                    );

                // 두 플레이어가 충돌하지 않은 경우 다음 조합 검사
                if (!currentCollisionTime)
                {
                    continue;
                }

                playerCollisions.push_back
                (
                    PlayerCollision
                    {
                        *currentCollisionTime,
                        &firstPlayerState,
                        &secondPlayerState
                    }
                );
            }
        }

        // Room에 등록된 모든 플레이어의 위치를 현재 플레이어 이동 상태를 기준으로 갱신
        for (auto& playerEntry : players_)
        {
            Player& player = playerEntry.second;
            PlayerState& state = player.GetState();

            // 현재 Tick 내에서 플레이어가 이동 가능한 시간
            float remainingMovementSeconds = simulationDeltaSeconds;

            // Dash 지속시간이 남아 있는 경우 Dash 방향으로 우선 이동
            if (state.dashRemainingSeconds > 0.0f)
            {
                // 현재 Tick 내에서 Dash가 적용되는 sub-Tick 시간
                const float dashDeltaSeconds = std::min(simulationDeltaSeconds, state.dashRemainingSeconds);

                // Dash 적용 시간만큼 Dash 이동
                state.positionX += state.dashDirectionX * PlayerDashSpeed * dashDeltaSeconds;
                state.positionY += state.dashDirectionY * PlayerDashSpeed * dashDeltaSeconds;

                // 사용한 Dash 지속시간 차감
                state.dashRemainingSeconds = std::max(0.0f, state.dashRemainingSeconds - dashDeltaSeconds);

                // 플레이어 이동 가능 시간에서 이동에 사용한 Dash 지속시간 차감  
                remainingMovementSeconds -= dashDeltaSeconds;
            }

            // 현재 Tick에서 Dash 이동 종료 후 남은 시간이 있는 경우 일반 이동
            if (remainingMovementSeconds > 0.0f)
            {
                state.positionX += state.directionX * PlayerMoveSpeed * remainingMovementSeconds;
                state.positionY += state.directionY * PlayerMoveSpeed * remainingMovementSeconds;
            }

            // Dash 쿨다운 감소
            state.dashCooldownRemainingSeconds = std::max(0.0f, state.dashCooldownRemainingSeconds - simulationDeltaSeconds);

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

        // 현재 Tick 내의 충돌 기록 순회
        for (auto& playerCollision : playerCollisions)
        {
            // 충돌 시점의 공격 진영 인덱스
            std::optional<std::size_t> attackingSideIndexAtCollision = GetAttackingSideIndexAtTickTime(playerCollision.timeInTick);

            // 충돌 시점의 공격 진영에 득점 처리
            if (attackingSideIndexAtCollision)
            {
                ++sideScores_[*attackingSideIndexAtCollision];

                std::cout
                    << "[Room] Score updated"
                    << ": roomId=" << roomId_
                    << ", flameScore=" << sideScores_[FlameSideIndex]
                    << ", frostScore=" << sideScores_[FrostSideIndex]
                    << '\n';
            }

            // 충돌한 플레이어의 Dash 종료
            playerCollision.firstPlayerState->dashDirectionX = 0.0f;
            playerCollision.firstPlayerState->dashDirectionY = 0.0f;
            playerCollision.firstPlayerState->dashRemainingSeconds = 0.0f;

            playerCollision.secondPlayerState->dashDirectionX = 0.0f;
            playerCollision.secondPlayerState->dashDirectionY = 0.0f;
            playerCollision.secondPlayerState->dashRemainingSeconds = 0.0f;

            // 충돌한 두 플레이어를 각자의 시작 위치로 재배치
            ResetPlayerPosition(*playerCollision.firstPlayerState);
            ResetPlayerPosition(*playerCollision.secondPlayerState);
        }

        // 게임 종료까지 남은 시간 감소
        gameRemainingSeconds_ = std::max(0.0f, gameRemainingSeconds_ - simulationDeltaSeconds);

        // 전체 게임 시간이 만료된 경우 게임 종료
        if (gameRemainingSeconds_ <= 0.0f)
        {
            FinishGame();
            return;
        }

        // 공수 진영 갱신
        UpdateAttackDefenseState(simulationDeltaSeconds);
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

            float snapshotDirectionX = state.directionX;
            float snapshotDirectionY = state.directionY;

            // Dash 중인 경우
            if (state.dashRemainingSeconds > 0.0f)
            {
                snapshotDirectionX = state.dashDirectionX;
                snapshotDirectionY = state.dashDirectionY;
            }

            snapshot.players.push_back
            (
                cna::network::PlayerStateSnapshot
                {
                    player.GetPlayerId(),
                    state.side,
                    state.positionX,
                    state.positionY,
                    state.positionZ,
                    gameRunning ? snapshotDirectionX : 0.0f,
                    gameRunning ? snapshotDirectionY : 0.0f,
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

        // 이전 게임의 결과 초기화
        winningSide_ = cna::PlayerSide::None;

        // 전체 게임 시간 설정
        gameRemainingSeconds_ = GameDurationSeconds;

        // 최초 공격 진영은 Flame으로 지정
        attackingSide_ = cna::PlayerSide::Flame;

        // 최초 공격 진영의 유지 시간 설정
        attackRoleSwitchRemainingSeconds_ = AttackRoleSwitchIntervalSeconds;

        // 플레이어별 게임 시작 상태 초기화
        for (auto& playerEntry : players_)
        {
            PlayerState& state = playerEntry.second.GetState();

            // 플레이어를 각 진영의 시작 위치로 이동
            ResetPlayerPosition(state);

            // 플레이어 이동 상태 초기화
            state.directionX = 0.0f;
            state.directionY = 0.0f;

            // Dash 상태 초기화
            state.dashDirectionX = 0.0f;
            state.dashDirectionY = 0.0f;
            state.dashRemainingSeconds = 0.0f;
            state.dashCooldownRemainingSeconds = 0.0f;
            state.dashInputActive = false;
        }
    }

    void Room::StopGame() noexcept
    {
        // 현재 공격 진영 제거
        attackingSide_ = cna::PlayerSide::None;

        // 공격 진영 교대 타이머 초기화
        attackRoleSwitchRemainingSeconds_ = 0.0f;

        // 전체 게임 타이머 초기화
        gameRemainingSeconds_ = 0.0f;

        // 중단된 게임의 결과 제거
        winningSide_ = cna::PlayerSide::None;

        // 중단된 게임의 점수 초기화
        sideScores_ = {};
    }

    void Room::FinishGame() noexcept
    {
        // Flame 진영의 점수가 더 높은 경우 Flame 승리
        if (sideScores_[FlameSideIndex] > sideScores_[FrostSideIndex])
        {
            winningSide_ = cna::PlayerSide::Flame;
        }

        // Frost 진영의 점수가 더 높은 경우 Frost 승리
        else if (sideScores_[FrostSideIndex] > sideScores_[FlameSideIndex])
        {
            winningSide_ = cna::PlayerSide::Frost;
        }

        // 두 진영의 점수가 동일한 경우 무승부
        else
        {
            winningSide_ = cna::PlayerSide::None;
        }

        // 게임 종료 후 공격 진영 제거
        attackingSide_ = cna::PlayerSide::None;

        // 공격 진영 교대 타이머 종료
        attackRoleSwitchRemainingSeconds_ = 0.0f;

        // 전체 게임 타이머 종료
        gameRemainingSeconds_ = 0.0f;

        // 진행 중인 Dash 종료
        for (auto& playerEntry : players_)
        {
            PlayerState& state = playerEntry.second.GetState();

            state.dashDirectionX = 0.0f;
            state.dashDirectionY = 0.0f;
            state.dashRemainingSeconds = 0.0f;
        }

        std::cout
            << "[Room] Game ended"
            << ": roomId=" << roomId_
            << ", flameScore=" << sideScores_[FlameSideIndex]
            << ", frostScore=" << sideScores_[FrostSideIndex];

        if (winningSide_ == cna::PlayerSide::Flame)
        {
            std::cout << ", winner=Flame" << '\n';
        }
        else if (winningSide_ == cna::PlayerSide::Frost)
        {
            std::cout << ", winner=Frost" << '\n';
        }
        else
        {
            std::cout << ", winner=Draw" << '\n';
        }
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

    std::optional<std::size_t> Room::GetOppositePlayerSideIndex(cna::PlayerSide side) noexcept
    {
        // Flame 진영인 경우
        if (side == cna::PlayerSide::Flame)
        {
            return FrostSideIndex;
        }

        // Frost 진영인 경우
        if (side == cna::PlayerSide::Frost)
        {
            return FlameSideIndex;
        }

        return std::nullopt;
    }

    std::optional<std::size_t> Room::GetAttackingSideIndexAtTickTime(float elapsedSeconds) const noexcept
    {
        // 타겟 시점이 역할 교체 이전인 경우
        if (elapsedSeconds < attackRoleSwitchRemainingSeconds_)
        {
            return GetPlayerSideIndex(attackingSide_);
        }

        // 타겟 시점이 역할 교체 이후인 경우
        else
        {
            return GetOppositePlayerSideIndex(attackingSide_);
        }
    }
}
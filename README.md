# CppNetworkArena

<p align="center">
  <img width="50%" alt="CppNetworkArena" src="https://github.com/user-attachments/assets/e14e536d-9c96-4aa1-aafb-1609f29695b2" />
</p>

---

## 🧭 프로젝트 개요

본 프로젝트는 **C++ 및 Boost.Asio를 활용한 실시간 클라이언트-서버 네트워크 기반 아레나 게임 개발 프로젝트**입니다.

클라이언트는 플레이어 입력을 서버에 전달하고, 서버에서 게임 상태와 이동, 충돌 및 득점 판정을 수행하는 **서버 권위형(Server-Authoritative) 구조**를 기반으로 구현하였습니다.

프로젝트 주요 목표는 다음과 같습니다.

- C++를 활용한 게임 클라이언트-서버 구조 설계
- Boost.Asio 기반 비동기 TCP 네트워크 통신 구현
- 메시지 직렬화 및 역직렬화를 통한 자체 네트워크 프로토콜 구성
- 서버 권위형 플레이어 이동 및 게임 규칙 처리
- Snapshot 기반 월드 상태 동기화 및 클라이언트 보간
- 연속 충돌 판정(CCD)을 통한 고속 이동 중 충돌 처리
- DirectX 11 기반 게임 월드 렌더링 및 Direct2D / DirectWrite 기반 HUD 구성
- Dear ImGui 기반 네트워크 상태 모니터링

---

## 🪄 주요 기능

### 비동기 TCP 네트워크 통신

Boost.Asio를 기반으로 서버와 클라이언트 간 비동기 TCP 통신을 구현하였습니다.

각 클라이언트 연결은 독립적인 Session으로 관리하며, TCP 스트림에서 수신한 데이터를 메시지 헤더와 Payload 단위로 분리하여 처리합니다.

클라이언트와 서버는 다음과 같은 타입의 주요 메시지를 주고받습니다.

- `Ping` / `Pong`
- `PlayerInput`
- `PlayerIdentity`
- `WorldStateSnapshot`

서버는 클라이언트의 Room 입장을 승인한 후 `PlayerIdentity`를 전달하며, 클라이언트는 해당 식별 정보를 수신한 이후부터 게임 월드와 플레이어 상태를 구성합니다.

### 서버 권위형 게임 시스템

**클라이언트는 이동 및 Dash 입력만 서버로 전송**하며, **실제 플레이어 상태와 게임 규칙은 서버에서 결정**합니다.

서버에서 관리하는 항목은 다음과 같습니다.

- 플레이어 이동
- Dash 상태 및 쿨다운
- 공격 / 수비 역할 전환
- 플레이어 충돌 판정
- 충돌 시 득점 처리
- 득점 후 Spawn 위치 초기화
- 게임 타이머 및 최종 승패 판정

### 3D 게임 월드 렌더링

**DirectX 11을 기반으로 게임 월드의 3D 요소를 렌더링**합니다.

<p align="left">
  <img width="50%" alt="3D Rendering" src="https://github.com/user-attachments/assets/0103eb56-bbf4-495d-baea-624e80137b1b" />
</p>

Assimp를 통해 FBX 모델의 메시 및 머티리얼 데이터를 임포트하고, 외부 텍스처를 연결하여 모델을 구성하였습니다.  
또한 Directional Light와 Ambient Light를 적용하여 기본적인 3D 라이팅을 구현하였습니다.

### Snapshot 기반 상태 동기화

서버는 **현재 Room의 게임 상태를 `WorldStateSnapshot`으로 구성하여 클라이언트에 전달**합니다.

클라이언트는 최근 Snapshot History를 유지하고, 최신 상태보다 일정 시간 과거의 두 Snapshot을 선택하여 플레이어 위치와 방향을 보간합니다.  
이를 통해 네트워크 Snapshot의 수신 간격 차이로 발생할 수 있는 화면상의 움직임 끊김을 완화하였습니다.

수신한 Snapshot은 Room 및 Server Tick을 기준으로 검증하며, 과거 또는 중복된 Snapshot을 제외하고 최근 상태를 History에 유지합니다.

### 연속 충돌 판정

Dash와 같이 한 Tick 동안의 이동량이 큰 경우, 단순한 위치 기반 충돌 판정에서 발생할 수 있는 충돌 누락을 방지하기 위해 **연속 충돌 판정 방식(CCD)을 적용**하였습니다.  
플레이어의 이동 구간을 기준으로 가장 이른 충돌 시점을 계산하고, 해당 시점의 공격 진영을 기준으로 득점을 처리합니다.

### 게임 HUD

**Direct2D 및 DirectWrite를 사용하여 3D 게임 월드와 동일한 SwapChain 백 버퍼에 게임 HUD를 렌더링**합니다.

<p align="left">
  <img width="50%" alt="HUD Rendering" src="https://github.com/user-attachments/assets/1a12e1d3-a862-457a-a3f6-9fcab493ae3a" />
</p>

게임 상태에 따라 다음 정보를 표시합니다.

- `Waiting` : 상대 플레이어 대기 상태
- `Playing` : 진영별 점수, 게임 타이머, 공격 / 수비 역할 및 역할 전환 시간
- `Finished` : 최종 점수 및 `WIN` / `LOSE` / `DRAW`

### Network Monitor

Dear ImGui를 활용하여 네트워크 상태를 실시간으로 확인할 수 있는 Network Monitor를 구현하였습니다.

<p align="left">
  <img width="50%" alt="Network Monitor" src="https://github.com/user-attachments/assets/73292056-424c-472e-9f78-6542c21a3e9f" />
</p>

Network Monitor UI 윈도우에는 다음 정보가 표시됩니다.

- 네트워크 및 Session 정보
  - 서버 연결 상태
  - RTT
  - Room ID / Player ID

- 네트워크 동기화 상태
  - 최신 Server Tick
  - Snapshot History

- 게임 플레이 상태
  - 게임 진행 상태
  - 현재 공격 진영 및 점수

- 최근 네트워크 메시지 송수신 기록
  - `SEND` / `RECV`
  - `MessageType`
  - Payload 크기

---

## 🚀 빌드 및 실행 방법

### 요구 환경

- Windows 10 / 11
- MSVC v143 C++ Build Tools
- Windows SDK
- CMake 3.20 이상
- Ninja
- vcpkg

### 프로젝트 빌드

1. 저장소를 Clone합니다.

```
git clone https://github.com/MANGRYANG/CppNetworkArena.git
cd CppNetworkArena
```

2. CMake Preset을 사용하여 프로젝트를 Configure합니다.

```
cmake --preset x64-release
```

3. 프로젝트를 빌드합니다.

```
cmake --build out/build/x64-release
```

_개발 및 디버깅이 필요한 경우 x64-debug Preset을 사용할 수 있습니다._

### 실행

게임은 서버 1개와 클라이언트 2개를 실행하여 테스트할 수 있습니다.

1. 서버를 먼저 실행합니다.

2. 첫 번째 클라이언트를 실행하여 서버에 접속합니다.
3. 두 번째 클라이언트를 실행하여 서버에 접속합니다.

4. 두 플레이어의 Room 입장이 완료되면 게임이 시작됩니다.

```
Server
  ↑ 
  ├── Client 1 
  └── Client 2 
```

클라이언트는 동일한 서버에 연결되며, 서버에서 Room 입장과 플레이어 식별 정보를 관리합니다.

### 기본 조작

`W`/`A`/`S`/`D` : 플레이어 이동 <br>
`Space` : Dash (쿨타임 1초)

---

## 🎮 게임 규칙

게임은 Flame과 Frost 두 진영에 각각 한 명의 플레이어가 배치되며, **플레이어 간 1:1 대전**으로 진행됩니다.

한 플레이어는 공격, 다른 플레이어는 수비 역할을 가지며, **일정 시간(3초)마다 역할이 자동으로 전환**됩니다.

두 플레이어가 충돌하면 **충돌 시점의 공격 진영이 1점을 획득**합니다.

득점 후 두 플레이어는 각자의 Spawn 위치로 초기화됩니다.

`Space` 키를 사용하여 Dash 기능을 사용할 수 있습니다.

**전체 게임 시간(30초)이 종료되면 양쪽 진영의 점수를 비교하여 승패를 결정**합니다.

점수가 동일한 경우 무승부로 처리합니다.

---

## 🔧 사용 기술

- `C++` : 서버 / 클라이언트 핵심 로직 구현
- `Windows` : 실행 플랫폼
- `CMake` : 프로젝트 빌드 구성
- `vcpkg` : 외부 라이브러리 의존성 관리
- `Boost.Asio TCP` : 비동기 네트워크 통신 및 Session 기반 연결 관리
- `Assimp` : FBX 모델 임포트 및 메시 / 머티리얼 데이터 파싱
- `DirectX 11` : 아레나 맵, 플레이어 등 3D 요소 렌더링
- `Direct2D`, `DirectWrite` : 게임 HUD 렌더링 및 텍스트 출력
- `Dear ImGui` : RTT, Packet Log 등 네트워크 상태 시각화

---

## 📄 문서

[![CppNetworkArena Docs](https://img.shields.io/badge/CppNetworkArena-Documentation-purple?style=for-the-badge&logo=obsidian)](https://github.com/MANGRYANG/CppNetworkArena-docs)

---

## ⚖️ 라이선스

본 프로젝트는 MIT 라이선스를 따릅니다. 다만 개인적인 용도로 개발되어 외부 기여를 받지 않고 있습니다.  
프로젝트를 참조하거나 사용하는 데는 문제가 없으며, 필요한 경우 포크하여 개인 프로젝트로 사용하셔도 무방합니다.

#include "client_application.h"

#include "graphics/mesh/primitives/unit_quad_mesh.h"

#include <boost/asio/error.hpp>

#include <DirectXMath.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
    // 클라이언트가 연결할 서버 호스트
    constexpr std::string_view TargetHost = "127.0.0.1";

    // 클라이언트가 연결할 서버 포트
    constexpr std::uint16_t TargetPort = 7777;

    // DirectX 렌더링 영역으로 사용할 기본 클라이언트 너비
    constexpr int InitialClientWidth = 1280;

    // DirectX 렌더링 영역으로 사용할 기본 클라이언트 높이
    constexpr int InitialClientHeight = 720;
}

namespace cna::client
{
    ClientApplication::ClientApplication(const HINSTANCE hInstance)
        : networkClient_(std::make_shared<NetworkClient>(ioContext_)), window_(hInstance)
    {
    }

    ClientApplication::~ClientApplication()
    {
        Shutdown();
    }

    int ClientApplication::Run()
    {
        // 애플리케이션이 이미 실행 중이거나 Win32 윈도우가 이미 생성되어 있는 경우
        if (running_ || window_.IsCreated())
        {
            return 1;
        }

        // DirectX Swap Chain이 사용할 Win32 윈도우 생성
        if (!window_.Create(L"CppNetworkArena", InitialClientWidth, InitialClientHeight))
        {
            // Win32 윈도우 생성 실패 메시지 출력
            std::cerr << "[GameClient] Failed to create Win32 window" << '\n';

            return 1;
        }

        // 생성된 Win32 윈도우를 대상으로 DirectX 11 그래픽 자원 초기화
        if (!renderer_.Initialize(window_.GetHandle(), InitialClientWidth, InitialClientHeight))
        {
            // 초기화 실패 메시지 출력
            std::cerr << "[GameClient] Failed to initialize DirectX 11 renderer" << '\n';

            // 애플리케이션 종료
            Shutdown();

            return 1;
        }

        // CPU 메쉬 데이터를 기반으로 단위 사각형 GPU 메쉬 생성
        std::unique_ptr<D3D11Mesh> unitQuadMesh = renderer_.CreateMesh(CreateUnitQuadMeshData());

        // 단위 사각형 메쉬 리소스 생성에 실패한 경우
        if (!unitQuadMesh)
        {
            // 생성 실패 메시지 출력
            std::cerr
                << "[GameClient] Failed to create unit quad mesh"
                << '\n';

            // 애플리케이션 종료
            Shutdown();

            return 1;
        }

        // GPU 단위 사각형 메쉬를 메쉬 저장소에 등록
        unitQuadMeshHandle_ = meshRepository_.AddMesh(std::move(unitQuadMesh));

        // 메쉬 저장소에 등록하지 못한 경우
        if (!unitQuadMeshHandle_.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to register unit quad mesh"
                << '\n';

            Shutdown();

            return 1;
        }

        // Win32 윈도우를 화면에 표시 
        window_.Show();

        // 클라이언트 영역 크기 변경 이벤트 콜백 등록
        window_.SetResizeCallback
        (
            [this](const std::uint32_t clientWidth, const std::uint32_t clientHeight) noexcept
            {
                HandleWindowResized(clientWidth, clientHeight);
            }
        );

        running_ = true;
        exitCode_ = 0;

        std::cout << "CppNetworkArena GameClient starting..." << '\n';

        // 서버에 클라이언트 연결
        if (!StartConnection())
        {
            // 서버에 연결하지 못한 경우 실패 메시지 출력
            std::cerr << "[NetworkClient] Connection request was rejected." << '\n';

            RequestExit(1);
        }

        // 애플리케이션 실행 루프 시작
        while (running_)
        {
            // OS 메시지 확인 및 처리
            window_.ProcessMessages(running_);

            if (!running_)
            {
                break;
            }

            // 대기 중인 네트워크 완료 이벤트 일괄 처리
            ProcessNetworkEvents();

            if (!running_)
            {
                break;
            }

            Update();

            // 애플리케이션 화면 렌더링
            if (!Render())
            {
                break;
            }
        }

        // 애플리케이션 종료
        Shutdown();

        return exitCode_;
    }

    bool ClientApplication::StartConnection()
    {
        return networkClient_->Connect
        (
            TargetHost,
            TargetPort,
            [this](const boost::asio::ip::tcp::endpoint& endpoint)
            {
                HandleConnected(endpoint);
            },
            [this](const boost::system::error_code& error)
            {
                HandleConnectionFailed(error);
            },
            [this](const boost::system::error_code& error)
            {
                HandleDisconnected(error);
            },
            [this](const cna::network::PlayerIdentityPayload& identity)
            {
                HandlePlayerIdentity(identity);
            },
            [this](const cna::network::WorldStateSnapshot& snapshot)
            {
                HandleWorldStateSnapshot(snapshot);
            }
        );
    }

    void ClientApplication::ProcessNetworkEvents()
    {
        // 대기 중인 네트워크 완료 이벤트 핸들러 일괄 실행
        ioContext_.poll();
    }

    void ClientApplication::Shutdown() noexcept
    {
        // 애플리케이션 실행 루프 중지
        running_ = false;

        // 애플리케이션 종료 과정에서 크기 변경 콜백이 렌더러를 호출하지 않도록 콜백 연결 해제
        window_.SetResizeCallback({});

        // 네트워크 연결 해제
        if (networkClient_ && (networkClient_->GetConnectionState() != NetworkClient::ConnectionState::Disconnected))
        {
            networkClient_->Disconnect();
        }

        // 단위 사각형 메쉬 핸들 무효화
        unitQuadMeshHandle_ = {};
        // GPU 메쉬 소유권 해제
        meshRepository_.Clear();

        // 메쉬 제거 후 렌더러 관련 자원 해제
        renderer_.Shutdown();

        // Win32 플랫폼 윈도우 제거
        window_.Destroy();

        // 종료된 연결의 플레이어 식별 정보 및 월드 상태 초기화
        clientGameState_.Reset();

        // IO 컨텍스트 중지
        ioContext_.stop();
    }

    void ClientApplication::RequestExit(const int exitCode) noexcept
    {
        // 종료 코드 설정
        exitCode_ = exitCode;

        // 애플리케이션 실행 루프 중지
        running_ = false;
    }

    void ClientApplication::HandleConnected(const boost::asio::ip::tcp::endpoint& endpoint)
    {
        // 서버 연결 성공 메시지 출력
        std::cout
            << "[NetworkClient] Connected: endpoint="
            << endpoint.address().to_string()
            << ':' << endpoint.port()
            << '\n';
    }

    void ClientApplication::HandleConnectionFailed(const boost::system::error_code& error)
    {
        // 실패한 연결 시도의 게임 상태 초기화
        clientGameState_.Reset();

        // 서버 연결 실패 메시지 출력
        std::cerr
            << "[NetworkClient] Connection failed: "
            << error.message()
            << '\n';
    }

    void ClientApplication::HandleDisconnected(const boost::system::error_code& error)
    {
        // 종료된 연결의 게임 상태 초기화
        clientGameState_.Reset();

        // 서버가 연결을 정상적으로 종료한 경우
        if (error == boost::asio::error::eof)
        {
            std::cout << "[NetworkClient] Server disconnected." << '\n';

            return;
        }

        // 오류가 발생하여 종료된 경우
        std::cerr
            << "[NetworkClient] Connection terminated: "
            << error.message()
            << '\n';
    }

    void ClientApplication::HandlePlayerIdentity(const cna::network::PlayerIdentityPayload& identity)
    {
        // 서버가 할당한 로컬 플레이어 식별 정보를 게임 상태 계층에 적용
        if (!clientGameState_.ApplyPlayerIdentity(identity))
        {
            std::cerr
                << "[GameClient] PlayerIdentity rejected"
                << ": roomId=" << identity.roomId
                << ", playerId=" << identity.playerId
                << '\n';

            return;
        }

        // 게임 상태 계층에 저장된 Room ID 조회
        const std::optional<cna::RoomId> roomId = clientGameState_.GetRoomId();
        // 게임 상태 계층에 저장된 로컬 Player ID 조회
        const std::optional<cna::PlayerId> localPlayerId = clientGameState_.GetLocalPlayerId();

        // 식별 정보 적용 후 조회할 수 없는 경우
        if (!roomId || !localPlayerId)
        {
            std::cerr << "[GameClient] PlayerIdentity unavailable after apply" << '\n';

            return;
        }

        std::cout
            << "[GameClient] PlayerIdentity received"
            << ": roomId=" << *roomId
            << ", playerId=" << *localPlayerId
            << '\n';
    }

    void ClientApplication::HandleWorldStateSnapshot(const cna::network::WorldStateSnapshot& snapshot)
    {
        // 서버에서 받은 월드 상태 스냅샷을 게임 상태 계층에 적용
        if (!clientGameState_.ApplyWorldStateSnapshot(snapshot))
        {
            std::cerr
                << "[GameClient] WorldStateSnapshot rejected"
                << ": serverTick=" << snapshot.serverTick
                << ", roomId=" << snapshot.roomId
                << '\n';

            return;
        }

        // 게임 상태 계층에 저장된 최신 월드 상태 조회
        const cna::network::WorldStateSnapshot* worldState = clientGameState_.GetWorldState();

        // 월드 상태가 적용되지 않은 경우
        if (!worldState)
        {
            return;
        }

        std::cout
            << "[GameClient] WorldStateSnapshot received"
            << ": serverTick=" << worldState->serverTick
            << ", roomId=" << worldState->roomId
            << ", playerCount=" << worldState->players.size()
            << '\n';
    }

    void ClientApplication::HandleWindowResized(std::uint32_t clientWidth, std::uint32_t clientHeight) noexcept
    {
        // 이미 종료가 요청된 경우 크기 변경 이벤트 무시
        if (!running_)
        {
            return;
        }

        // 변경된 클라이언트 영역 크기에 맞춰 그래픽 자원 재구성
        if (!renderer_.Resize(clientWidth, clientHeight))
        {
            std::cerr
                << "[GameClient] Failed to resize DirectX 11 renderer"
                << ": width=" << clientWidth
                << ", height=" << clientHeight
                << '\n';

            RequestExit(1);
        }
    }

    void ClientApplication::Update()
    {
        // 내부 데이터 및 상태 갱신 로직
    }

    bool ClientApplication::Render()
    {
        // 백 버퍼를 단색 배경으로 초기화
        if (!renderer_.BeginFrame(0.05f, 0.08f, 0.12f, 1.0f))
        {
            std::cerr << "[GameClient] Failed to clear backbuffer" << '\n';

            RequestExit(1);

            return false;
        }

        // 저장소에서 현재 유효한 단위 사각형 메쉬 조회
        const D3D11Mesh* const testRectangleMesh = meshRepository_.FindMesh(unitQuadMeshHandle_);

        // 유효한 단위 사각형 메쉬를 찾지 못한 경우
        if (!testRectangleMesh)
        {
            std::cerr
                << "[GameClient] Cannot find test rectangle mesh"
                << '\n';

            RequestExit(1);

            return false;
        }

        // 단위 사각형에 적용할 월드 변환 행렬 계산
        const DirectX::XMMATRIX testRectangleScale = DirectX::XMMatrixScaling(8.0f, 8.0f, 1.0f);
        const DirectX::XMMATRIX testRectangleRotation = DirectX::XMMatrixRotationZ(DirectX::XMConvertToRadians(45.0f));
        const DirectX::XMMATRIX testRectangleTranslation = DirectX::XMMatrixTranslation(3.0f, 0.0f, 0.0f);
        const DirectX::XMMATRIX testRectangleWorldMatrix = testRectangleScale * testRectangleRotation * testRectangleTranslation;

        // 메쉬에 월드 변환 행렬 적용 후 그리기
        if (!renderer_.DrawMesh(*testRectangleMesh, testRectangleWorldMatrix))
        {
            std::cerr
                << "[GameClient] Failed to draw test rectangle"
                << '\n';

            RequestExit(1);

            return false;
        }

        // 백 버퍼와 프론트 버퍼를 교체하여 화면에 출력
        if (!renderer_.EndFrame())
        {
            std::cerr << "[GameClient] Failed to present swapchain" << '\n';

            RequestExit(1);

            return false;
        }

        return true;
    }
}
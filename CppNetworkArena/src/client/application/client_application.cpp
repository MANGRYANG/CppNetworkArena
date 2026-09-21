#include "client_application.h"

#include "graphics/model/loaders/assimp_model_loader.h"

#include <boost/asio/error.hpp>

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
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

    // Windows 환경에서의 최대 경로 길이를 포함할 수 있는 문자열 버퍼 크기 설정
    constexpr DWORD MaxExecutablePathLength = 32768;

    // 현재 프로세스의 실행 파일을 기준으로 클라이언트 런타임 파일의 절대 경로를 계산하는 함수
    std::filesystem::path ResolveClientRuntimePath(const std::filesystem::path& relativeFilePath)
    {
        std::wstring executablePathBuffer(MaxExecutablePathLength, L'\0');

        // 현재 프로세스의 실행 파일 경로 저장
        const DWORD executablePathLength = GetModuleFileNameW
        (
            nullptr,
            executablePathBuffer.data(),
            static_cast<DWORD>(executablePathBuffer.size())
        );

        if (executablePathLength == 0 || executablePathLength >= static_cast<DWORD>(executablePathBuffer.size()))
        {
            return {};
        }

        // 경로 버퍼 크기 재조정
        executablePathBuffer.resize(executablePathLength);

        // 클라이언트 런타임 파일의 절대 경로 반환
        return std::filesystem::path(executablePathBuffer).parent_path()/relativeFilePath;
    }
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

        // 실행 파일을 기준으로 정점 셰이더 경로 계산
        const std::filesystem::path vertexShaderPath = ResolveClientRuntimePath("shaders/VertexShader.hlsl");

        // 실행 파일을 기준으로 픽셀 셰이더 경로 계산
        const std::filesystem::path pixelShaderPath = ResolveClientRuntimePath("shaders/PixelShader.hlsl");

        if (vertexShaderPath.empty() || pixelShaderPath.empty())
        {
            std::cerr << "[GameClient] Failed to resolve shader file paths" << '\n';

            Shutdown();

            return 1;
        }

        // 렌더러에 전달할 윈도우 정보와 셰이더 경로 구성
        const D3D11RendererInitializeInfo rendererInitializeInfo =
        {
            window_.GetHandle(),
            InitialClientWidth,
            InitialClientHeight,
            vertexShaderPath,
            pixelShaderPath
        };

        // 생성된 Win32 윈도우를 대상으로 DirectX 11 그래픽 자원 초기화
        if (!renderer_.Initialize(rendererInitializeInfo))
        {
            // 초기화 실패 메시지 출력
            std::cerr << "[GameClient] Failed to initialize DirectX 11 renderer" << '\n';

            // 애플리케이션 종료
            Shutdown();

            return 1;
        }

        // 애플리케이션에서 사용할 메쉬와 렌더 객체 생성
        if (!InitializeRenderResources())
        {
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

    bool ClientApplication::InitializeRenderResources()
    {
        // 아레나 맵 FBX 파일의 절대 경로 계산
        const std::filesystem::path arenaMapPath = ResolveClientRuntimePath("assets/meshes/environments/map01.fbx");

        if (arenaMapPath.empty())
        {
            std::cerr
                << "[GameClient] Failed to resolve client asset directory"
                << '\n';

            return false;
        }

        // 아레나 맵 FBX를 CPU 모델 데이터로 변환
        AssimpModelLoadResult loadResult = LoadStaticModelData(arenaMapPath);

        if (!loadResult.Succeeded())
        {
            std::cerr
                << "[GameClient] Failed to load arena FBX model: "
                << loadResult.errorMessage
                << '\n';

            return false;
        }

        const ModelMeshData& mapMesh = loadResult.modelData.meshes.front();

        // 아레나 맵 메쉬를 메쉬 저장소에 등록하고 핸들 추출
        arenaMapMeshHandle_ = CreateMeshResource(mapMesh.meshData);

        if (!arenaMapMeshHandle_.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to create arena mesh resource: "
                << mapMesh.name
                << '\n';

            return false;
        }

        // 아레나 맵 메쉬가 참조하는 머티리얼이 있으면 기본 색상 및 노멀 맵 텍스처 생성
        if (mapMesh.materialIndex != InvalidMaterialIndex)
        {
            if (mapMesh.materialIndex >= loadResult.modelData.materials.size())
            {
                std::cerr
                    << "[GameClient] Arena mesh references invalid material index"
                    << ": materialIndex=" << mapMesh.materialIndex
                    << '\n';

                return false;
            }

            const ModelMaterialData& mapMaterial = loadResult.modelData.materials[mapMesh.materialIndex];

            // 기본 색상 텍스처가 연결된 머티리얼이면 GPU 텍스처 리소스를 생성하고 텍스처 저장소에 등록
            if (mapMaterial.baseColorTextureIndex != InvalidTextureIndex)
            {
                if (mapMaterial.baseColorTextureIndex >= loadResult.modelData.textures.size())
                {
                    std::cerr
                        << "[GameClient] Arena material references invalid Base Color texture index"
                        << ": textureIndex=" << mapMaterial.baseColorTextureIndex
                        << '\n';

                    return false;
                }

                const ModelTextureData& baseColorTextureData = loadResult.modelData.textures[mapMaterial.baseColorTextureIndex];

                arenaMapBaseColorTextureHandle_ = CreateTextureResource(baseColorTextureData);

                if (!arenaMapBaseColorTextureHandle_.IsValid())
                {
                    std::cerr
                        << "[GameClient] Failed to create arena Base Color texture resource"
                        << ": source=" << baseColorTextureData.sourceReference
                        << '\n';

                    return false;
                }
            }

            // 노멀 맵 텍스처가 연결된 머티리얼이면 GPU 텍스처 리소스를 생성하고 텍스처 저장소에 등록
            if (mapMaterial.normalMapTextureIndex != InvalidTextureIndex)
            {
                if (mapMaterial.normalMapTextureIndex >= loadResult.modelData.textures.size())
                {
                    std::cerr
                        << "[GameClient] Arena material references invalid Normal texture index"
                        << ": textureIndex=" << mapMaterial.normalMapTextureIndex
                        << '\n';

                    return false;
                }

                const ModelTextureData& normalTextureData = loadResult.modelData.textures[mapMaterial.normalMapTextureIndex];

                arenaMapNormalMapTextureHandle_ = CreateTextureResource(normalTextureData);

                if (!arenaMapNormalMapTextureHandle_.IsValid())
                {
                    std::cerr
                        << "[GameClient] Failed to create arena Normal texture resource"
                        << ": source=" << normalTextureData.sourceReference
                        << '\n';

                    return false;
                }
            }
        }

        // 아레나 맵 렌더 객체 구성
        RenderObject arenaMapObject;

        arenaMapObject.meshHandle = arenaMapMeshHandle_;
        arenaMapObject.baseColorTextureHandle = arenaMapBaseColorTextureHandle_;
        arenaMapObject.normalMapTextureHandle = arenaMapNormalMapTextureHandle_;

        // 렌더 객체 목록에 아레나 맵 렌더 객체 등록
        renderObjects_.push_back(arenaMapObject);

        return true;
    }

    MeshHandle ClientApplication::CreateMeshResource(const MeshData& meshData)
    {
        // CPU 메쉬 데이터를 사용하여 GPU 메쉬 생성
        std::unique_ptr<D3D11Mesh> mesh = renderer_.CreateMesh(meshData);

        if (!mesh)
        {
            return {};
        }

        // 생성한 GPU 메쉬의 소유권을 저장소로 이전하고 핸들 반환
        return meshRepository_.AddMesh(std::move(mesh));
    }

    TextureHandle ClientApplication::CreateTextureResource(const ModelTextureData& textureData)
    {
        std::unique_ptr<D3D11Texture> texture;

        // FBX 파일에 내장된 텍스처는 압축 여부에 따라 내장 데이터를 사용한 WIC 디코딩 또는 원시 BGRA 데이터 변환을 거쳐 생성
        if (textureData.IsEmbedded())
        {
            if (textureData.IsCompressedEmbedded())
            {
                texture = renderer_.CreateTextureFromEncodedMemory(textureData.embeddedData);
            }
            else
            {
                texture = renderer_.CreateTextureFromBgraPixels
                (
                    textureData.width,
                    textureData.height,
                    textureData.embeddedData
                );
            }
        }
        else
        {
            // 외부 경로의 텍스처는 Assimp 로더를 통해 계산한 파일 경로를 사용한 WIC 디코딩을 거쳐 생성
            texture = renderer_.CreateTextureFromFile(textureData.filePath);
        }

        if (!texture)
        {
            return {};
        }

        // 생성된 GPU 텍스처 리소스를 텍스처 저장소에 등록
        return textureRepository_.AddTexture(std::move(texture));
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

        // GPU 메쉬와 텍스처를 참조하는 모든 렌더 객체 제거
        renderObjects_.clear();

        // 아레나 맵 메쉬 및 텍스처 핸들 무효화
        arenaMapMeshHandle_ = {};
        arenaMapBaseColorTextureHandle_ = {};
        arenaMapNormalMapTextureHandle_ = {};

        // 렌더러보다 먼저 GPU 메쉬와 텍스처 소유권 해제
        meshRepository_.Clear();
        textureRepository_.Clear();

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

        // 등록된 렌더 객체를 순회하며 공유 메쉬와 객체별 월드 변환 적용
        for (const RenderObject& renderObject : renderObjects_)
        {
            // 렌더 객체가 참조하는 GPU 메쉬 조회
            const D3D11Mesh* const mesh = meshRepository_.FindMesh(renderObject.meshHandle);

            // 렌더 객체가 참조하는 GPU 메쉬를 찾지 못한 경우
            if (!mesh)
            {
                std::cerr
                    << "[GameClient] Cannot find render object mesh"
                    << '\n';

                RequestExit(1);

                return false;
            }

            const D3D11Texture* baseColorTexture = nullptr;

            // 렌더 객체가 기본 색상 텍스처를 참조하는 경우 GPU 텍스처 리소스 조회
            if (renderObject.baseColorTextureHandle.IsValid())
            {
                baseColorTexture = textureRepository_.FindTexture(renderObject.baseColorTextureHandle);

                if (!baseColorTexture)
                {
                    std::cerr
                        << "[GameClient] Cannot find render object Base Color texture"
                        << '\n';

                    RequestExit(1);

                    return false;
                }
            }

            const D3D11Texture* normalMapTexture = nullptr;

            // 렌더 객체가 노멀 맵 텍스처를 참조하는 경우 GPU 텍스처 리소스 조회
            if (renderObject.normalMapTextureHandle.IsValid())
            {
                normalMapTexture = textureRepository_.FindTexture(renderObject.normalMapTextureHandle);

                if (!normalMapTexture)
                {
                    std::cerr
                        << "[GameClient] Cannot find render object Normal texture"
                        << '\n';

                    RequestExit(1);

                    return false;
                }
            }

            // 객체별 월드 변환 행렬과 기본 색상 및 노멀 텍스처를 적용하여 렌더 객체 출력
            if (!renderer_.DrawMesh(*mesh, baseColorTexture, normalMapTexture, renderObject.transform.GetWorldMatrix()))
            {
                std::cerr
                    << "[GameClient] Failed to draw render object"
                    << '\n';

                RequestExit(1);

                return false;
            }
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
#include "client_application.h"

#include "graphics/model/loaders/assimp_model_loader.h"

#include <boost/asio/error.hpp>

#include <DirectXMath.h>

#include <cmath>
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

        // 플레이어 FBX 파일의 절대 경로 계산
        const std::filesystem::path playerModelPath = ResolveClientRuntimePath("assets/meshes/players/player.fbx");

        if (playerModelPath.empty())
        {
            std::cerr
                << "[GameClient] Failed to resolve player model path"
                << '\n';

            return false;
        }

        // 플레이어 FBX를 CPU 모델 데이터로 변환
        AssimpModelLoadResult playerLoadResult = LoadStaticModelData(playerModelPath);

        if (!playerLoadResult.Succeeded())
        {
            std::cerr
                << "[GameClient] Failed to load player FBX model: "
                << playerLoadResult.errorMessage
                << '\n';

            return false;
        }

        const ModelMeshData& playerMesh = playerLoadResult.modelData.meshes.front();

        // 모든 플레이어가 공유할 메쉬를 메쉬 저장소에 등록하고 핸들 추출
        playerMeshHandle_ = CreateMeshResource(playerMesh.meshData);

        if (!playerMeshHandle_.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to create player mesh resource: "
                << playerMesh.name
                << '\n';

            return false;
        }

        // 화염 측 플레이어의 기본 색상 텍스처 파일 경로 계산
        const std::filesystem::path flameBaseColorTexturePath =
            ResolveClientRuntimePath("assets/textures/players/player_flame_texture.png");

        // 화염 측 플레이어의 노멀 텍스처 파일 경로 계산
        const std::filesystem::path flameNormalMapTexturePath =
            ResolveClientRuntimePath("assets/textures/players/player_flame_texture_normal.png");

        // 서리 측 플레이어의 기본 색상 텍스처 파일 경로 계산
        const std::filesystem::path frostBaseColorTexturePath =
            ResolveClientRuntimePath("assets/textures/players/player_frost_texture.png");

        // 서리 측 플레이어의 노멀 텍스처 파일 경로 계산
        const std::filesystem::path frostNormalMapTexturePath =
            ResolveClientRuntimePath("assets/textures/players/player_frost_texture_normal.png");

        if (flameBaseColorTexturePath.empty() || flameNormalMapTexturePath.empty() ||
            frostBaseColorTexturePath.empty() || frostNormalMapTexturePath.empty()
            )
        {
            std::cerr
                << "[GameClient] Failed to resolve player texture paths"
                << '\n';

            return false;
        }

        // 화염 측 플레이어의 기본 색상 텍스처를 텍스처 저장소에 등록
        flamePlayerTextureSet_.baseColorTextureHandle = CreateTextureResource(flameBaseColorTexturePath);

        if (!flamePlayerTextureSet_.baseColorTextureHandle.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to create flame player Base Color texture resource"
                << ": source=" << flameBaseColorTexturePath
                << '\n';

            return false;
        }

        // 화염 측 플레이어의 노멀 텍스처를 텍스처 저장소에 등록
        flamePlayerTextureSet_.normalTextureHandle = CreateTextureResource(flameNormalMapTexturePath);

        if (!flamePlayerTextureSet_.normalTextureHandle.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to create flame player Normal texture resource"
                << ": source=" << flameNormalMapTexturePath
                << '\n';

            return false;
        }

        // 서리 측 플레이어의 기본 색상 텍스처를 텍스처 저장소에 등록
        frostPlayerTextureSet_.baseColorTextureHandle = CreateTextureResource(frostBaseColorTexturePath);

        if (!frostPlayerTextureSet_.baseColorTextureHandle.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to create frost player Base Color texture resource"
                << ": source=" << frostBaseColorTexturePath
                << '\n';

            return false;
        }

        // 서리 측 플레이어의 노멀 텍스처를 텍스처 저장소에 등록
        frostPlayerTextureSet_.normalTextureHandle = CreateTextureResource(frostNormalMapTexturePath);

        if (!frostPlayerTextureSet_.normalTextureHandle.IsValid())
        {
            std::cerr
                << "[GameClient] Failed to create frost player Normal texture resource"
                << ": source=" << frostNormalMapTexturePath
                << '\n';

            return false;
        }

        // 아레나 맵 렌더 객체 구성
        RenderObject arenaMapObject;

        arenaMapObject.meshHandle = arenaMapMeshHandle_;
        arenaMapObject.baseColorTextureHandle = arenaMapBaseColorTextureHandle_;
        arenaMapObject.normalMapTextureHandle = arenaMapNormalMapTextureHandle_;

        // 일반 렌더 객체 목록에 아레나 맵 렌더 객체 등록
        renderObjects_.push_back(arenaMapObject);

        return true;
    }

    RenderObject ClientApplication::CreatePlayerRenderObject(const cna::network::PlayerStateSnapshot& playerState) const
    {
        RenderObject renderObject;

        // 모든 플레이어가 공유하는 공용 메쉬 핸들 등록
        renderObject.meshHandle = playerMeshHandle_;

        // 서버에서 전달받은 플레이어 진영에 따라 외형 텍스처 등록
        if (playerState.side == cna::PlayerSide::Flame)
        {
            renderObject.baseColorTextureHandle = flamePlayerTextureSet_.baseColorTextureHandle;
            renderObject.normalMapTextureHandle = flamePlayerTextureSet_.normalTextureHandle;
        }
        else if (playerState.side == cna::PlayerSide::Frost)
        {
            renderObject.baseColorTextureHandle = frostPlayerTextureSet_.baseColorTextureHandle;
            renderObject.normalMapTextureHandle = frostPlayerTextureSet_.normalTextureHandle;
        }

        return renderObject;
    }

    void ClientApplication::SynchronizePlayerRenderObjects(const cna::network::WorldStateSnapshot& worldState)
    {
        // 최신 월드 상태를 기준으로 구성할 플레이어 렌더 엔트리 목록 생성
        std::vector<PlayerRenderEntry> synchronizedPlayerRenderEntries;

        // 최신 월드 상태의 플레이어 수만큼 공간 예약
        synchronizedPlayerRenderEntries.reserve(worldState.players.size());

        // 최신 월드 상태의 모든 플레이어를 렌더 엔트리 목록에 동기화
        for (const cna::network::PlayerStateSnapshot& playerState : worldState.players)
        {
            PlayerRenderEntry* existingPlayerRenderEntry = nullptr;

            // 동일한 Player ID를 사용하는 기존 플레이어 렌더 엔트리 검색
            for (PlayerRenderEntry& playerRenderEntry : playerRenderEntries_)
            {
                if (playerRenderEntry.playerId == playerState.playerId)
                {
                    existingPlayerRenderEntry = &playerRenderEntry;

                    break;
                }
            }

            // 일치하는 플레이어 렌더 엔트리가 존재하는 경우 재사용
            if (existingPlayerRenderEntry)
            {
                synchronizedPlayerRenderEntries.push_back
                (
                    std::move(*existingPlayerRenderEntry)
                );
            }
            // 일치하는 플레이어 렌더 엔트리를 찾을 수 없는 경우 새로 생성
            else
            {
                synchronizedPlayerRenderEntries.push_back
                (
                    PlayerRenderEntry
                    {
                        playerState.playerId,
                        CreatePlayerRenderObject(playerState)
                    }
                );
            }

            PlayerRenderEntry& synchronizedPlayerRenderEntry = synchronizedPlayerRenderEntries.back();

            // 서버에서 전달받은 최신 플레이어 위치를 렌더 객체에 적용
            synchronizedPlayerRenderEntry.renderObject.transform.position =
            {
                playerState.positionX,
                playerState.positionY,
                playerState.positionZ
            };

            // 플레이어가 이동 중인 경우 서버 속도를 기준으로 이동 방향을 계산하여 렌더 방향 설정
            if (playerState.velocityX != 0.0f || playerState.velocityY != 0.0f)
            {
                synchronizedPlayerRenderEntry.renderObject.transform.rotationRadians.z = std::atan2(playerState.velocityX, -playerState.velocityY);
            }
        }

        // 최신 월드 상태에 대응하는 플레이어 렌더 엔트리 목록으로 교체
        playerRenderEntries_.swap(synchronizedPlayerRenderEntries);
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

    TextureHandle ClientApplication::CreateTextureResource(const std::filesystem::path& textureFilePath)
    {
        // 파일 경로 기반으로 WIC 디코딩을 수행하여 GPU 텍스처 리소스 생성
        std::unique_ptr<D3D11Texture> texture = renderer_.CreateTextureFromFile(textureFilePath);

        if (!texture)
        {
            return {};
        }

        // 생성된 GPU 텍스처 리소스를 텍스처 저장소에 등록
        return textureRepository_.AddTexture(std::move(texture));
    }

    bool ClientApplication::DrawRenderObject(const RenderObject& renderObject)
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

        return true;
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
        playerRenderEntries_.clear();

        // 아레나 맵 메쉬 및 텍스처 핸들 무효화
        arenaMapMeshHandle_ = {};
        arenaMapBaseColorTextureHandle_ = {};
        arenaMapNormalMapTextureHandle_ = {};

        // 플레이어 메쉬 및 텍스처 핸들 무효화
        playerMeshHandle_ = {};
        flamePlayerTextureSet_ = {};
        frostPlayerTextureSet_ = {};

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

        // 새로 할당받은 플레이어를 기준으로 마지막 입력 송신 상태 초기화
        lastSentPlayerInput_ = {};

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

        // 최신 월드 상태를 기준으로 플레이어 렌더 엔트리 목록 갱신
        SynchronizePlayerRenderObjects(*worldState);

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

    cna::network::PlayerInputPayload ClientApplication::CollectPlayerInput() const noexcept
    {
        cna::network::PlayerInputPayload input;

        // GameClient 윈도우가 현재 활성 윈도우가 아닌 경우 중립 입력 반환
        if (GetForegroundWindow() != window_.GetHandle())
        {
            return input;
        }

        // W 키가 눌린 경우 +Y 양의 방향 입력 적용
        if ((GetAsyncKeyState('W') & 0x8000) != 0)
        {
            input.moveY += cna::network::MaxPlayerInputAxisRawValue;
        }

        // A 키가 눌린 경우 X축 음의 방향 입력 적용
        if ((GetAsyncKeyState('A') & 0x8000) != 0)
        {
            input.moveX -= cna::network::MaxPlayerInputAxisRawValue;
        }

        // S 키가 눌린 경우 -Y 방향 입력 적용
        if ((GetAsyncKeyState('S') & 0x8000) != 0)
        {
            input.moveY -= cna::network::MaxPlayerInputAxisRawValue;
        }

        // D 키가 눌린 경우 X축 양의 방향 입력 적용
        if ((GetAsyncKeyState('D') & 0x8000) != 0)
        {
            input.moveX += cna::network::MaxPlayerInputAxisRawValue;
        }

        return input;
    }

    void ClientApplication::Update()
    {
        // 아직 Room 입장이 승인되지 않은 경우 플레이어 입력을 전송하지 않음
        if (!clientGameState_.HasPlayerIdentity())
        {
            return;
        }

        // 현재 키보드 상태를 플레이어 이동 입력으로 구성
        currentPlayerInput_ = CollectPlayerInput();

        // 현재 입력이 마지막으로 서버에 전송한 입력과 동일한 경우 추가로 전송하지 않음
        if (currentPlayerInput_.moveX == lastSentPlayerInput_.moveX &&
            currentPlayerInput_.moveY == lastSentPlayerInput_.moveY &&
            currentPlayerInput_.moveZ == lastSentPlayerInput_.moveZ
        )
        {
            return;
        }

        // 변경된 플레이어 입력을 서버 송신 큐에 등록
        if (!networkClient_->SendPlayerInput(currentPlayerInput_))
        {
            return;
        }

        // 송신 큐 등록에 성공한 경우 마지막 전송 입력 갱신
        lastSentPlayerInput_ = currentPlayerInput_;
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

        // 서버에서 Room 입장이 승인되어 로컬 플레이어 식별 정보를 받은 경우 게임 월드 출력
        if (clientGameState_.HasPlayerIdentity())
        {
            // 등록된 일반 렌더 객체를 순회하며 화면에 출력
            for (const RenderObject& renderObject : renderObjects_)
            {
                if (!DrawRenderObject(renderObject))
                {
                    return false;
                }
            }

            // 등록된 플레이어 렌더 객체를 순회하며 화면에 출력
            for (const PlayerRenderEntry& playerRenderEntry : playerRenderEntries_)
            {
                if (!DrawRenderObject(playerRenderEntry.renderObject))
                {
                    return false;
                }
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
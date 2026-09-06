#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace cna::client
{
    // DirectX 11 메쉬 데이터 관리 및 렌더링을 수행하기 위한 클래스
    class D3D11Mesh
    {
    public:
        D3D11Mesh() = default;

        ~D3D11Mesh()
        {
            Shutdown();
        }

        // 복사 생성자 및 복사 대입 연산자 삭제
        D3D11Mesh(const D3D11Mesh&) = delete;
        D3D11Mesh& operator=(const D3D11Mesh&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        D3D11Mesh(D3D11Mesh&&) noexcept = delete;
        D3D11Mesh& operator=(D3D11Mesh&&) noexcept = delete;

        // 임의의 정점 데이터 및 인덱스 데이터를 받아 정점 버퍼 및 입력 레이아웃을 생성하는 범용 메쉬 초기화 함수
        template <typename VertexType, typename IndexType>
        bool Initialize
        (
            ID3D11Device* device,
            const std::vector<VertexType>& vertices,
            const std::vector<IndexType>& indices,
            const D3D11_INPUT_ELEMENT_DESC* layoutDescs,
            UINT layoutDescCount,
            ID3DBlob* vertexShaderBlob
        );

        // Mesh를 그리기 위한 그래픽 파이프라인 설정 및 Draw call 호출 함수
        void Draw(ID3D11DeviceContext* context) const;

        // 정점 버퍼와 인덱스 버퍼, 입력 레이아웃이 생성되었는지 확인하는 함수
        bool IsInitialized() const noexcept;

        // 메쉬 관련 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // GPU 메모리에 생성되는 정점 데이터 버퍼
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
        // 정점 버퍼의 정점을 참조하는 인덱스 데이터 버퍼
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
        // 정점 버퍼의 데이터 구조를 정점 셰이더에 매칭하는 입력 레이아웃 객체
        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;

        // 정점 한 개가 차지하는 바이트 크기
        UINT vertexStride_ = 0;
        // 정점 버퍼에 저장된 총 정점의 개수
        UINT vertexCount_ = 0;
        // 인덱스 버퍼에 저장된 총 인덱스 개수
        UINT indexCount_ = 0;

        // 인덱스 버퍼의 데이터 포맷을 저장하는 변수
        DXGI_FORMAT indexFormat_ = DXGI_FORMAT_UNKNOWN;
    };
}

#include "d3d11_mesh.inl"
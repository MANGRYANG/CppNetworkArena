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

        // 임의의 정점 데이터를 받아 정점 버퍼 및 입력 레이아웃을 생성하는 범용 메쉬 초기화 함수
        template <typename VertexType>
        bool Initialize
        (
            ID3D11Device* device,
            const std::vector<VertexType>& vertices,
            const D3D11_INPUT_ELEMENT_DESC* layoutDescs,
            UINT layoutDescCount,
            ID3DBlob* vertexShaderBlob
        )
        {
            if (!device || !vertexShaderBlob || vertices.empty() || !layoutDescs || layoutDescCount == 0)
            {
                return false;
            }

            Shutdown();

            vertexStride_ = sizeof(VertexType);
            vertexCount_ = static_cast<UINT>(vertices.size());

            // 버퍼 설명자 구조체
            D3D11_BUFFER_DESC vertexBufferDesc = {};
            // 버퍼를 구성할 데이터 배열의 메모리 크기 설정
            vertexBufferDesc.ByteWidth = vertexStride_ * vertexCount_;
            // 버퍼의 사용 방식을 기본 버퍼 형태로 설정
            vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            // 버퍼의 사용 용도를 파이프라인의 정점 버퍼로 지정
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            // CPU가 버퍼에 직접 엑세스할 수 없도록 설정
            vertexBufferDesc.CPUAccessFlags = 0;
            // 기타 특수 기능 사용하지 않음
            vertexBufferDesc.MiscFlags = 0;
            // Structured Buffer가 아니므로 기본값으로 설정
            vertexBufferDesc.StructureByteStride = 0;

            // 생성될 버퍼에 채워 넣을 초기화 데이터
            D3D11_SUBRESOURCE_DATA initialData = {};
            // 정점 데이터 배열을 초기화 데이터로 설정
            initialData.pSysMem = vertices.data();

            // 정점 버퍼 생성
            HRESULT hr = device->CreateBuffer
            (
                &vertexBufferDesc,
                &initialData,
                vertexBuffer_.GetAddressOf()
            );

            // 버퍼 생성에 실패한 경우
            if (FAILED(hr))
            {
                Shutdown();
                return false;
            }

            // 입력 레이아웃 객체 생성
            hr = device->CreateInputLayout
            (
                layoutDescs,
                layoutDescCount,
                vertexShaderBlob->GetBufferPointer(),
                vertexShaderBlob->GetBufferSize(),
                inputLayout_.GetAddressOf()
            );

            // 입력 레이아웃 객체 생성에 실패한 경우
            if (FAILED(hr))
            {
                Shutdown();
                return false;
            }

            return true;
        }

        // Mesh를 그리기 위한 그래픽 파이프라인 설정 및 Draw call 호출 함수
        void Draw(ID3D11DeviceContext* context) const;

        // 정점 버퍼 및 입력 레이아웃이 생성되었는지 확인하는 함수
        bool IsInitialized() const noexcept;

        // 메쉬 관련 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // GPU 메모리에 생성되는 정점 데이터 버퍼
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
        // 정점 버퍼의 데이터 구조를 정점 셰이더에 매칭하는 입력 레이아웃 객체
        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;

        // 정점 한 개가 차지하는 바이트 크기
        UINT vertexStride_ = 0;
        // 버퍼에 저장된 총 정점의 개수
        UINT vertexCount_ = 0;
    };
}
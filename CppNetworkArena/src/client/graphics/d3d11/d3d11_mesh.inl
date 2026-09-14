#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace cna::client
{
    // 임의의 정점 데이터 및 인덱스 데이터를 받아 정점 버퍼 및 입력 레이아웃을 생성하는 범용 메쉬 초기화 함수
    template <typename VertexType, typename IndexType>
    bool D3D11Mesh::Initialize
    (
        ID3D11Device* device,
        const std::vector<VertexType>& vertices,
        const std::vector<IndexType>& indices,
        const D3D11_INPUT_ELEMENT_DESC* layoutDescs,
        UINT layoutDescCount,
        ID3DBlob* vertexShaderBlob
    )
    {
        if (!device || !vertexShaderBlob || vertices.empty() || indices.empty() || !layoutDescs || layoutDescCount == 0)
        {
            return false;
        }

        // 삼각형 목록을 완성할 수 없는 인덱스 개수인 경우 실패 처리
        if ((indices.size() % 3) != 0)
        {
            return false;
        }

        // 인덱스 타입이 uint16_t와 uint32_t 중 하나인지 확인
        static_assert
        (
            std::is_same_v<IndexType, std::uint16_t> ||
            std::is_same_v<IndexType, std::uint32_t>,
            "Only std::uint16_t and std::uint32_t are supported."
        );

        // D3D11_BUFFER_DESC::ByteWidth로 표현 가능한 최대 크기 계산
        constexpr std::size_t MaxD3D11BufferByteWidth = static_cast<std::size_t>(std::numeric_limits<UINT>::max());
        // IndexType 자료형으로 표현 가능한 최대 정점 개수
        constexpr std::size_t MaxIndexableVertexCount = static_cast<std::size_t>(std::numeric_limits<IndexType>::max()) + 1;

        // IndexType 타입 인덱스로 참조할 수 있는 정점 개수를 초과한 경우 실패 처리
        if (vertices.size() > MaxIndexableVertexCount)
        {
            return false;
        }

        // Direct3D 11 버퍼의 ByteWidth 범위를 초과하는 경우 실패 처리
        if (vertices.size() > (MaxD3D11BufferByteWidth / sizeof(VertexType)) ||
            indices.size() > (MaxD3D11BufferByteWidth / sizeof(IndexType))
        )
        {
            return false;
        }

        // 모든 인덱스가 전달된 정점 배열의 유효한 범위를 참조하는지 확인
        for (const IndexType index : indices)
        {
            if (static_cast<std::size_t>(index) >= vertices.size())
            {
                return false;
            }
        }

        // 기존에 생성된 메쉬 자원 정리
        Shutdown();

        // 정점 버퍼가 GPU 메모리 상에서 차지할 전체 바이트 크기
        const std::size_t vertexBufferByteWidth = sizeof(VertexType) * vertices.size();
        // 인덱스 버퍼가 GPU 메모리 상에서 차지할 전체 바이트 크기
        const std::size_t indexBufferByteWidth = sizeof(IndexType) * indices.size();

        // 정점 한 개가 차지하는 바이트 크기 계산
        vertexStride_ = static_cast<UINT>(sizeof(VertexType));
        // 정점 버퍼에 저장된 총 정점의 개수 계산
        vertexCount_ = static_cast<UINT>(vertices.size());
        // 인덱스 버퍼에 저장된 총 인덱스 개수 계산
        indexCount_ = static_cast<UINT>(indices.size());

        // 인덱스 자료형에 대응하는 DXGI 포맷 지정
        if constexpr (std::is_same_v<IndexType, std::uint16_t>)
        {
            indexFormat_ = DXGI_FORMAT_R16_UINT;
        }
        else
        {
            indexFormat_ = DXGI_FORMAT_R32_UINT;
        }

        // 정점 버퍼 설명자 구조체
        D3D11_BUFFER_DESC vertexBufferDesc = {};
        // 버퍼를 구성할 데이터 배열의 메모리 크기 설정
        vertexBufferDesc.ByteWidth = static_cast<UINT>(vertexBufferByteWidth);
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
        D3D11_SUBRESOURCE_DATA vertexInitialData = {};
        // 정점 데이터 배열을 초기화 데이터로 설정
        vertexInitialData.pSysMem = vertices.data();

        // 정점 버퍼 생성
        HRESULT hr = device->CreateBuffer
        (
            &vertexBufferDesc,
            &vertexInitialData,
            vertexBuffer_.GetAddressOf()
        );

        // 정점 버퍼 생성에 실패한 경우
        if (FAILED(hr))
        {
            Shutdown();
            return false;
        }

        // 인덱스 버퍼 설명자 구조체
        D3D11_BUFFER_DESC indexBufferDesc = {};
        // 버퍼를 구성할 데이터 배열의 메모리 크기 설정
        indexBufferDesc.ByteWidth = static_cast<UINT>(indexBufferByteWidth);
        // 버퍼의 사용 방식을 기본 버퍼 형태로 설정
        indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        // 버퍼의 사용 용도를 파이프라인의 인덱스 버퍼로 지정
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        // CPU가 버퍼에 직접 엑세스할 수 없도록 설정
        indexBufferDesc.CPUAccessFlags = 0;
        // 기타 특수 기능 사용하지 않음
        indexBufferDesc.MiscFlags = 0;
        // Structured Buffer가 아니므로 기본값으로 설정
        indexBufferDesc.StructureByteStride = 0;

        // 생성될 버퍼에 채워 넣을 초기화 데이터
        D3D11_SUBRESOURCE_DATA indexInitialData = {};
        indexInitialData.pSysMem = indices.data();

        // 인덱스 버퍼 생성
        hr = device->CreateBuffer
        (
            &indexBufferDesc,
            &indexInitialData,
            indexBuffer_.GetAddressOf()
        );

        // 인덱스 버퍼 생성에 실패한 경우
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
}
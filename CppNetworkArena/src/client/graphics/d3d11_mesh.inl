#pragma once

#include "D3D11_mesh.h"

namespace cna::client
{
    // 임의의 정점 데이터를 받아 정점 버퍼 및 입력 레이아웃을 생성하는 범용 메쉬 초기화 함수
    template <typename VertexType>
    bool D3D11Mesh::Initialize
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
}
#include "d3d11_mesh.h"

namespace cna::client
{
	void D3D11Mesh::Draw(ID3D11DeviceContext* context) const
	{
        // 메쉬 출력에 필요한 파이프라인 자원이 준비되지 않은 경우 중단
        if (!context || !IsInitialized())
        {
            return;
        }

        // 시작 오프셋을 0으로 설정
        const UINT vertexOffset = 0;

        // 입력 조립기 단계의 입력 레이아웃 설정
        context->IASetInputLayout(inputLayout_.Get());

        // 입력 조립기 단계의 정점 버퍼 설정
        context->IASetVertexBuffers
        (
            0,                              // 버퍼를 바인딩할 입력 슬롯 번호
            1,                              // 설정할 버퍼의 개수
            vertexBuffer_.GetAddressOf(),   // 정점 버퍼 포인터들이 담긴 배열
            &vertexStride_,                 // 정점 한 개가 차지하는 바이트 크기
            &vertexOffset                   // 버퍼 시작점으로부터의 바이트 오프셋
        );

        // 입력 조립기 단계의 인덱스 버퍼 설정
        context->IASetIndexBuffer
        (
            indexBuffer_.Get(),
            indexFormat_,
            0
        );

        // 정점과 인덱스를 삼각형 목록으로 해석하도록 입력 조립기 단계의 토폴로지 설정
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 인덱스 버퍼에 저장된 순서에 따라 메쉬 Draw Call
        context->DrawIndexed(indexCount_, 0, 0);
	}

    bool D3D11Mesh::IsInitialized() const noexcept
    {
        return vertexBuffer_ && indexBuffer_ && inputLayout_ &&
            vertexStride_ > 0 && vertexCount_ > 0 && indexCount_ > 0 && indexFormat_ != DXGI_FORMAT_UNKNOWN;;
    }

    void D3D11Mesh::Shutdown() noexcept
    {
        inputLayout_.Reset();
        vertexBuffer_.Reset();
        indexBuffer_.Reset();

        vertexStride_ = 0;
        vertexCount_ = 0;
        indexCount_ = 0;
        indexFormat_ = DXGI_FORMAT_UNKNOWN;
    }
}
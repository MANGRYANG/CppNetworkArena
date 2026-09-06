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

        // 입력 조립기 단계의 데이터 형식 설정
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Draw call 호출
        context->Draw(vertexCount_, 0);
	}

    bool D3D11Mesh::IsInitialized() const noexcept
    {
        return vertexBuffer_ && inputLayout_;
    }

    void D3D11Mesh::Shutdown() noexcept
    {
        inputLayout_.Reset();
        vertexBuffer_.Reset();

        vertexStride_ = 0;
        vertexCount_ = 0;
    }
}
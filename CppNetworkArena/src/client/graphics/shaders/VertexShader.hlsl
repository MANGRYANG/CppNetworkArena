// 3D 월드 공간을 클립 공간으로 변환하기 위한 카메라 상수 버퍼
cbuffer CameraData : register(b0)
{
    row_major matrix viewProjectionMatrix;
}

// 로컬 공간을 월드 공간으로 변환하기 위한 객체 상수 버퍼
cbuffer ObjectData : register(b1)
{
    row_major matrix worldMatrix;
}

// Vertex Shader의 입력으로 전달되는 데이터 구조체
struct VertexInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

// Vertex Shader 연산 후 래스터라이저로 넘길 데이터 구조체
struct PixelInput
{
    // 4차원 동차 좌표계 위치
    float4 position : SV_POSITION;
    // Pixel Shader로 전달할 색상 데이터
    float4 color : COLOR;
};

// 모든 정점마다 실행되는 정점 셰이더의 Entry Point
PixelInput VSMain(VertexInput input)
{
    PixelInput output;
    
    // 로컬 공간 좌표를 객체별 월드 공간 좌표로 변환
    const float4 worldPosition = mul
    (
        float4(input.position, 1.0f),
        worldMatrix
    );
    
    // 월드 공간 좌표를 뷰-투영 결합 행렬을 사용하여 클립 공간으로 변환
    output.position = mul
    (
        worldPosition,
        viewProjectionMatrix
    );
    
    // 정점 색상을 Pixel Shader로 전달
    output.color = input.color;
    
    return output;
}
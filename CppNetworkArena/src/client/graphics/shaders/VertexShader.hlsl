// 고정된 2D 가상 화면을 클립 공간으로 변환하기 위한 투영 상수 버퍼
cbuffer ProjectionData : register(b0)
{
    row_major matrix projectionMatrix;
}

// Vertex Shader의 입력으로 전달되는 데이터 구조체
struct VertexInput
{
    float2 position : POSITION;
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
    
    // 2D 가상 화면 좌표를 투영 행렬을 사용하여 클립 공간 좌표로 변환
    output.position = mul
    (
        float4(input.position, 0.0f, 1.0f),
        projectionMatrix
    );
    
    // 정점 색상을 Pixel Shader로 전달
    output.color = input.color;
    
    return output;
}
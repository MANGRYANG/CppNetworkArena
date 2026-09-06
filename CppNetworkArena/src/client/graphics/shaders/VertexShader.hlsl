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
    
    // 2D 정점 위치를 래스터라이저 처리를 위한 동차 좌표계로 확장
    output.position = float4(input.position, 0.0f, 1.0f);
    
    // 정점 색상을 Pixel Shader로 전달
    output.color = input.color;
    
    return output;
}
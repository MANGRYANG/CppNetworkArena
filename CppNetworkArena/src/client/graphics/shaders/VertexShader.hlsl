// 3D 월드 공간을 클립 공간으로 변환하기 위한 카메라 상수 버퍼
cbuffer CameraData : register(b0)
{
    row_major matrix viewProjectionMatrix;
}

// 로컬 공간을 월드 공간으로 변환하기 위한 객체 상수 버퍼
cbuffer ObjectData : register(b1)
{
    row_major matrix worldMatrix;
    row_major matrix normalMatrix;
}

// Vertex Shader의 입력으로 전달되는 데이터 구조체
struct VertexInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 textureCoordinate : TEXCOORD;
};

// Vertex Shader 연산 후 래스터라이저로 넘길 데이터 구조체
struct PixelInput
{
    // 4차원 동차 좌표계 위치
    float4 position : SV_POSITION;
    // Pixel Shader로 전달할 색상 데이터
    float4 color : COLOR;
    // Pixel Shader로 전달할 월드 공간 정점 노멀 데이터
    float3 normal : NORMAL;
    // Pixel Shader로 전달할 월드 공간 탄젠트 데이터 및 방향 복원 부호
    float4 tangent : TANGENT;
    // Pixel Shader로 전달할 텍스처 좌표 데이터
    float2 textureCoordinate : TEXCOORD;
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
    
    // 노멀 변환 행렬의 이동 성분 제거 후 행렬곱
    output.normal = mul(input.normal, (float3x3) normalMatrix);
    
    // 월드 변환 행렬의 이동 성분을 제외하고 탄젠트 방향을 월드 공간으로 변환
    output.tangent.xyz = mul(input.tangent.xyz, (float3x3) worldMatrix);
    output.tangent.w = input.tangent.w;

    // 텍스처 좌표를 Pixel Shader로 전달
    output.textureCoordinate = input.textureCoordinate;
    
    return output;
}
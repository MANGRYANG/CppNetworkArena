// 기본 색상 텍스처를 픽셀 셰이더에서 읽기 위한 Texture Resource
Texture2D baseColorTexture : register(t0);

// 기본 색상 텍스처의 UV 샘플링 규칙을 정의하는 Sampler State
SamplerState baseColorSampler : register(s0);

// 현재 Draw Call에서 Base Color 텍스처 사용 여부를 전달하는 머티리얼 상수 버퍼
cbuffer MaterialData : register(b0)
{
    uint useBaseColorTexture;
    float3 materialPadding;
}

// 래스터라이저가 전달하는 입력 데이터 구조체
struct PixelInput
{
    // 래스터라이저가 계산한 화면 공간 픽셀 위치
    float4 position : SV_POSITION;
    // 각 픽셀 위치에 맞게 보간된 색상 데이터
    float4 color : COLOR;
    // 각 픽셀 위치에 맞게 보간된 정점 노멀 데이터
    float3 normal : NORMAL;
    // 각 픽셀 위치에 맞게 보간된 텍스처 좌표 데이터
    float2 textureCoordinate : TEXCOORD;
};

// 각 픽셀마다 독립적으로 실행되는 픽셀 셰이더의 Entry Point
float4 PSMain(PixelInput input) : SV_TARGET
{
    // Base Color 텍스처가 연결된 머티리얼이면 UV 좌표를 사용하여 실제 텍스처 색상 반환
    if (useBaseColorTexture != 0)
    {
        return baseColorTexture.Sample(baseColorSampler, input.textureCoordinate);
    }

    // 텍스처가 없는 머티리얼은 기존 정점 색상을 유지
    return input.color;
}
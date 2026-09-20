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

// Directional Light의 조명 계산에 필요한 데이터를 전달하는 조명 상수 버퍼
cbuffer LightingData : register(b1)
{
    float3 directionToLight;
    float ambientIntensity;

    float3 lightColor;
    float diffuseIntensity;
}

// 래스터라이저가 전달하는 입력 데이터 구조체
struct PixelInput
{
    // 래스터라이저가 계산한 화면 공간 픽셀 위치
    float4 position : SV_POSITION;
    // 각 픽셀 위치에 맞게 보간된 색상 데이터
    float4 color : COLOR;
    // 각 픽셀 위치에 맞게 보간된 월드 공간 정점 노멀 데이터
    float3 normal : NORMAL;
    // 각 픽셀 위치에 맞게 보간된 텍스처 좌표 데이터
    float2 textureCoordinate : TEXCOORD;
};

// 각 픽셀마다 독립적으로 실행되는 픽셀 셰이더의 Entry Point
float4 PSMain(PixelInput input) : SV_TARGET
{
    // 텍스처가 없는 머티리얼은 기존 정점 색상을 표면 기본 색상으로 사용
    float4 surfaceColor = input.color;

    // 기본 색상 텍스처가 연결된 머티리얼이면 UV 좌표를 사용하여 실제 텍스처 색상 사용
    if (useBaseColorTexture != 0)
    {
        surfaceColor = baseColorTexture.Sample(baseColorSampler, input.textureCoordinate);
    }

    const float normalLengthSquared = dot(input.normal, input.normal);

    float lambertFactor = 0.0f;

    // 노멀 데이터가 존재하는 경우에만 Lambert 계수 계산
    if (normalLengthSquared > 0.000001f)
    {
        // 래스터라이저 보간 과정에서 노멀 길이가 변경되었을 수 있으므로 정규화
        const float3 normalizedNormal = input.normal * rsqrt(normalLengthSquared);

        lambertFactor = saturate(dot(normalizedNormal, directionToLight));
    }

    // Directional Light의 Diffuse 성분과 Ambient Light를 합산하여 최종 표면 밝기 계산
    const float3 lighting =
        (lightColor * lambertFactor * diffuseIntensity) +
        float3(ambientIntensity, ambientIntensity, ambientIntensity);

    // Alpha 값은 기본 색상 텍스처의 값을 유지하고 RGB에만 조명을 적용
    return float4(surfaceColor.rgb * lighting, surfaceColor.a);
}
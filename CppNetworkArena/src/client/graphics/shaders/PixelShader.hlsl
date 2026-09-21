// 기본 색상 텍스처를 픽셀 셰이더에서 읽기 위한 Texture Resource
Texture2D baseColorTexture : register(t0);

// 노멀 맵 텍스처를 픽셀 셰이더에서 읽기 위한 Texture Resource
Texture2D normalMapTexture : register(t1);

// 기본 색상 텍스처와 노멀 텍스처의 UV 샘플링 규칙을 정의하는 Sampler State
SamplerState textureSampler : register(s0);

// 현재 Draw Call에서 Base Color 및 노멀 텍스처 사용 여부를 전달하는 머티리얼 상수 버퍼
cbuffer MaterialData : register(b0)
{
    uint useBaseColorTexture;
    uint useNormalMapTexture;
    float2 materialPadding;
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
    // 각 픽셀 위치에 맞게 보간된 월드 공간 탄젠트 데이터 및 방향 복원 부호
    float4 tangent : TANGENT;
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
        surfaceColor = baseColorTexture.Sample(textureSampler, input.textureCoordinate);
    }

    const float normalLengthSquared = dot(input.normal, input.normal);

    float lambertFactor = 0.0f;

    // 노멀 데이터가 존재하는 경우에만 Lambert 계수 계산
    if (normalLengthSquared > 0.000001f)
    {
        // 래스터라이저 보간 과정에서 노멀 길이가 변경되었을 수 있으므로 정규화
        const float3 normalizedNormal = input.normal * rsqrt(normalLengthSquared);

        float3 lightingNormal = normalizedNormal;

        // 노멀 맵 텍스처를 사용하는 경우
        if (useNormalMapTexture != 0)
        {
            const float tangentLengthSquared = dot(input.tangent.xyz, input.tangent.xyz);

            // 탄젠트 데이터가 존재하는 경우
            if (tangentLengthSquared > 0.000001f)
            {
                // 래스터라이저 보간 과정에서 탄젠트 길이가 변경되었을 수 있으므로 정규화
                float3 normalizedTangent = input.tangent.xyz * rsqrt(tangentLengthSquared);

                // 그람-슈미트 직교화 과정을 통해 탄젠트 벡터를 노멀 벡터에 직교하도록 재정렬
                normalizedTangent = normalizedTangent - normalizedNormal * dot(normalizedNormal, normalizedTangent);

                // 직교화 과정을 거쳐 재정렬된 탄젠트 벡터의 유효성 검증
                const float orthogonalTangentLengthSquared = dot(normalizedTangent, normalizedTangent);

                if (orthogonalTangentLengthSquared > 0.000001f)
                {
                    // 탄젠트 벡터를 단위 벡터 크기로 정규화
                    normalizedTangent *= rsqrt(orthogonalTangentLengthSquared);

                    // 노멀 벡터와 탄젠트 벡터의 외적 결과와 전달된 부호 값을 통해 이중법선 벡터 복원
                    const float tangentHandedness = input.tangent.w < 0.0f ? -1.0f : 1.0f;
                    const float3 normalizedBitangent = cross(normalizedNormal, normalizedTangent) * tangentHandedness;

                    // 노멀 맵 텍스처의 [0, 1] 범위를 탄젠트 공간의 [-1, 1] 범위로 언패킹
                    const float3 tangentSpaceNormal = (normalMapTexture.Sample(textureSampler, input.textureCoordinate).xyz * 2.0f) -1.0f;

                    // 탄젠트 공간에서의 노멀 벡터 유효성 검증
                    const float tangentSpaceNormalLengthSquared = dot(tangentSpaceNormal, tangentSpaceNormal);

                    if (tangentSpaceNormalLengthSquared > 0.000001f)
                    {
                        // 탄젠트 공간에서의 노멀 벡터 정규화
                        const float3 normalizedTangentSpaceNormal = tangentSpaceNormal * rsqrt(tangentSpaceNormalLengthSquared);

                        // TBN 기저를 사용하여 Tangent Space 노멀을 월드 공간 노멀로 변환
                        lightingNormal = normalize
                        (
                            normalizedTangentSpaceNormal.x * normalizedTangent +
                            normalizedTangentSpaceNormal.y * normalizedBitangent +
                            normalizedTangentSpaceNormal.z * normalizedNormal
                        );
                    }
                }
            }
        }

        lambertFactor = saturate(dot(lightingNormal, directionToLight));
    }

    // Directional Light의 Diffuse 성분과 Ambient Light를 합산하여 최종 표면 밝기 계산
    const float3 lighting =
        (lightColor * lambertFactor * diffuseIntensity) +
        float3(ambientIntensity, ambientIntensity, ambientIntensity);

    // Alpha 값은 기본 색상 텍스처의 값을 유지하고 RGB에만 조명을 적용
    return float4(surfaceColor.rgb * lighting, surfaceColor.a);
}
// 래스터라이저가 전달하는 입력 데이터 구조체
struct PixelInput
{
    // 래스터라이저가 계산한 화면 공간 픽셀 위치
    float4 position : SV_POSITION;
    // 각 픽셀 위치에 맞게 보간된 색상 데이터
    float4 color : COLOR;
};

// 각 픽셀마다 독립적으로 실행되는 픽셀 셰이더의 Entry Point
float4 PSMain(PixelInput input) : SV_TARGET
{
    // 색상 데이터를 변환하지 않고 반환
    return input.color;
}
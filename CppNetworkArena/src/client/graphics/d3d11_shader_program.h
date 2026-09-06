#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <filesystem>

namespace cna::client
{
    // DirectX 11 셰이더의 생성 및 관리를 위한 클래스
    class D3D11ShaderProgram final
    {
    public:
        D3D11ShaderProgram() = default;
        ~D3D11ShaderProgram() = default;

        // 복사 생성자 및 복사 대입 연산자 삭제
        D3D11ShaderProgram(const D3D11ShaderProgram&) = delete;
        D3D11ShaderProgram& operator=(const D3D11ShaderProgram&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        D3D11ShaderProgram(D3D11ShaderProgram&&) = delete;
        D3D11ShaderProgram& operator=(D3D11ShaderProgram&&) = delete;

        // HLSL 파일을 컴파일하고 정점 및 픽셀 셰이더를 생성하는 함수
        bool Initialize
        (
            ID3D11Device* device,
            const std::filesystem::path& vertexShaderPath,
            const std::filesystem::path& pixelShaderPath
        );

        // 디바이스 컨텍스트의 파이프라인에 셰이더를 바인딩하는 함수
        void Bind(ID3D11DeviceContext* deviceContext) const noexcept;

        // 정점 셰이더 블롭 객체를 가리키는 포인터를 반환하는 함수
        ID3DBlob* GetVertexShaderBlob() const noexcept;

        // 정점 셰이더와 픽셀 셰이더가 모두 생성되었는지 확인하는 함수
        bool IsInitialized() const noexcept;

        // 생성된 셰이더 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // 셰이더 파일 경로를 받아 HLSL 셰이더 코드를 바이너리로 컴파일하는 함수
        static bool CompileShaderFromFile
        (
            const std::filesystem::path& filePath,
            const char* entryPoint,
            const char* target,
            Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob
        );

        // 컴파일 완료 후 생성된 정점 셰이더 객체
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
        // 컴파일 완료 후 생성된 픽셀 셰이더 객체
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;

        // 컴파일된 정점 셰이더의 원시 바이너리 데이터를 보관하는 블롭 객체
        Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob_;
        // 컴파일된 픽셀 셰이더의 원시 바이너리 데이터를 보관하는 블롭 객체
        Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob_;
    };
}
#include "d3d11_shader_program.h"

#include <iostream>
#include <string>

namespace cna::client
{
    bool D3D11ShaderProgram::Initialize(ID3D11Device* device, const std::filesystem::path& vertexShaderPath, const std::filesystem::path& pixelShaderPath)
    {
        // 전달된 인자가 유효하지 않거나 이미 초기화된 경우 실패 처리
        if (!device || vertexShaderPath.empty() || pixelShaderPath.empty() || IsInitialized())
        {
            return false;
        }

        // 정점 셰이더 컴파일
        if (!CompileShaderFromFile(vertexShaderPath, "VSMain", "vs_5_0", vertexShaderBlob_))
        {
            Shutdown();

            return false;
        }

        // 정점 셰이더 객체 생성
        HRESULT hr = device->CreateVertexShader
        (
            vertexShaderBlob_->GetBufferPointer(),
            vertexShaderBlob_->GetBufferSize(),
            nullptr,
            vertexShader_.GetAddressOf()
        );

        // 정점 셰이더 객체 생성에 실패한 경우
        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        // 픽셀 셰이더 컴파일
        if (!CompileShaderFromFile(pixelShaderPath, "PSMain", "ps_5_0", pixelShaderBlob_))
        {
            Shutdown();

            return false;
        }

        // 픽셀 셰이더 객체 생성
        hr = device->CreatePixelShader
        (
            pixelShaderBlob_->GetBufferPointer(),
            pixelShaderBlob_->GetBufferSize(),
            nullptr,
            pixelShader_.GetAddressOf()
        );

        // 픽셀 셰이더 객체 생성에 실패한 경우
        if (FAILED(hr))
        {
            Shutdown();

            return false;
        }

        return true;
    }

    void D3D11ShaderProgram::Bind(ID3D11DeviceContext* deviceContext) const noexcept
    {
        // 디바이스 컨텍스트 또는 셰이더가 준비되지 않은 경우
        if (!deviceContext || !IsInitialized())
        {
            return;
        }

        // 정점 셰이더 객체를 정점 셰이더 단계에 바인딩
        deviceContext->VSSetShader(vertexShader_.Get(), nullptr, 0);

        // 픽셀 셰이더 객체를 픽셀 셰이더 단계에 바인딩
        deviceContext->PSSetShader(pixelShader_.Get(), nullptr, 0);
    }

    ID3DBlob* D3D11ShaderProgram::GetVertexShaderBlob() const noexcept
    {
        return vertexShaderBlob_.Get();
    }

    bool D3D11ShaderProgram::IsInitialized() const noexcept
    {
        return vertexShader_ && pixelShader_ && vertexShaderBlob_ && pixelShaderBlob_;
    }

    void D3D11ShaderProgram::Shutdown() noexcept
    {
        pixelShader_.Reset();
        vertexShader_.Reset();
        vertexShaderBlob_.Reset();
        pixelShaderBlob_.Reset();
    }

    bool D3D11ShaderProgram::CompileShaderFromFile(const std::filesystem::path& filePath, const char* entryPoint, const char* target, Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob)
    {
        // 컴파일 시 사용할 특수 기능을 담는 변수
        UINT compileFlags = 0;

        // Debug 모드로 빌드하는 경우 디버그 정보를 출력 코드에 삽입하고 최적화를 건너뛰도록 설정
    #if defined(_DEBUG)
        compileFlags |= D3DCOMPILE_DEBUG;
        compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    #endif

        // 에러 보관용 블롭
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        // HLSL 셰이더 파일에 대한 컴파일 작업 수행
        HRESULT hr = D3DCompileFromFile
        (
            filePath.c_str(),                   // 파일 경로
            nullptr,                            // 매크로 정의 배열 (사용하지 않음)
            D3D_COMPILE_STANDARD_FILE_INCLUDE,  // Include 파일을 처리하는 데 사용되는 처리기 설정
            entryPoint,                         // 셰이더의 Entry Point
            target,                             // 타겟 셰이더 모델
            compileFlags,                       // 셰이더 컴파일 옵션
            0,                                  // 효과 컴파일 옵션 (사용하지 않음)
            shaderBlob.GetAddressOf(),          // 컴파일된 셰이더 블롭 객체를 가리키는 포인터
            errorBlob.GetAddressOf()            // 컴파일 에러 메시지 블롭 객체를 가리키는 포인터
        );

        // 셰이더 파일에 대한 컴파일에 실패한 경우
        if (FAILED(hr))
        {
            std::cerr << "[D3D11ShaderProgram] Failed to compile shader: " << filePath.string() << '\n';

            // 컴파일 에러 메시지가 존재하는 경우
            if (errorBlob)
            {
                std::cerr
                    << "Shader Compiler Output: "
                    << static_cast<const char*>(errorBlob->GetBufferPointer())
                    << '\n';
            }

            std::cerr
                << "HRESULT=0x"
                << std::hex << std::uppercase << static_cast<unsigned long>(hr)
                << std::dec << '\n';

            return false;
        }

        return true;
    }
}
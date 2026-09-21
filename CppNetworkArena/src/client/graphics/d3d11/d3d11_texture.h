#pragma once

#include <Windows.h>

#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace cna::client
{
    // 텍스처와 Shader Resource View를 관리하는 클래스
    class D3D11Texture final
    {
    public:
        D3D11Texture() = default;
        ~D3D11Texture();

        // 복사 생성자 및 복사 대입 연산자 삭제
        D3D11Texture(const D3D11Texture&) = delete;
        D3D11Texture& operator=(const D3D11Texture&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        D3D11Texture(D3D11Texture&&) = delete;
        D3D11Texture& operator=(D3D11Texture&&) = delete;

        // 외부 텍스처 이미지 파일을 WIC로 디코딩하여 GPU 텍스처 리소스를 생성하는 함수
        bool InitializeFromFile
        (
            ID3D11Device* device,
            IWICImagingFactory* imagingFactory,
            const std::filesystem::path& filePath
        );

        // PNG나 JPEG와 같은 압축 이미지 데이터를 WIC로 디코딩하여 GPU 텍스처 리소스를 생성하는 함수
        bool InitializeFromEncodedMemory
        (
            ID3D11Device* device,
            IWICImagingFactory* imagingFactory,
            std::span<const std::byte> encodedData
        );

        // Assimp가 제공하는 BGRA 8비트 픽셀 배열을 사용하여 GPU 텍스처를 생성하는 함수
        bool InitializeFromBgraPixels
        (
            ID3D11Device* device,
            std::uint32_t width,
            std::uint32_t height,
            std::span<const std::byte> bgraPixels
        );

        // 픽셀 셰이더에 바인딩할 Shader Resource View를 반환하는 함수
        ID3D11ShaderResourceView* GetShaderResourceView() const noexcept;

        // 텍스처와 Shader Resource View가 생성되었는지 확인하는 함수
        bool IsInitialized() const noexcept;

        // 생성된 DirectX 11 텍스처 자원을 정리하는 함수
        void Shutdown() noexcept;

    private:
        // WIC 디코더의 첫 번째 프레임을 공통 RGBA 8비트 텍스처로 변환하는 함수
        bool InitializeFromDecoder
        (
            ID3D11Device* device,
            IWICImagingFactory* imagingFactory,
            IWICBitmapDecoder* decoder
        );

        // WIC 비트맵 소스를 RGBA 8비트 픽셀 데이터로 변환하는 함수
        bool InitializeFromBitmapSource
        (
            ID3D11Device* device,
            IWICImagingFactory* imagingFactory,
            IWICBitmapSource* bitmapSource
        );

        // RGBA 8비트 픽셀 배열로 텍스처와 Shader Resource View를 생성하는 함수
        bool InitializeFromRgbaPixels
        (
            ID3D11Device* device,
            std::uint32_t width,
            std::uint32_t height,
            std::span<const std::byte> rgbaPixels
        );

        // 실제 픽셀 데이터를 보관하는 텍스처 리소스
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;

        // 픽셀 셰이더에서 텍스처를 읽기 위한 Shader Resource View
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView_;
    };
}
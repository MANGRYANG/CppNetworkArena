#include "d3d11_texture.h"

#include <shlwapi.h>

#include <limits>
#include <utility>
#include <vector>

namespace
{
    constexpr std::size_t BytesPerRgbaPixel = 4;

    // 너비와 높이를 기반으로 RGBA 픽셀 버퍼 크기를 계산하는 함수
    bool TryCalculateRgbaBufferSize
    (
        const std::uint32_t width, const std::uint32_t height,
        std::size_t& rowPitch,
        std::size_t& bufferSize
    )
    {
        if (width == 0 || height == 0)
        {
            return false;
        }

        const std::size_t widthSize = static_cast<std::size_t>(width);
        const std::size_t heightSize = static_cast<std::size_t>(height);

        if (widthSize > std::numeric_limits<std::size_t>::max() / BytesPerRgbaPixel)
        {
            return false;
        }

        rowPitch = widthSize * BytesPerRgbaPixel;

        if (heightSize > std::numeric_limits<std::size_t>::max() / rowPitch)
        {
            return false;
        }

        bufferSize = rowPitch * heightSize;

        return true;
    }
}

namespace cna::client
{
    D3D11Texture::~D3D11Texture()
    {
        Shutdown();
    }

    bool D3D11Texture::InitializeFromFile(ID3D11Device* device, IWICImagingFactory* imagingFactory, const std::filesystem::path& filePath)
    {
        if (!device || !imagingFactory || filePath.empty() || IsInitialized())
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;

        // 파일 이미지 형식을 자동 판별하여 WIC 디코더 생성
        const HRESULT hr = imagingFactory->CreateDecoderFromFilename
        (
            filePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        return InitializeFromDecoder(device, imagingFactory, decoder.Get());
    }

    bool D3D11Texture::InitializeFromEncodedMemory(ID3D11Device* device, IWICImagingFactory* imagingFactory, const std::span<const std::byte> encodedData)
    {
        if (!device || !imagingFactory || encodedData.empty() || IsInitialized())
        {
            return false;
        }

        // SHCreateMemStream이 UINT 크기를 사용하므로 지원 범위를 벗어난 데이터는 거부
        if (encodedData.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max()))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IStream> stream;

        // 전달된 이미지 데이터를 복사하여 독립적인 메모리 스트림 생성
        stream.Attach
        (
            SHCreateMemStream
            (
                reinterpret_cast<const BYTE*>(encodedData.data()),
                static_cast<UINT>(encodedData.size())
            )
        );

        // 스트림을 생성하지 못한 경우 실패 처리
        if (!stream)
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;

        // 메모리 스트림의 이미지 형식을 자동 판별하여 WIC 디코더 생성
        const HRESULT hr = imagingFactory->CreateDecoderFromStream
        (
            stream.Get(),
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        return InitializeFromDecoder(device, imagingFactory, decoder.Get());
    }

    bool D3D11Texture::InitializeFromBgraPixels(ID3D11Device* device, const std::uint32_t width, const std::uint32_t height, const std::span<const std::byte> bgraPixels)
    {
        if (!device || bgraPixels.empty() || IsInitialized())
        {
            return false;
        }

        std::size_t rowPitch = 0;
        std::size_t bufferSize = 0;

        if (!TryCalculateRgbaBufferSize(width, height, rowPitch, bufferSize) ||
            bgraPixels.size() < bufferSize ||
            rowPitch > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            bufferSize > static_cast<std::size_t>(std::numeric_limits<UINT>::max()))
        {
            return false;
        }

        // 픽셀 데이터 배열의 BGRA 메모리 레이아웃을 RGBA로 변환
        std::vector<std::byte> rgbaPixels(bgraPixels.begin(), bgraPixels.end());

        for (std::size_t pixelOffset = 0; pixelOffset < bufferSize; pixelOffset += BytesPerRgbaPixel)
        {
            std::swap(rgbaPixels[pixelOffset + 0], rgbaPixels[pixelOffset + 2]);
        }

        return InitializeFromRgbaPixels(device, width, height, std::span<const std::byte>(rgbaPixels));
    }

    ID3D11ShaderResourceView* D3D11Texture::GetShaderResourceView() const noexcept
    {
        return shaderResourceView_.Get();
    }

    bool D3D11Texture::IsInitialized() const noexcept
    {
        return texture_ && shaderResourceView_;
    }

    void D3D11Texture::Shutdown() noexcept
    {
        shaderResourceView_.Reset();
        texture_.Reset();
    }

    bool D3D11Texture::InitializeFromDecoder(ID3D11Device* device, IWICImagingFactory* imagingFactory, IWICBitmapDecoder* decoder)
    {
        if (!device || !imagingFactory || !decoder || IsInitialized())
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;

        // 텍스처 이미지의 첫 번째 프레임 조회
        const HRESULT getTextureFrame = decoder->GetFrame(0, frame.GetAddressOf());

        if (FAILED(getTextureFrame))
        {
            return false;
        }

        return InitializeFromBitmapSource(device, imagingFactory, frame.Get());
    }

    bool D3D11Texture::InitializeFromBitmapSource(ID3D11Device* device, IWICImagingFactory* imagingFactory, IWICBitmapSource* bitmapSource)
    {
        if (!device || !imagingFactory || !bitmapSource || IsInitialized())
        {
            return false;
        }

        UINT width = 0;
        UINT height = 0;

        HRESULT hr = bitmapSource->GetSize(&width, &height);
        
        // 비트맵 소스의 크기를 가져오지 못했거나 유효하지 않은 크기인 경우
        if (FAILED(hr) || width == 0 || height == 0)
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> formatConverter;

        // 다양한 WIC 이미지 형식을 공통 RGBA 8비트 형식으로 변환하기 위한 컨버터 객체 생성
        hr = imagingFactory->CreateFormatConverter(formatConverter.GetAddressOf());

        if (FAILED(hr))
        {
            return false;
        }

        // 컨버터 객체 초기화
        hr = formatConverter->Initialize
        (
            bitmapSource,
            GUID_WICPixelFormat32bppPRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.f,
            WICBitmapPaletteTypeCustom
        );

        if (FAILED(hr))
        {
            return false;
        }

        std::size_t rowPitch = 0;
        std::size_t bufferSize = 0;

        if (!TryCalculateRgbaBufferSize(width, height, rowPitch, bufferSize) ||
            rowPitch > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            bufferSize > static_cast<std::size_t>(std::numeric_limits<UINT>::max()))
        {
            return false;
        }

        std::vector<std::byte> rgbaPixels(bufferSize);

        // 변환된 RGBA 픽셀 데이터를 CPU 버퍼로 복사
        hr = formatConverter->CopyPixels
        (
            nullptr,    // 전체 비트맵 범위 복사
            static_cast<UINT>(rowPitch),
            static_cast<UINT>(bufferSize),
            reinterpret_cast<BYTE*>(rgbaPixels.data())
        );

        if (FAILED(hr))
        {
            return false;
        }

        return InitializeFromRgbaPixels(device, width, height, std::span<const std::byte>(rgbaPixels));
    }

    bool D3D11Texture::InitializeFromRgbaPixels(ID3D11Device* device, const std::uint32_t width, const std::uint32_t height, const std::span<const std::byte> rgbaPixels)
    {
        if (!device || rgbaPixels.empty() || IsInitialized())
        {
            return false;
        }

        std::size_t rowPitch = 0;
        std::size_t bufferSize = 0;

        if (!TryCalculateRgbaBufferSize(width, height, rowPitch, bufferSize) ||
            rgbaPixels.size() < bufferSize ||
            rowPitch > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            bufferSize > static_cast<std::size_t>(std::numeric_limits<UINT>::max()))
        {
            return false;
        }

        // 텍스처 설명자 구조체 구성
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;

        // 초기 데이터 설정
        D3D11_SUBRESOURCE_DATA initialData = {};
        initialData.pSysMem = rgbaPixels.data();
        initialData.SysMemPitch = static_cast<UINT>(rowPitch);
        initialData.SysMemSlicePitch = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

        // RGBA 픽셀 데이터를 사용하여 GPU 텍스처 리소스 생성
        HRESULT hr = device->CreateTexture2D
        (
            &textureDesc,
            &initialData,
            texture.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;

        // 생성한 텍스처를 픽셀 셰이더에서 읽을 수 있도록 Shader Resource View 생성
        hr = device->CreateShaderResourceView
        (
            texture.Get(),
            nullptr,
            shaderResourceView.GetAddressOf()
        );

        if (FAILED(hr))
        {
            return false;
        }

        texture_ = std::move(texture);
        shaderResourceView_ = std::move(shaderResourceView);

        return true;
    }
}
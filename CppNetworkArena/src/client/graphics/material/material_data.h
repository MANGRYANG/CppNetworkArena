#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace cna::client
{
    // 모델의 머티리얼이 참조하는 텍스처가 외부 파일인지 모델 내부 데이터인지 구분하기 위해 사용하는 열거형
    enum class ModelTextureSourceType : std::uint8_t
    {
        ExternalFile,
        Embedded
    };

    // 외부 모델에서 불러온 텍스처 원본 데이터를 CPU 메모리에 보관하는 구조체
    struct ModelTextureData final
    {
        ModelTextureSourceType sourceType = ModelTextureSourceType::ExternalFile;

        std::filesystem::path filePath;
        std::string sourceReference;
        std::vector<std::byte> embeddedData;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::string formatHint;

        // 모델 내부에 포함된 텍스처 데이터인지 확인하는 함수
        bool IsEmbedded() const noexcept
        {
            return sourceType == ModelTextureSourceType::Embedded;
        }

        // 모델 내부 텍스처가 PNG나 JPEG 같은 압축 데이터인지 확인하는 함수
        bool IsCompressedEmbedded() const noexcept
        {
            return IsEmbedded() && height == 0;
        }
    };

    inline constexpr std::size_t InvalidTextureIndex = std::numeric_limits<std::size_t>::max();

    // 모델 머티리얼의 이름과 기본 색상 및 노멀 맵 텍스처 연결 정보를 보관하는 구조체
    struct ModelMaterialData final
    {
        std::string name;
        std::size_t baseColorTextureIndex = InvalidTextureIndex;
        std::size_t normalMapTextureIndex = InvalidTextureIndex;
    };
}
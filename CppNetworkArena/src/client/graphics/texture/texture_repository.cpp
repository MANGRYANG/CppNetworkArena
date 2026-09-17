#include "texture_repository.h"

#include <limits>
#include <memory>
#include <utility>

namespace cna::client
{
    TextureRepository::~TextureRepository()
    {
        Clear();
    }

    TextureHandle TextureRepository::AddTexture(std::unique_ptr<D3D11Texture> texture)
    {
        // 생성되지 않았거나 초기화되지 않은 GPU 텍스처는 등록하지 않음
        if (!texture || !texture->IsInitialized())
        {
            return {};
        }

        // 식별자 범위를 모두 소진한 경우 텍스처를 추가할 수 없음
        if (nextTextureId_ == 0)
        {
            return {};
        }

        // 추가할 텍스처에 할당할 텍스처 식별자
        const std::uint64_t textureId = nextTextureId_;

        // GPU 텍스처 리소스의 소유권을 저장소에 등록
        const bool inserted = textures_.emplace(textureId, std::move(texture)).second;

        // 등록에 실패한 경우
        if (!inserted)
        {
            return {};
        }

        // 핸들 값의 재사용을 방지하기 위해 식별자 단조 증가
        if (nextTextureId_ == std::numeric_limits<std::uint64_t>::max())
        {
            nextTextureId_ = 0;
        }
        else
        {
            ++nextTextureId_;
        }

        return TextureHandle(textureId);
    }

    bool TextureRepository::RemoveTexture(const TextureHandle textureHandle)
    {
        // 유효하지 않은 텍스처 핸들인 경우 실패 처리
        if (!textureHandle.IsValid())
        {
            return false;
        }

        // 핸들에 대응하는 GPU 텍스처의 소유권 해제
        return textures_.erase(textureHandle.value_) > 0;
    }

    const D3D11Texture* TextureRepository::FindTexture(const TextureHandle textureHandle) const
    {
        // 유효하지 않은 텍스처 핸들인 경우 실패 처리
        if (!textureHandle.IsValid())
        {
            return nullptr;
        }

        const auto textureIterator = textures_.find(textureHandle.value_);

        // 텍스처 핸들에 해당하는 텍스처를 저장소에서 찾지 못한 경우
        if (textureIterator == textures_.end())
        {
            return nullptr;
        }

        // 소유권은 전달하지 않고 렌더링에 사용할 읽기 전용 포인터만 반환
        return textureIterator->second.get();
    }

    void TextureRepository::Clear() noexcept
    {
        textures_.clear();
    }
}
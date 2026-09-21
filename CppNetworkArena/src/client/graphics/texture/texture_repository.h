#pragma once

#include "graphics/d3d11/d3d11_texture.h"
#include "texture_handle.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace cna::client
{
    // 생성된 GPU 텍스처 리소스의 소유권 및 생명 주기를 관리하는 저장소
    class TextureRepository final
    {
    public:
        TextureRepository() = default;
        ~TextureRepository();

        // 복사 생성자 및 복사 대입 연산자 삭제
        TextureRepository(const TextureRepository&) = delete;
        TextureRepository& operator=(const TextureRepository&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        TextureRepository(TextureRepository&&) = delete;
        TextureRepository& operator=(TextureRepository&&) = delete;

        // GPU 텍스처를 저장소에 추가하고 핸들을 반환하는 함수
        TextureHandle AddTexture(std::unique_ptr<D3D11Texture> texture);

        // 전달된 핸들에 대응하는 텍스처를 저장소에서 제거하는 함수
        bool RemoveTexture(TextureHandle textureHandle);

        // 전달된 핸들에 대응하는 텍스처를 조회하는 함수
        const D3D11Texture* FindTexture(TextureHandle textureHandle) const;

        // 저장소가 소유한 모든 텍스처를 제거하는 함수
        void Clear() noexcept;

    private:
        // 핸들 값과 GPU 텍스처 소유권을 연결하여 저장하는 unordered_map
        std::unordered_map<std::uint64_t, std::unique_ptr<D3D11Texture>> textures_;

        // 새 텍스처에 할당할 단조 증가 식별자
        std::uint64_t nextTextureId_ = 1;
    };
}
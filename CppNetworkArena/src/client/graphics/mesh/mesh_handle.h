#pragma once

#include <cstdint>

namespace cna::client
{
    class MeshRepository;

    // 메쉬 저장소가 소유한 메쉬를 식별하기 위한 핸들
    class MeshHandle final
    {
    public:
        MeshHandle() = default;

        // 유효한 메쉬를 가리키는 핸들인지 확인하는 함수
        bool IsValid() const noexcept
        {
            return value_ != 0;
        }

        bool operator==(const MeshHandle& other) const noexcept
        {
            return value_ == other.value_;
        }

        bool operator!=(const MeshHandle& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        // friend 클래스인 MeshRepository 클래스에서만 호출할 수 있도록 private로 설정
        explicit MeshHandle(const std::uint64_t value) noexcept
            : value_(value)
        {
        }

        // 0은 유효하지 않은 핸들 값으로 사용
        std::uint64_t value_ = 0;

        friend class MeshRepository;
    };
}
#pragma once

#include "graphics/d3d11/d3d11_mesh.h"
#include "mesh_handle.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace cna::client
{
    // 생성된 DirectX 11 메쉬의 소유권과 생명 주기를 관리하는 저장소
    class MeshRepository final
    {
    public:
        MeshRepository() = default;
        ~MeshRepository();

        // 복사 생성자 및 복사 대입 연산자 삭제
        MeshRepository(const MeshRepository&) = delete;
        MeshRepository& operator=(const MeshRepository&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        MeshRepository(MeshRepository&&) = delete;
        MeshRepository& operator=(MeshRepository&&) = delete;

        // GPU 메쉬를 저장소에 추가하고 핸들을 반환하는 함수
        MeshHandle AddMesh(std::unique_ptr<D3D11Mesh> mesh);

        // 전달된 핸들에 대응하는 메쉬를 저장소에서 제거하는 함수
        bool RemoveMesh(MeshHandle meshHandle);

        // 전달된 핸들에 대응하는 메쉬를 조회하는 함수
        const D3D11Mesh* FindMesh(MeshHandle meshHandle) const;

        // 저장소가 소유한 모든 메쉬를 제거하는 함수
        void Clear() noexcept;

    private:
        // 핸들 값과 GPU 메쉬 소유권을 연결하여 저장하는 unordered_map
        std::unordered_map<std::uint64_t, std::unique_ptr<D3D11Mesh>> meshes_;

        // 새 메쉬에 할당할 단조 증가 식별자
        std::uint64_t nextMeshId_ = 1;
    };
}
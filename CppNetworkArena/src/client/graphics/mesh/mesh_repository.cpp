#include "mesh_repository.h"

#include <limits>
#include <memory>
#include <utility>

namespace cna::client
{
    MeshRepository::~MeshRepository()
    {
        Clear();
    }

    MeshHandle MeshRepository::AddMesh(std::unique_ptr<D3D11Mesh> mesh)
    {
        // 생성되지 않았거나 초기화되지 않은 GPU 메쉬는 등록하지 않음
        if (!mesh || !mesh->IsInitialized())
        {
            return {};
        }

        // 식별자 범위를 모두 소진한 경우 메쉬 추가 불가
        if (nextMeshId_ == 0)
        {
            return {};
        }

        // 추가할 메쉬에 할당할 메쉬 식별자
        const std::uint64_t meshId = nextMeshId_;

        // GPU 메쉬 소유권을 저장소에 등록
        const bool inserted = meshes_.emplace(meshId, std::move(mesh)).second;

        // 등록에 실패한 경우
        if (!inserted)
        {
            return {};
        }

        // 핸들 값의 재사용을 방지하기 위해 식별자 단조 증가
        if (nextMeshId_ == std::numeric_limits<std::uint64_t>::max())
        {
            nextMeshId_ = 0;
        }
        else
        {
            ++nextMeshId_;
        }

        return MeshHandle(meshId);
    }

    bool MeshRepository::RemoveMesh(const MeshHandle meshHandle)
    {
        // 유효하지 않은 메쉬 핸들인 경우 실패 처리
        if (!meshHandle.IsValid())
        {
            return false;
        }

        // 핸들에 대응하는 GPU 메쉬의 소유권 해제
        return meshes_.erase(meshHandle.value_) > 0;
    }

    const D3D11Mesh* MeshRepository::FindMesh(const MeshHandle meshHandle) const
    {
        // 유효하지 않은 메쉬 핸들인 경우 실패 처리
        if (!meshHandle.IsValid())
        {
            return nullptr;
        }

        const auto meshIterator = meshes_.find(meshHandle.value_);

        // 메쉬 핸들에 해당하는 메쉬를 저장소에서 찾지 못한 경우
        if (meshIterator == meshes_.end())
        {
            return nullptr;
        }

        // 소유권은 전달하지 않고 렌더링에 사용할 읽기 전용 포인터만 반환
        return meshIterator->second.get();
    }

    void MeshRepository::Clear() noexcept
    {
        meshes_.clear();
    }
}
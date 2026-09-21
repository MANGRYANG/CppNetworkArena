#pragma once

#include "graphics/mesh/mesh_handle.h"
#include "graphics/render/transform3d.h"
#include "graphics/texture/texture_handle.h"

namespace cna::client
{
    // 공유 메쉬와 객체별 변환 정보를 연결하는 렌더 객체 구조체
    struct RenderObject final
    {
        // 메쉬 저장소에 등록된 GPU 메쉬 리소스에 접근하기 위한 핸들
        MeshHandle meshHandle;

        // 텍스처 저장소에 등록된 Base Color 텍스처 리소스에 접근하기 위한 핸들
        TextureHandle baseColorTextureHandle;

        // 텍스처 저장소에 등록된 Normal Map 텍스처 리소스에 접근하기 위한 핸들
        TextureHandle normalMapTextureHandle;

        // 객체마다 독립적으로 적용되는 위치, 회전, 스케일 변환 정보
        Transform3D transform;
    };
}
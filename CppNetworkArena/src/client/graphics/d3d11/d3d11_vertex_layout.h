#pragma once

#include "graphics/mesh/vertex3d.h"

#include <d3d11.h>

#include <array>
#include <cstddef>
#include <type_traits>

namespace cna::client
{
    // 멤버 오프셋을 안전하게 계산할 수 있는 정점 구조인지 확인
    static_assert(std::is_standard_layout_v<Vertex3D>);

    // Vertex3D 구조체에 대응하는 Input Layout 설명자 배열
    const D3D11_INPUT_ELEMENT_DESC Vertex3DInputLayoutDescs[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            static_cast<UINT>(offsetof(Vertex3D, position)),
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            static_cast<UINT>(offsetof(Vertex3D, color)),
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    };
}
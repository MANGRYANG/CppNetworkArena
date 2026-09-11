#include "unit_quad_mesh.h"

namespace cna::client
{
    MeshData CreateUnitQuadMeshData()
    {
        MeshData meshData;

        // 월드 변환으로 크기와 위치를 결정할 수 있도록 한 변의 길이를 1로 구성
        meshData.vertices =
        {
            { { -0.5f,  0.5f, 0.0f }, { 0.10f, 0.75f, 1.00f, 1.00f } },
            { {  0.5f,  0.5f, 0.0f }, { 0.20f, 0.35f, 1.00f, 1.00f } },
            { { -0.5f, -0.5f, 0.0f }, { 0.75f, 0.20f, 1.00f, 1.00f } },
            { {  0.5f, -0.5f, 0.0f }, { 1.00f, 0.75f, 0.20f, 1.00f } }
        };

        meshData.indices =
        {
            0, 1, 2,
            2, 1, 3
        };

        return meshData;
    }
}
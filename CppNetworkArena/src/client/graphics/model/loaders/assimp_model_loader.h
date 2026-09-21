#pragma once

#include "graphics/model/model_data.h"

#include <DirectXMath.h>

#include <filesystem>
#include <string>

namespace cna::client
{
    // 외부 정적 모델의 로딩 결과와 오류 메시지를 함께 전달하는 구조체
    struct AssimpModelLoadResult final
    {
        ModelData modelData;
        std::string errorMessage;

        // 렌더링 가능한 모델 데이터가 생성됐는지 확인하는 함수
        bool Succeeded() const noexcept;
    };

    // 외부 모델 파일을 정적 ModelData로 변환하는 함수
    AssimpModelLoadResult LoadStaticModelData(const std::filesystem::path& modelFilePath);
}
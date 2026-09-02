#include "application/client_application.h"

#include <Windows.h>

#include <exception>
#include <iostream>

int main(void)
{
    try
    {
        // 현재 GameClient 실행 모듈의 Win32 인스턴스 핸들 가져오기
        const HINSTANCE instanceHandle = GetModuleHandleW(nullptr);

        // 인스턴스 핸들을 가져오지 못한 경우
        if (!instanceHandle)
        {
            std::cerr
                << "[GameClient] Failed to get module instance handle"
                << '\n';

            return 1;
        }

        // Win32 창과 네트워크 생명 주기를 공유하는 애플리케이션 객체 생성
        cna::client::ClientApplication application(instanceHandle);

        return application.Run();
    }

    // 예외 처리
    catch (const std::exception& exception)
    {
        std::cerr << "Client fatal error: " << exception.what() << '\n';

        return 1;
    }
}
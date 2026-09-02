#pragma once

#include <Windows.h>

#include <string_view>

namespace cna::client
{
    // Win32 윈도우의 생성 및 소멸을 관리하기 위한 클래스
    class Win32Window final
    {
    public:
        explicit Win32Window(HINSTANCE hInstance) noexcept;
        ~Win32Window();

        // 복사 생성자 및 복사 대입 연산자 삭제
        Win32Window(const Win32Window&) = delete;
        Win32Window& operator=(const Win32Window&) = delete;

        // 이동 생성자 및 이동 대입 연산자 삭제
        Win32Window(Win32Window&&) = delete;
        Win32Window& operator=(Win32Window&&) = delete;

        // 지정한 클라이언트 영역 크기의 Win32 윈도우를 생성하는 함수
        bool Create(std::wstring_view title, int clientWidth, int clientHeight);

        // 생성된 윈도우를 화면에 표시하는 함수
        void Show() const noexcept;

        // 생성된 윈도우와 등록된 윈도우 클래스를 정리하는 함수
        void Destroy() noexcept;

        // 운영체제 메시지를 확인하여 처리하는 함수
        void ProcessMessages(bool& running);

        // 생성된 윈도우의 핸들을 반환하는 함수
        HWND GetHandle() const noexcept;

        // 윈도우 생성 여부를 반환하는 함수
        bool IsCreated() const noexcept;

    private:
        // Win32 메시지를 현재 윈도우 객체로 전달하는 정적 윈도우 프로시저
        static LRESULT CALLBACK StaticWindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        // 현재 윈도우 객체에 전달된 Win32 메시지를 처리하는 함수
        LRESULT WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        // 현재 실행 모듈(.exe)의 인스턴스 핸들
        HINSTANCE hInstance_ = nullptr;

        // 생성할 윈도우의 핸들
        HWND hwnd_ = nullptr;

        // 윈도우 클래스명
        const wchar_t* WindowClassName = L"CNAGameClientWindow";

        // 현재 객체의 Win32 윈도우 클래스 등록 여부
        bool classRegistered_ = false;
    };
}
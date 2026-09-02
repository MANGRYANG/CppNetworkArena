#include "win32_window.h"

#include <string>

namespace cna::client
{
    Win32Window::Win32Window(const HINSTANCE hInstance) noexcept
        : hInstance_(hInstance)
    {
    }

    Win32Window::~Win32Window()
    {
        Destroy();
    }

    bool Win32Window::Create(const std::wstring_view title, const int clientWidth, const int clientHeight)
    {
        // 실행 모듈이 없거나 클라이언트 영역 크기가 유효하지 않은 경우 생성 거부
        if (!hInstance_ || clientWidth <= 0 || clientHeight <= 0)
        {
            return false;
        }

        // 이미 운영체제 윈도우가 생성된 경우 중복 생성 거부
        if (hwnd_)
        {
            return false;
        }

        // 현재 GameClient 객체가 사용할 Win32 윈도우 클래스 등록
        if (!classRegistered_)
        {
            // 윈도우 클래스 구조체 설정
            WNDCLASSEXW wc = {};
            // 구조체 크기 명시
            wc.cbSize = sizeof(WNDCLASSEXW);
            // 클래스 스타일 설정
            wc.style = CS_HREDRAW | CS_VREDRAW;
            // 윈도우 프로시저 함수 주소 설정
            wc.lpfnWndProc = &Win32Window::StaticWindowProcedure;
            // 인스턴스 핸들 설정
            wc.hInstance = hInstance_;
            // 윈도우 클래스 커서 핸들 설정
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            // 윈도우 클래스 배경 브러시 설정
            wc.hbrBackground = nullptr;
            // 윈도우 클래스명 등록
            wc.lpszClassName = WindowClassName;

            // 윈도우 클래스 등록에 실패한 경우
            if (!RegisterClassExW(&wc))
            {
                // 메시지 박스 플로팅
                MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);

                return false;
            }

            // 윈도우 클래스 등록 여부 설정
            classRegistered_ = true;
        }

        // 윈도우 크기 확보를 위한 RECT 구조체 설정
        RECT windowRect = {};
        windowRect.left = 0;
        windowRect.top = 0;
        windowRect.right = clientWidth;
        windowRect.bottom = clientHeight;

        // 테두리를 포함한 윈도우 크기 계산
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        // 최종 윈도우 너비
        const int windowWidth = windowRect.right - windowRect.left;

        // 최종 윈도우 높이
        const int windowHeight = windowRect.bottom - windowRect.top;

        const std::wstring windowTitle(title);

        // 윈도우 생성
        hwnd_ = CreateWindowExW
        (
            0,                      // 확장 윈도우 스타일 (기본값)
            WindowClassName,        // 윈도우 클래스명
            windowTitle.c_str(),    // 윈도우 타이틀
            WS_OVERLAPPEDWINDOW,    // 윈도우 스타일
            CW_USEDEFAULT,          // 화면 X 좌표
            CW_USEDEFAULT,          // 화면 Y 좌표
            windowWidth,            // 윈도우 너비
            windowHeight,           // 윈도우 높이
            nullptr,                // 부모 윈도우
            nullptr,                // 메뉴 바 핸들
            hInstance_,             // 인스턴스 핸들
            this                    // 추가 파라미터
        );

        return (hwnd_ != nullptr);
    }

    void Win32Window::Show() const noexcept
    {
        // 윈도우가 생성되지 않은 경우 무시
        if (!hwnd_)
        {
            return;
        }

        // 윈도우 표시 상태 설정
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        // 메시지 큐를 우회하여 윈도우 프로시저애 WM_PAINT 메시지 즉시 전달
        UpdateWindow(hwnd_);
    }

    void Win32Window::Destroy() noexcept
    {
        // 생성된 윈도우가 남아 있는 경우 제거
        if (hwnd_)
        {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }

        // 현재 객체가 등록한 Win32 윈도우 클래스 해제
        if (classRegistered_)
        {
            UnregisterClassW(WindowClassName, hInstance_);

            classRegistered_ = false;
        }
    }

    HWND Win32Window::GetHandle() const noexcept
    {
        return hwnd_;
    }

    bool Win32Window::IsCreated() const noexcept
    {
        return (hwnd_ != nullptr);
    }

    void Win32Window::ProcessMessages(bool& running)
    {
        MSG msg = {};

        // 메시지 큐에 메시지가 존재하는 경우
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;

                return;
            }

            // 가상 키 메시지를 문자 메시지로 변환
            TranslateMessage(&msg);

            // 메시지를 윈도우 프로시저로 전달 (WndProc)
            DispatchMessageW(&msg);
        }
    }

    LRESULT CALLBACK Win32Window::StaticWindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Win32Window* window = nullptr;

        // 윈도우가 처음 만들어지는 시점인 경우
        if (uMsg == WM_NCCREATE)
        {
            // 윈도우 생성 정보 구조체 가져오기
            CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);

            // CreateWindowEx의 lpParam으로 전달한 값 추출 (this 포인터)
            window = reinterpret_cast<Win32Window*>(createStruct->lpCreateParams);

            if (window)
            {
                // 추출한 객체 주소를 윈도우 내부 데이터 영역(GWLP_USERDATA)에 저장
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

                // CreateWindowExW으로 생성되었던 윈도우 핸들 보관
                window->hwnd_ = hwnd;
            }
        }

        // 윈도우 생성 이후 시점인 경우
        else
        {
            // 윈도우 내부 데이터 영역(GWLP_USERDATA)에서 윈도우 객체 주소 추출
            window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        // 윈도우와 연결된 객체가 존재하지 않는 경우
        if (!window)
        {
            // 기본 윈도우 프로시저 호출
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }

        // WindowProc 함수 호출
        return window->WindowProcedure(hwnd, uMsg, wParam, lParam);
    }

    LRESULT Win32Window::WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_CLOSE:              // 사용자가 닫기 버튼을 눌렀을 때
            DestroyWindow(hwnd);    // 윈도우 제거 함수 호출
            
            return 0;

        case WM_DESTROY:            // 윈도우 제거 함수가 호출되었을 때
            PostQuitMessage(0);     // 애플리케이션 종료 함수 호출
            
            return 0;

        case WM_NCDESTROY:                              // 윈도우 제거 함수가 종료되었을 때
            hwnd_ = nullptr;                            // 제거된 윈도우 핸들을 더 이상 사용하지 않도록 초기화
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);  // 윈도우 핸들과 C++ 객체 사이의 연결 제거

            // 기본 윈도우 프로시저로 전달
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);

        default:
            // 기본 윈도우 프로시저로 전달
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }
}
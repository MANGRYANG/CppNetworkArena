# CMake 최소 버전 설정
cmake_minimum_required (VERSION 3.20)

# CMake 프로젝트 정의
project(CppNetworkArena VERSION 0.2.0 LANGUAGES CXX)

# 프로젝트 전체 C++ 컴파일 기준 설정
set(CMAKE_CXX_STANDARD 20)                  # C++20 표준 사용
set(CMAKE_CXX_STANDARD_REQUIRED ON)         # 지원되지 않을 경우 낮은 표준으로 대체하지 않고 오류 처리
set(CMAKE_CXX_EXTENSIONS OFF)               # 컴파일러별 확장 방언 대신 ISO C++ 표준 모드 사용

# Visual Studio에서 타깃을 논리적 폴더로 분류할 수 있도록 설정
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# vcpkg가 설치한 Boost.Asio 패키지 탐색
find_package(Boost CONFIG REQUIRED COMPONENTS asio)

# ---------------------------------------------------------------------------------------------
# Shared
# ---------------------------------------------------------------------------------------------

# 서버-클라이언트 공용 헤더 전용 타깃 생성 (CppNetworkArenaShared)
# INTERFACE 타깃이므로 별도의 .lib 파일은 생성하지 않음
add_library(CppNetworkArenaShared INTERFACE)

# CppNetworkArenaShared를 사용하는 타깃에 공용 헤더 검색 경로를 전달
target_include_directories(CppNetworkArenaShared
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
)

# 서버와 클라이언트에서 공유하는 메시지 헤더 등록
target_sources(
    CppNetworkArenaShared
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/network/message_header.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/network/message_type.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/network/message_codec.h
)

# ---------------------------------------------------------------------------------------------
# Server
# ---------------------------------------------------------------------------------------------

# 서버 실행 파일 생성 (GameServer.exe)
add_executable(GameServer
    src/server/main.cpp
    src/server/network/server.cpp
    src/server/network/server.h
    src/server/network/session.cpp
    src/server/network/session.h
    src/server/network/session_types.h
    src/server/network/session_manager.h
    src/server/network/session_manager.cpp
)

# GameServer 타깃을 빌드할 때 CppNetworkArenaShared 타깃과 Boost.Asio를 링크하도록 설정
target_link_libraries(GameServer
    PRIVATE
        CppNetworkArenaShared
        Boost::asio
)

# Visual Studio에서 GameServer 타깃이 Apps 폴더 아래에 표시되도록 설정
set_target_properties(GameServer PROPERTIES FOLDER "Apps")

# ---------------------------------------------------------------------------------------------
# Client
# ---------------------------------------------------------------------------------------------

# 클라이언트 실행 파일 생성 (GameClient.exe)
add_executable(GameClient
    src/client/main.cpp
)

# GameClient 타깃을 빌드할 때 CppNetworkArenaShared 타깃을 링크하도록 설정
target_link_libraries(GameClient
    PRIVATE
        CppNetworkArenaShared
)

# Visual Studio에서 GameClient 타깃이 Apps 폴더 아래에 표시되도록 설정
set_target_properties(GameClient PROPERTIES FOLDER "Apps")
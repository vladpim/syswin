cmake_minimum_required(VERSION 3.15)
project(SystemInfo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sysinfo main.cpp)

if(WIN32)
    target_link_libraries(sysinfo
        psapi
        advapi32
        iphlpapi
        secur32
        winmm
        ws2_32
        shell32
    )
endif()

if(MSVC)
    target_compile_options(sysinfo PRIVATE /EHa)
endif()

if(MSVC)
    target_compile_definitions(sysinfo PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
endif()

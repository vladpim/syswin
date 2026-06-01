cmake_minimum_required(VERSION 3.15)
project(SystemInfo)

# Устанавливаем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем исполняемый файл
add_executable(sysinfo main.cpp)

# Для Windows указываем необходимые библиотеки
if(WIN32)
    target_link_libraries(sysinfo
        psapi
        advapi32
        iphlpapi
        secur32      # для GetUserNameExW
        winmm
        ws2_32
        shell32
        # sddl.lib не требуется — функции из него уже есть в advapi32
    )
endif()

if(MSVC)
    target_compile_options(sysinfo PRIVATE /EHa)
endif()

# Для MSVC отключаем предупреждения о небезопасных функциях и включаем Unicode
if(MSVC)
    target_compile_definitions(sysinfo PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
endif()
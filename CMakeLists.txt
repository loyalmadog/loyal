cmake_minimum_required(VERSION 3.20)
set(CMAKE_RC_COMPILER "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe")
project(loyal VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)


FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)


FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90
)
FetchContent_MakeAvailable(imgui)


set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui_lib STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_lib PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC d3d11 d3dcompiler dwmapi)


file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(loyal WIN32 ${SOURCES})


target_include_directories(loyal PRIVATE src)


target_link_libraries(loyal PRIVATE 
    imgui_lib 
    spdlog::spdlog 
    advapi32 
    psapi 
    wintrust 
    crypt32 
    wevtapi 
    shlwapi
    setupapi
)


if (MSVC)
    set_target_properties(loyal PROPERTIES 
        LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /PDBALTPATH:none"
    )
endif()


add_custom_command(TARGET loyal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/fonts"
        "$<TARGET_FILE_DIR:loyal>/fonts"
    COMMENT "Copie des polices dans le dossier de sortie..."
)

add_custom_command(TARGET loyal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/fileless.ps1"
        "$<TARGET_FILE_DIR:loyal>/fileless.ps1"
    COMMENT "Copie de fileless.ps1 dans le dossier de sortie..."
)

add_custom_command(TARGET loyal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/usn_check.ps1"
        "$<TARGET_FILE_DIR:loyal>/usn_check.ps1"
    COMMENT "Copie de usn_check.ps1 dans le dossier de sortie..."
)






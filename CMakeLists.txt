cmake_minimum_required(VERSION 3.30)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXE_NAME QMAP_txt_extractor)
project(${EXE_NAME} LANGUAGES CXX)

option(ENABLE_COVERAGE "Build with gcov-compatible coverage instrumentation" OFF)

set(LOCAL_DEPS "${CMAKE_SOURCE_DIR}/dependencies")
set(IO_PLATFORM_DIR "${LOCAL_DEPS}/IO_Platform")
set(GLAD_DIR "${LOCAL_DEPS}/glad1")

function(qmap_enable_coverage target_name)
    if(ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target_name} PRIVATE --coverage -O0 -g)
            target_link_options(${target_name} PRIVATE --coverage)
        else()
            message(WARNING "ENABLE_COVERAGE is only wired for GCC/Clang-style gcov builds")
        endif()
    endif()
endfunction()

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR "${imgui_SOURCE_DIR}")

find_package(SDL3 CONFIG REQUIRED)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(${EXE_NAME} WIN32)   # WIN32 makes program open without dos window
    if (MSVC)
        set_target_properties(
            ${EXE_NAME} PROPERTIES
            LINK_FLAGS "/ENTRY:mainCRTStartup"
        )
    endif()
    target_compile_definitions(
        ${EXE_NAME}
        PUBLIC WINDOWS
    )

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(${EXE_NAME})
    target_compile_definitions(
        ${EXE_NAME}
        PUBLIC LINUX
    )
endif()

add_library(qmap_parsers STATIC)
target_sources(
    qmap_parsers PRIVATE
    src/map_txt_parser.cpp
    src/map_map_parser.cpp
    src/B_Endian/B_Endian.cpp
    ${IO_PLATFORM_DIR}/io_Platform.cpp
)
target_include_directories(
    qmap_parsers PUBLIC
    src
    ${IO_PLATFORM_DIR}
)
qmap_enable_coverage(qmap_parsers)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(qmap_parsers PUBLIC WINDOWS)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(qmap_parsers PUBLIC LINUX)
endif()

target_sources(
    ${EXE_NAME} PRIVATE
    src/main.cpp
    src/map_txt_gui.cpp
)
qmap_enable_coverage(${EXE_NAME})

add_library(qmap_imgui STATIC
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.h
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.h
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)
qmap_enable_coverage(qmap_imgui)

add_subdirectory(${GLAD_DIR})

target_include_directories(
    qmap_imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# ImGui needs its own SDL3 links
target_link_libraries(
    qmap_imgui PRIVATE
    SDL3::SDL3
)
find_package(OpenGL REQUIRED)
target_link_libraries(
    ${EXE_NAME} PRIVATE
    qmap_parsers
    qmap_imgui
    glad
    SDL3::SDL3
    OpenGL::GL
)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)

    add_executable(qmap_parser_tests
        tests/parser_tests.cpp
        tests/foundation_tests.cpp
    )
    target_link_libraries(
        qmap_parser_tests PRIVATE
        qmap_parsers
        Catch2::Catch2WithMain
    )
    target_compile_definitions(
        qmap_parser_tests PRIVATE
        TEST_MAPS_DIR="${CMAKE_SOURCE_DIR}/test_maps"
    )
    qmap_enable_coverage(qmap_parser_tests)
    add_test(NAME qmap_parser_tests COMMAND qmap_parser_tests)
    set_tests_properties(
        qmap_parser_tests PROPERTIES
        ENVIRONMENT "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
    )
endif()


set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-source")

set(CPACK_SOURCE_IGNORE_FILES
  "/\\.git/"
  "/_build/"
  "/build/"
  "/build-.*/"
  "/out/"
  "/\\.vs/"
  "/\\.vscode/"
  "/vendor-source-package/"
  "/CMakeFiles/"
  "/CMakeCache\\.txt$"
  "/\\.ninja_log$"
  "/\\.ninja_deps$"
  "/_CPack_Packages/"
  "/CPackConfig\\.cmake$"
  "/CPackSourceConfig\\.cmake$"
)

include(CPack)

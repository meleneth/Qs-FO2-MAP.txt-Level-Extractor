cmake_minimum_required(VERSION 3.25)

project(QMAP_txt_editor LANGUAGES C CXX)

set(EXE_NAME QMAP_txt_editor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(cmake/Dependencies.cmake)

option(USE_LOCAL_IO_PLATFORM "Use local IO_Platform checkout" OFF)
option(USE_VENDORED_DEPS "Use bundled dependency sources" OFF)


set(IO_PLATFORM_SOURCE_DIR "../IO_Platform" CACHE PATH "Path to local IO_Platform checkout")

if(USE_LOCAL_IO_PLATFORM)
  message(STATUS "Using local IO_Platform: ${IO_PLATFORM_SOURCE_DIR}")

  add_subdirectory(
    "${IO_PLATFORM_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/IO_platform-build"
  )

elseif(USE_VENDORED_DEPS AND EXISTS "${CMAKE_SOURCE_DIR}/vendor/IO_platform/CMakeLists.txt")
  message(STATUS "Using vendored IO_platform")

  add_subdirectory(
    "${CMAKE_SOURCE_DIR}/vendor/IO_platform"
    "${CMAKE_BINARY_DIR}/vendor/IO_platform-build"
  )

else()
  message(STATUS "Fetching IO_platform")

  FetchContent_Declare(
    IO_platform
    GIT_REPOSITORY https://github.com/Meleneth/IO_platform.git
    GIT_TAG        6d7783d6d785752f590dd2ae5ac26845000e099e
    GIT_SHALLOW    FALSE
  )

  FetchContent_MakeAvailable(IO_Platform)
endif()

set(APP_SOURCES
  src/main.cpp
  src/map_txt_parser.cpp
  src/map_txt_gui.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND APP_SOURCES
    src/X11_drag_drop.cpp
  )
endif()

add_executable(${EXE_NAME}
  ${APP_SOURCES}
)

target_compile_definitions(${EXE_NAME}
  PRIVATE
    $<$<PLATFORM_ID:Windows>:WINDOWS>
    $<$<PLATFORM_ID:Linux>:LINUX>
)

find_package(OpenGL REQUIRED)

target_link_libraries(${EXE_NAME}
  PRIVATE
    IO_platform::IO_platform
    ImGui
    glad
    SDL3::SDL3
    OpenGL::GL
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)

  target_link_libraries(${EXE_NAME}
    PRIVATE
      X11::X11
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

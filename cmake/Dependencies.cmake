include(FetchContent)

set(FETCHCONTENT_QUIET OFF)


option(USE_VENDORED_DEPS "Use bundled dependency sources" OFF)

# SDL3
if(USE_VENDORED_DEPS AND EXISTS "${CMAKE_SOURCE_DIR}/vendor/SDL3/CMakeLists.txt")
  add_subdirectory("${CMAKE_SOURCE_DIR}/vendor/SDL3" "${CMAKE_BINARY_DIR}/vendor/SDL3-build")
else()
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        683181b47cfabd293e3ea409f838915b8297a4fd
    GIT_SHALLOW    FALSE
  )

  FetchContent_MakeAvailable(SDL3)
endif()

# ImGui
if(USE_VENDORED_DEPS AND EXISTS "${CMAKE_SOURCE_DIR}/vendor/imgui/imgui.cpp")
  set(imgui_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/imgui")
else()
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        dac07199cfd761113d966eb8ad739254e10df2fe
    GIT_SHALLOW    FALSE
  )

  FetchContent_GetProperties(imgui)

  if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
  endif()
endif()

# GLAD is generated source, not fetched generator repo source.
add_library(glad STATIC
  "${CMAKE_SOURCE_DIR}/dependencies/glad1/src/glad.c"
)

target_include_directories(glad
  PUBLIC
    "${CMAKE_SOURCE_DIR}/dependencies/glad1/include"
)

# ImGui wrapper target
add_library(ImGui STATIC)

target_sources(ImGui
  PRIVATE
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
)

target_include_directories(ImGui
  PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)

target_link_libraries(ImGui
  PUBLIC
    SDL3::SDL3
)

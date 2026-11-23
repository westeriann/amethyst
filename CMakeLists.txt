cmake_minimum_required(VERSION 3.20)
project(Amethyst CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# libprojectM 4.2 (prebuilt, see third_party/projectm/BUILD-INFO.txt)
add_library(projectM4 SHARED IMPORTED)
set_target_properties(projectM4 PROPERTIES
  IMPORTED_LOCATION ${TP}/projectm/bin/libprojectM-4.dll
  IMPORTED_IMPLIB ${TP}/projectm/lib/libprojectM-4.dll.a
  INTERFACE_INCLUDE_DIRECTORIES ${TP}/projectm/include)

# Spout2 (BSD-2) — GPU texture sharing with OBS, Resolume, TouchDesigner...
file(GLOB SPOUT_SRC ${TP}/spout/*.cpp)
add_library(spout STATIC ${SPOUT_SRC})
target_include_directories(spout PUBLIC ${TP}/spout)
target_compile_options(spout PRIVATE -mssse3 -msse4.1 -w)
target_link_libraries(spout PUBLIC opengl32 d3d11 dxgi gdi32 version winmm shlwapi psapi comctl32 ole32)

set(IMGUI ${TP}/imgui)
add_executable(Amethyst WIN32
  src/main.cpp src/audio.cpp src/thumbs.cpp src/platform_win.cpp src/gfx.cpp src/midi.cpp
  src/net.cpp src/tempo.cpp src/media.cpp src/image.cpp src/stagelinq.cpp src/player.cpp src/preset_editor.cpp src/structure.cpp
  ${TP}/stb/wfETC.c ${TP}/stb/image_DXT.c
  ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
  ${IMGUI}/backends/imgui_impl_sdl2.cpp ${IMGUI}/backends/imgui_impl_opengl3.cpp
  src/app.rc)
target_include_directories(Amethyst PRIVATE ${IMGUI} src ${TP}/stb ${TP}/link/include ${TP}/asio/include)
target_compile_definitions(Amethyst PRIVATE NOMINMAX LINK_PLATFORM_WINDOWS=1 _WIN32_WINNT=0x0A00)
# Ableton Link + asio are noisy under GCC; keep our own warnings readable.
set_source_files_properties(src/tempo.cpp PROPERTIES COMPILE_OPTIONS "-w")
set_source_files_properties(src/platform_win.cpp src/media.cpp src/midi.cpp PROPERTIES COMPILE_DEFINITIONS "UNICODE;_UNICODE")
target_link_libraries(Amethyst PRIVATE SDL2::SDL2main SDL2::SDL2 GLEW::GLEW OpenGL::GL projectM4 spout
  ole32 shell32 uuid shlwapi winmm ws2_32 iphlpapi mf mfplat mfreadwrite mfuuid propsys)
target_link_options(Amethyst PRIVATE -static-libgcc -static-libstdc++)

# Offline test bench for tempo + song-structure detection (not shipped)
add_executable(amethyst-analyze tools/analyze.cpp src/tempo.cpp src/structure.cpp)
target_include_directories(amethyst-analyze PRIVATE src ${TP}/stb ${TP}/link/include ${TP}/asio/include)
target_compile_definitions(amethyst-analyze PRIVATE NOMINMAX LINK_PLATFORM_WINDOWS=1 _WIN32_WINNT=0x0A00)
target_link_libraries(amethyst-analyze PRIVATE ws2_32 iphlpapi winmm)
target_link_options(amethyst-analyze PRIVATE -static)

cmake_minimum_required(VERSION 3.16)
project(vr32x_track_editor CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
          third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_impl_opengl3.cpp)
set(CORE src/rom.cpp src/objio.cpp)

add_executable(vrtool tools/vrtool.cpp ${CORE})

if(WIN32)
  add_executable(vr32x-track-editor WIN32 src/platform_win32.cpp src/app.cpp src/renderer.cpp src/gl.cpp ${CORE}
                 ${IMGUI} third_party/imgui/imgui_impl_win32.cpp res/app.rc)
  target_compile_definitions(vr32x-track-editor PRIVATE UNICODE _UNICODE)
  target_link_libraries(vr32x-track-editor opengl32 gdi32 user32 comdlg32 shell32 dwmapi)
  if(MINGW)
    target_link_options(vr32x-track-editor PRIVATE -municode -static)
    target_link_options(vrtool PRIVATE -static)
  endif()
else()
  find_package(X11 REQUIRED)
  add_executable(vr32x-track-editor src/platform_x11.cpp src/app.cpp src/renderer.cpp src/gl.cpp ${CORE} ${IMGUI})
  target_link_libraries(vr32x-track-editor ${X11_LIBRARIES} GL dl)
endif()
target_include_directories(vr32x-track-editor PRIVATE third_party third_party/imgui)

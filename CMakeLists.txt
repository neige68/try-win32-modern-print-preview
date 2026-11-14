cmake_minimum_required(VERSION 3.20)
project(modern_print_preview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(modern_print_preview WIN32
  src/main.cpp
  src/print_source.cpp
  src/gdi_content.cpp)

# ソースは BOM なし UTF-8 → /utf-8 を指定 (C4819 回避)。C++/WinRT は SDK 同梱ヘッダー (cppwinrt/) を使う。
target_compile_options(modern_print_preview PRIVATE /utf-8 /W4 /EHsc /permissive-)
target_compile_definitions(modern_print_preview PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(modern_print_preview PRIVATE windowsapp d3d11 dxgi)

# 画像アセットを exe の隣へ
add_custom_command(TARGET modern_print_preview POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/sample.bmp $<TARGET_FILE_DIR:modern_print_preview>/sample.bmp)

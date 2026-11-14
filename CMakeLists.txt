cmake_minimum_required(VERSION 3.20)
project(modern_print_preview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# OWLNext の静的ライブラリ (-t) は /MT (RuntimeLibrary=MT_StaticRelease) でビルドされているため、全体を /MT にそろえる
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 印刷処理 (両バージョン共通。OWL にも素の Win32 にも依存しない)
add_library(print_common OBJECT
  src/print_ui.cpp
  src/print_source.cpp
  src/gdi_content.cpp)
# ソースは BOM なし UTF-8 → /utf-8 を指定 (C4819 回避)。C++/WinRT は SDK 同梱ヘッダー (cppwinrt/) を使う。
target_compile_options(print_common PRIVATE /utf-8 /W4 /EHsc /permissive-)
target_compile_definitions(print_common PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(print_common PUBLIC windowsapp d3d11 dxgi)

# ---- 素の Win32 版
add_executable(modern_print_preview WIN32 src/main.cpp)
target_compile_options(modern_print_preview PRIVATE /utf-8 /W4 /EHsc /permissive-)
target_link_libraries(modern_print_preview PRIVATE print_common)

# ---- OWLNext 7 版 (OWLROOT 環境変数が必要。OWL ライブラリは静的 /MT ビルドなので同じ CRT にそろえる)
if(DEFINED ENV{OWLROOT})
  file(TO_CMAKE_PATH "$ENV{OWLROOT}" OWLROOT)
  add_executable(modern_print_preview_owl WIN32 src/owl_main.cpp)
  target_compile_options(modern_print_preview_owl PRIVATE /utf-8 /W4 /EHsc /permissive-)
  target_include_directories(modern_print_preview_owl PRIVATE ${OWLROOT}/include)
  target_link_directories(modern_print_preview_owl PRIVATE ${OWLROOT}/lib)
  target_link_libraries(modern_print_preview_owl PRIVATE print_common)
  # OWL の _tmain をエントリにする (GUI サブシステム + wmainCRTStartup)
  target_link_options(modern_print_preview_owl PRIVATE /ENTRY:wmainCRTStartup)
  add_custom_command(TARGET modern_print_preview_owl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/sample.bmp $<TARGET_FILE_DIR:modern_print_preview_owl>/sample.bmp)
endif()

# 画像アセットを exe の隣へ
add_custom_command(TARGET modern_print_preview POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/sample.bmp $<TARGET_FILE_DIR:modern_print_preview>/sample.bmp)

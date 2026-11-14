// 素の Win32 版: ウィンドウと「印刷」ボタン。印刷 UI の処理は print_ui.cpp。
#include <windows.h>

#include "print_ui.h"

namespace {

constexpr int kIdPrint = 1;
constexpr UINT_PTR kTimerAuto = 1;

bool g_autoClick = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE: {
      HWND button = ::CreateWindowExW(0, L"BUTTON", L"印刷", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 20, 120, 40,
                                      hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPrint)), nullptr, nullptr);
      ::SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), TRUE);
      if (g_autoClick) ::SetTimer(hwnd, kTimerAuto, 1000, nullptr);
      return 0;
    }
    case WM_TIMER:
      if (wp == kTimerAuto) {
        ::KillTimer(hwnd, kTimerAuto);
        ShowPrintUi(hwnd);
      }
      return 0;
    case WM_COMMAND:
      if (LOWORD(wp) == kIdPrint) ShowPrintUi(hwnd);
      return 0;
    case WM_DESTROY:
      ShutdownPrintUi();
      ::PostQuitMessage(0);
      return 0;
  }
  return ::DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
  InitWinRt();
  g_autoClick = cmd && wcsstr(cmd, L"--auto");

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = L"ModernPrintPreviewTest";
  ::RegisterClassW(&wc);
  HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Modern Print Preview Test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                200, 200, 480, 200, nullptr, nullptr, inst, nullptr);
  ::ShowWindow(hwnd, show);

  MSG m;
  while (::GetMessageW(&m, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&m);
    ::DispatchMessageW(&m);
  }
  return 0;
}

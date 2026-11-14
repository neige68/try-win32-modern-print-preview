// Win32 + C++/WinRT: GDI 描画を Windows 11 の新しい印刷 UI のプレビューに出す検証アプリ。
#include <windows.h>
#include <unknwn.h>

#include <PrintManagerInterop.h>  // IPrintManagerInterop (SDK um/)

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Printing.h>

#include "print_source.h"

namespace wgp = winrt::Windows::Graphics::Printing;
namespace wf = winrt::Windows::Foundation;

namespace {

constexpr int kIdPrint = 1;
constexpr UINT_PTR kTimerAuto = 1;

HWND g_button = nullptr;
bool g_autoClick = false;
wgp::PrintManager g_printManager{nullptr};
winrt::event_token g_token;
wf::IAsyncOperation<bool> g_op{nullptr};

void ShowPrintUi(HWND hwnd) {
  try {
    if (!wgp::PrintManager::IsSupported()) {
      ::MessageBoxW(hwnd, L"PrintManager is not supported.", L"Error", MB_ICONERROR);
      return;
    }
    // GetForWindow は IPrintManagerInterop 経由 (Win32 から HWND を指定するための相互運用 API)
    auto interop = winrt::get_activation_factory<wgp::PrintManager, IPrintManagerInterop>();
    if (!g_printManager) {
      winrt::check_hresult(
          interop->GetForWindow(hwnd, winrt::guid_of<wgp::PrintManager>(), winrt::put_abi(g_printManager)));
      g_token = g_printManager.PrintTaskRequested([](wgp::PrintManager const&, wgp::PrintTaskRequestedEventArgs const& args) {
        Log(L"PrintTaskRequested");
        auto source = CreatePrintSource();
        args.Request().CreatePrintTask(L"GDI Preview Test", [source](wgp::PrintTaskSourceRequestedArgs const& a) {
          Log(L"PrintTaskSourceRequested -> SetSource");
          a.SetSource(source);
        });
      });
    }
    Log(L"ShowPrintUIForWindowAsync");
    winrt::check_hresult(interop->ShowPrintUIForWindowAsync(hwnd, winrt::guid_of<wf::IAsyncOperation<bool>>(),
                                                            winrt::put_abi(g_op)));
    g_op.Completed([](wf::IAsyncOperation<bool> const& op, wf::AsyncStatus s) {
      Log(L"ShowPrintUI completed status=" + std::to_wstring(static_cast<int>(s)));
      (void)op;
    });
  } catch (winrt::hresult_error const& e) {
    Log(L"ShowPrintUi failed: " + std::wstring(e.message()));
    ::MessageBoxW(hwnd, e.message().c_str(), L"Print UI error", MB_ICONERROR);
  }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      g_button = ::CreateWindowExW(0, L"BUTTON", L"印刷", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 20, 120, 40, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPrint)), nullptr, nullptr);
      ::SendMessageW(g_button, WM_SETFONT, reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), TRUE);
      if (g_autoClick) ::SetTimer(hwnd, kTimerAuto, 1000, nullptr);
      return 0;
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
      if (g_printManager) g_printManager.PrintTaskRequested(g_token);
      g_printManager = nullptr;
      ::PostQuitMessage(0);
      return 0;
  }
  return ::DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
  winrt::init_apartment(winrt::apartment_type::single_threaded);
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

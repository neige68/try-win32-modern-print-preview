#include "print_ui.h"

#include <unknwn.h>  // winrt ヘッダーより前に必要

#include <PrintManagerInterop.h>  // IPrintManagerInterop (SDK um/)

#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Printing.h>

#include "print_source.h"

namespace wgp = winrt::Windows::Graphics::Printing;
namespace wf = winrt::Windows::Foundation;

namespace {

wgp::PrintManager g_printManager{nullptr};
winrt::event_token g_token;
wf::IAsyncOperation<bool> g_op{nullptr};

}  // namespace

void InitWinRt() { winrt::init_apartment(winrt::apartment_type::single_threaded); }

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

void ShutdownPrintUi() {
  if (g_printManager) g_printManager.PrintTaskRequested(g_token);
  g_printManager = nullptr;
}

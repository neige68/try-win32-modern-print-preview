// OWLNext 7 版: TApplication / TFrameWindow / TButton で同じ検証を行う。
// 印刷 UI の処理 (print_ui.cpp) は winrt をここへ持ち込まないため、OWL と C++/WinRT のヘッダーは混ざらない。
#include <owl/pch.h>

#include <owl/applicat.h>
#include <owl/button.h>
#include <owl/framewin.h>

#include "print_ui.h"

namespace {

constexpr int kIdPrint = 101;
constexpr UINT_PTR kTimerAuto = 1;

}  // namespace

class TPrintTestFrame : public owl::TFrameWindow {
 public:
  TPrintTestFrame(bool autoClick)
      : owl::TFrameWindow(nullptr, _T("Modern Print Preview Test (OWLNext 7)")), m_autoClick(autoClick) {
    Attr.X = 200;
    Attr.Y = 200;
    Attr.W = 480;
    Attr.H = 200;
    new owl::TButton(this, kIdPrint, _T("印刷"), 20, 20, 120, 40);
  }

 protected:
  void SetupWindow() override {
    owl::TFrameWindow::SetupWindow();
    if (m_autoClick) SetTimer(kTimerAuto, 1000);
  }
  void CleanupWindow() override {
    ShutdownPrintUi();
    owl::TFrameWindow::CleanupWindow();
  }

  void CmPrint() { ShowPrintUi(GetHandle()); }  // TWindow::GetHandle() で HWND を渡すだけ
  void EvTimer(owl::uint timerId) {
    if (timerId != kTimerAuto) return;
    KillTimer(kTimerAuto);
    CmPrint();
  }

  DECLARE_RESPONSE_TABLE(TPrintTestFrame);

 private:
  bool m_autoClick;
};

DEFINE_RESPONSE_TABLE1(TPrintTestFrame, owl::TFrameWindow)
  EV_COMMAND(kIdPrint, CmPrint),
  EV_WM_TIMER,
END_RESPONSE_TABLE;

class TPrintTestApp : public owl::TApplication {
 public:
  TPrintTestApp() : owl::TApplication(_T("ModernPrintPreviewOwl")) {}

 protected:
  void InitMainWindow() override {
    const bool autoClick = ::wcsstr(::GetCommandLineW(), L"--auto") != nullptr;
    SetMainWindow(std::make_unique<TPrintTestFrame>(autoClick));
  }
};

int OwlMain(int, _TCHAR*[]) {
  InitWinRt();  // OWL コアは COM を初期化しない (OCF 使用時のみ OleInitialize) ので、ここで STA を初期化する
  return TPrintTestApp().Run();
}

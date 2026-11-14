// 新しい印刷 UI の表示 (HWND だけに依存する。素の Win32 / OWLNext のどちらからも呼べる)。
// C++/WinRT のヘッダーを公開しないので、呼び出し側は winrt を include しなくてよい。
#pragma once
#include <windows.h>

// hwnd のウィンドウをオーナーとして印刷 UI を表示する。初回呼び出し時に PrintManager を取得する。
// 呼び出しスレッドは init_apartment (STA) 済みであること (InitWinRt 参照)。
void ShowPrintUi(HWND hwnd);

// WinRT (STA) の初期化。プロセスで 1 回、メッセージループを回すスレッドで呼ぶ。
void InitWinRt();

// ウィンドウ破棄時に呼ぶ。PrintTaskRequested の登録解除と PrintManager の解放。
void ShutdownPrintUi();

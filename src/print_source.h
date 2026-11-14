#pragma once
#include <windows.h>
#include <unknwn.h>  // C++/WinRT で classic COM を実装するため winrt ヘッダーより前に必要

#include <string>

#include <winrt/Windows.Graphics.Printing.h>

// XAML を使わず、IPrintDocumentPageSource / IPrintPreviewPageCollection を自前実装した印刷ソースを作る。
// 戻り値を PrintTaskSourceRequestedArgs::SetSource() に渡す。
winrt::Windows::Graphics::Printing::IPrintDocumentSource CreatePrintSource();

// 診断ログ (%TEMP%\modern_print_preview.log)
void Log(const std::wstring& msg);

// プレビュー描画の解像度 (dpi)。環境変数 PREVIEW_DPI で上書き可能。既定 192。
int PreviewRenderDpi();

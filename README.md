# try-win32-modern-print-preview

Win32 (C++/WinRT) アプリで、GDI 描画を Windows 11 の新しい印刷 UI のプレビューに表示できるかを検証する。

## 構成
- `src/main.cpp` : ウィンドウと「印刷」ボタン。`IPrintManagerInterop::GetForWindow` / `ShowPrintUIForWindowAsync`
- `src/print_source.cpp` : XAML 非依存の印刷ソース (`IPrintDocumentSource` + `IPrintDocumentPageSource` + `IPrintPreviewPageCollection`)
- `src/gdi_content.cpp` : GDI 描画 `DrawPageContent(HDC, RECT, pageNo, dpi)` (テキスト・直線・ビットマップ)
- `assets/sample.bmp` : 読み込む画像 (exe の隣へコピーされる)

## ビルド
VS2022 + Windows SDK 10.0.26100 (C++/WinRT ヘッダーは SDK 同梱)。

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

## 実行・診断
- `modern_print_preview.exe --auto` : 起動 1 秒後に印刷 UI を自動表示
- ログ: `%TEMP%\modern_print_preview.log` (Paginate / MakePage の呼び出しと寸法)
- 環境変数 `PREVIEW_DPI` (既定 192) : プレビューの描画解像度
- 環境変数 `DUMP_PREVIEW=1` : 描画した BGRA ビットマップを `%TEMP%\preview_page*.bmp` に保存

## 制限
- 実印刷 (`MakeDocument`) は未実装 (`E_NOTIMPL`)。検証対象外。

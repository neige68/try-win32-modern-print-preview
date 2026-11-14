# try-win32-modern-print-preview

Win32 (C++/WinRT) アプリで、GDI 描画を Windows 11 の新しい印刷 UI のプレビューに表示できるかを検証する。

## 構成
- `src/main.cpp` : 素の Win32 版。ウィンドウと「印刷」ボタン (`modern_print_preview.exe`)
- `src/owl_main.cpp` : OWLNext 7 版。`TApplication` / `TFrameWindow` / `TButton` (`modern_print_preview_owl.exe`)
- `src/print_ui.cpp` : `ShowPrintUi(HWND)`。`IPrintManagerInterop::GetForWindow` / `ShowPrintUIForWindowAsync`。winrt を公開しない
- `src/print_source.cpp` : XAML 非依存の印刷ソース (`IPrintDocumentSource` + `IPrintDocumentPageSource` + `IPrintPreviewPageCollection`)
- `src/gdi_content.cpp` : GDI 描画 `DrawPageContent(HDC, RECT, pageNo, dpi)` (テキスト・直線・ビットマップ)
- `assets/sample.bmp` : 読み込む画像 (exe の隣へコピーされる)

## ビルド
VS2022 + Windows SDK 10.0.26100 (C++/WinRT ヘッダーは SDK 同梱)。
OWL 版は環境変数 `OWLROOT` (例: `U:\lib\owl\owlnext-7.0.20-patched`) があるときだけビルドされる。
OWL の静的ライブラリ (`owl-7.0-v1930-x64-tu.lib`) が `/MT` なので、両版とも `/MT` (Release のみ検証)。

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

## 実行・診断
- `modern_print_preview.exe --auto` : 起動 1 秒後に印刷 UI を自動表示
- ログ: `%TEMP%\modern_print_preview.log` (Paginate / MakePage の呼び出しと寸法)
- 環境変数 `PREVIEW_DPI` (既定 192) : プレビューの描画解像度
- 環境変数 `DUMP_PREVIEW=1` : 描画した BGRA ビットマップを `%TEMP%\preview_page*.bmp` に保存

## 制限
- 実印刷 (`MakeDocument`) は未実装 (`E_NOTIMPL`)。検証対象外。

## OWLNext 7 との共存 (ブランチ owlnext7 の検証結果)
- 共存できた。OWL 版で印刷 UI・プレビュー・ページ送り・キャンセル・終了 (exit code 0) を確認 (Microsoft Print to PDF)
- 渡すものは `TWindow::GetHandle()` の `HWND` だけ。OWL コアは COM を初期化しないので、`OwlMain` 先頭で `InitWinRt()` (STA)
- OWL と C++/WinRT のヘッダーは翻訳単位を分けて混在させない (`print_ui.h` は windows.h のみ)
- OWL 側の `_tmain` を使うため `/ENTRY:wmainCRTStartup` + `WIN32` (GUI サブシステム)
- 警告 C4005 `WIN32_LEAN_AND_MEAN` 再定義 (OWL の `wsysinc.h` が定義) は無害
- 未確認: Debug ビルド (`-dtu` ライブラリ + `/MTd`)、OCF/OLE 併用時の STA 整合、x86

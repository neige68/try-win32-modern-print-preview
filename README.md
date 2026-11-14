# try-win32-modern-print-preview

Win32 (C++/WinRT) アプリで、GDI 描画を Windows 11 の新しい印刷 UI (プレビュー付き) のプレビューに表示できるかを検証する。
実プリンターへの出力は対象外。

## 結論
- **できた。** XAML を使わず、`IPrintDocumentPageSource` / `IPrintPreviewPageCollection` を自前実装して GDI 描画を渡せる
- 素の Win32 版と OWLNext 7 版の両方で動作 (Microsoft Print to PDF で確認)
- 用紙サイズ (A3→A4) と向き (横→縦) の変更はプレビューに追随する (変更のたびに `Paginate` が再呼び出しされる)

## 構成
- `src/main.cpp` : 素の Win32 版。ウィンドウと「印刷」ボタン (`modern_print_preview.exe`)
- `src/owl_main.cpp` : OWLNext 7 版。`TApplication` / `TFrameWindow` / `TButton` (`modern_print_preview_owl.exe`)
- `src/print_ui.cpp` : `ShowPrintUi(HWND)`。`IPrintManagerInterop::GetForWindow` / `ShowPrintUIForWindowAsync`。winrt を公開しない
- `src/print_source.cpp` : XAML 非依存の印刷ソース (`IPrintDocumentSource` + `IPrintDocumentPageSource` + `IPrintPreviewPageCollection`)
- `src/gdi_content.cpp` : GDI 描画 `DrawPageContent(HDC, RECT, pageNo, dpi)` (テキスト・直線・ビットマップ)
- `assets/sample.bmp` : 読み込む画像 (exe の隣へコピーされる)

## 方式
1. `Paginate` : `IPrintTaskOptionsCore::GetPageDescription` で実用紙サイズ (DIP) を取得し、`SetJobPageCount(2)`
2. `MakePage` : 実用紙サイズ × `PREVIEW_DPI` の DIB セクション (メモリ DC) に GDI で描く
3. alpha を 255 に補正して D3D11 テクスチャ (BGRA) にコピーし、`IPrintPreviewDxgiPackageTarget::DrawPage` に渡す

検討して見送った方式: XAML Islands + `Windows.UI.Xaml.Printing.PrintDocument` (KiCad が採用。純 Win32 の小アプリには依存が重い。比較実装はしていない)。

## ビルド
VS2022 + Windows SDK 10.0.26100 (C++/WinRT ヘッダーは SDK 同梱)。

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release   # または --config Debug

- OWL 版は環境変数 `OWLROOT` (例: `U:\lib\owl\owlnext-7.0.20-patched`) があるときだけビルドされる
- OWL の静的ライブラリ (`owl-7.0-v1930-x64-tu.lib`) が `/MT` なので、両版とも `/MT` (Debug は `/MTd` + `owl-7.0-v1930-x64-dtu.lib`。OWL の `__DEBUG` は `_DEBUG` から自動で 1 になるので追加設定は不要)。Release / Debug とも警告 0 を確認

## 実行・診断
- `--auto` : 起動 1 秒後に印刷 UI を自動表示
- ログ: `%TEMP%\modern_print_preview.log` (Paginate / MakePage の呼び出しと寸法)
- 環境変数 `PREVIEW_DPI` (既定 192) : プレビューの描画解像度
- 環境変数 `DUMP_PREVIEW=1` : 描画した BGRA ビットマップを `%TEMP%\preview_page*.bmp` に保存

## 画質で気づいた点
- プレビュー枠は約 500 DIP 幅で、システムが縮小表示する。5〜7pt の小さい文字は読みにくい (日本語の字形は崩れない)
- 1px の細線は縮小で薄いグレーになる。破線・点線は一部潰れる
- ビットマップは崩れないが、市松模様はモアレ気味
- ズーム時に再描画されるかは未確認

## OWLNext 7 との共存
- 渡すものは `TWindow::GetHandle()` の `HWND` だけ。OWL コアは COM を初期化しないので、`OwlMain` 先頭で `InitWinRt()` (STA)
- OWL と C++/WinRT のヘッダーは翻訳単位を分けて混在させない (`print_ui.h` は windows.h のみ)
- OWL 側の `_tmain` を使うため `/ENTRY:wmainCRTStartup` + `WIN32` (GUI サブシステム)
- `WIN32_LEAN_AND_MEAN` は OWL (`wsysinc.h`) が値なしで定義する → こちらも空定義 (`WIN32_LEAN_AND_MEAN=`) にして C4005 を回避。警告 0

## 制限・未確認
- 実印刷 (`MakeDocument`) は未実装 (`E_NOTIMPL`)。「印刷」ボタンは動作しない
- 未確認: ズーム時の再描画、Microsoft Print to PDF 以外のプリンター (余白 = `ImageableRect`)、x86、OCF/OLE 併用時の STA 整合
- 本格導入の懸念: 実印刷経路 (`ID2D1PrintControl` 等) とプレビューの一致、コールバックが任意スレッドから来ること、大判用紙 × 高 DPI のメモリ (A3 @192dpi で約 28MB/ページ)、C++ の公式サンプルが無く挙動を実機で確かめている点

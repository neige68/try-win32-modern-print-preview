# CLAUDE.md

GDI 描画を Windows 11 の新しい印刷 UI のプレビューに出す検証プロジェクト (概要・結論は README.md)。
既存の製品コードは参照も変更もしない独立プロジェクト。

## ビルド・実行 (WSL2 から)
- CMake + VS2022 (x64 Release)。Windows SDK は `N:\Windows Kits\10` の 10.0.26100 (`-DCMAKE_SYSTEM_VERSION=10.0.26100.0` を付ける)
- ビルド用スクリプトはリポジトリに置かず `/mnt/tmp/try-win32-modern-print-preview/` (= `C:\Temp\...`) に置く。ソースの Windows パスは `C:\Users\neige\git\try-win32-modern-print-preview`
- exe の実行中は上書きできない (LNK1104)。再ビルド前に `Get-Process modern_print_preview* | Stop-Process -Force`
- OWL 版は環境変数 `OWLROOT` が必要。OWL のライブラリは静的 `/MT` のため全ターゲットを `/MT` にそろえている (Debug 未検証)
- git 操作は Windows の git.exe (pwsh 経由)。コミットメッセージは `-F` でファイルから渡す

## 動作確認 (GUI)
- `--auto` で起動 1 秒後に印刷 UI を自動表示。ログは `%TEMP%\modern_print_preview.log`
- 画面は PowerShell の `CopyFromScreen` でキャプチャして Read で見る。印刷ダイアログの操作は UI Automation
  (名前: `印刷の向き` / `縦` / `その他の設定` / `用紙サイズ` / `次のページ` / `キャンセル`)
- 印刷ダイアログの「ズーム」ボタンは UIA から操作できなかった (未確認事項)
- OWL の `TButton` は UIA で名前が空・Pane に見える → `ClassName == 'Button'` の矩形中央をマウスクリックする

## 実装上の落とし穴 (調査済み)
- `IPrintPreviewPageCollection::MakePage` の `width` / `height` は**縮小表示サイズ**。実用紙サイズは `Paginate` の
  `GetPageDescription` で取り、サーフェスは「実用紙サイズ × dpi」で作る (取り違えると内容が約 3 倍に拡大されてはみ出す)
- `JOB_PAGE_APPLICATION_DEFINED` (0xFFFFFFFF) が `Paginate` / `MakePage` に来る → 1 ページ目として扱う
- GDI は alpha を書かない → 0xFF000000 を OR してからテクスチャへ
- `winrt::implements` を継承したクラス内の素の `IInspectable` は winrt 側に解決される → COM のシグネチャは `::IInspectable*`
- classic COM を `implements` する前に `<unknwn.h>` を winrt ヘッダーより前に include
- コールバック (`Paginate` / `MakePage` 等) は任意のスレッドから呼ばれる → `std::mutex` で直列化
- OWL と C++/WinRT のヘッダーは同じ翻訳単位に入れない (`print_ui.h` は windows.h のみ)
- OWL は `_tmain` を提供する → `WIN32` + `/ENTRY:wmainCRTStartup`

## ソース規約
- Write / Edit で作る `.cpp` / `.h` は BOM なし UTF-8・LF。MSVC には `/utf-8` を渡して C4819 を回避している (BOM 変換はしない)
- フォント名は GDI の `lfFaceName` に英語名を使う (`MS 明朝` ではなく `MS Mincho`。日本語名だと別フォントに落ちた)

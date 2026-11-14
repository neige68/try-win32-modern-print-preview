#include "gdi_content.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

namespace {

int PtToPx(double pt, int dpi) { return static_cast<int>(std::lround(pt * dpi / 72.0)); }
int MmToPx(double mm, int dpi) { return static_cast<int>(std::lround(mm * dpi / 25.4)); }

std::wstring ExeDir() {
  wchar_t buf[MAX_PATH]{};
  ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring s = buf;
  return s.substr(0, s.find_last_of(L'\\') + 1);
}

// ファイルから読み込んだ画像 (BMP)。初回のみロードして使い回す。
HBITMAP GetSampleBitmap() {
  static std::once_flag once;
  static HBITMAP bmp = nullptr;
  std::call_once(once, [] {
    bmp = static_cast<HBITMAP>(::LoadImageW(nullptr, (ExeDir() + L"sample.bmp").c_str(), IMAGE_BITMAP, 0, 0,
                                            LR_LOADFROMFILE | LR_CREATEDIBSECTION));
  });
  return bmp;
}

void Text(HDC dc, int x, int y, const wchar_t* face, double pt, int dpi, const std::wstring& s, bool italic = false,
          int weight = FW_NORMAL, int angle10 = 0) {
  LOGFONTW lf{};
  lf.lfHeight = -PtToPx(pt, dpi);
  lf.lfWeight = weight;
  lf.lfItalic = italic;
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfEscapement = lf.lfOrientation = angle10;
  lf.lfQuality = ANTIALIASED_QUALITY;
  wcscpy_s(lf.lfFaceName, face);
  HFONT f = ::CreateFontIndirectW(&lf);
  HGDIOBJ old = ::SelectObject(dc, f);
  ::TextOutW(dc, x, y, s.c_str(), static_cast<int>(s.size()));
  ::SelectObject(dc, old);
  ::DeleteObject(f);
}

void Line(HDC dc, HPEN pen, int x1, int y1, int x2, int y2) {
  HGDIOBJ old = ::SelectObject(dc, pen);
  ::MoveToEx(dc, x1, y1, nullptr);
  ::LineTo(dc, x2, y2);
  ::SelectObject(dc, old);
  ::DeleteObject(pen);
}

HPEN GeoPen(int widthPx, DWORD style = PS_SOLID, COLORREF c = RGB(0, 0, 0), const DWORD* dash = nullptr,
            DWORD dashCount = 0) {
  LOGBRUSH lb{BS_SOLID, c, 0};
  return ::ExtCreatePen(PS_GEOMETRIC | PS_ENDCAP_FLAT | style, widthPx < 1 ? 1 : widthPx, &lb, dashCount, dash);
}

void DrawPage1(HDC dc, const RECT& a, int dpi) {
  const int L = a.left, W = a.right - a.left;
  int y = a.top;

  ::SetBkMode(dc, TRANSPARENT);
  ::SetTextColor(dc, RGB(0, 0, 0));

  // ---- テキスト (日本語を含む。フォントとサイズ数種類) ----
  Text(dc, L, y, L"Yu Gothic UI", 24, dpi, L"GDI → 印刷プレビュー検証 Page 1", false, FW_BOLD);
  y += PtToPx(24, dpi) * 3 / 2;
  struct Row {
    const wchar_t* face;
    double pt;
    bool italic;
    const wchar_t* text;
  } rows[] = {
      {L"MS Mincho", 14, false, L"MS 明朝 14pt: 吾輩は猫である。名前はまだ無い。"},
      {L"MS Gothic", 12, false, L"MS ゴシック 12pt: 日本語と ABC abc 0123 の混在"},
      {L"Meiryo", 10.5, false, L"メイリオ 10.5pt: 漢字・ひらがな・カタカナ・ｈａｌｆ－ｗｉｄｔｈ ｶﾀｶﾅ"},
      {L"Times New Roman", 12, true, L"Times New Roman Italic 12pt: The quick brown fox jumps over the lazy dog."},
      {L"Consolas", 9, false, L"Consolas 9pt: int main() { return 0; }  // 等幅"},
      {L"Yu Mincho", 7, false, L"游明朝 7pt (小さい文字): 髙﨑 ①②③ 〒100-0001 東京都千代田区千代田1-1"},
      {L"Arial", 5, false, L"Arial 5pt (極小): The five boxing wizards jump quickly."},
  };
  for (const auto& r : rows) {
    Text(dc, L, y, r.face, r.pt, dpi, r.text, r.italic);
    y += PtToPx(r.pt, dpi) * 3 / 2 + MmToPx(1, dpi);
  }

  // ---- 直線 (太さと線種) ----
  y += MmToPx(4, dpi);
  Text(dc, L, y, L"Yu Gothic UI", 11, dpi, L"直線 (太さ: 1px / 0.1mm / 0.25mm / 0.5mm / 1mm / 2mm)");
  y += PtToPx(11, dpi) * 2;
  const int mm[] = {0, 10, 25, 50, 100, 200};  // 0.01mm 単位 (0 は 1px)
  for (int v : mm) {
    int w = v == 0 ? 1 : static_cast<int>(std::lround(v / 100.0 * dpi / 25.4));
    Line(dc, GeoPen(w), L, y, L + W, y);
    y += MmToPx(3, dpi);
  }
  y += MmToPx(2, dpi);
  Text(dc, L, y, L"Yu Gothic UI", 11, dpi, L"線種 (実線 / 破線 / 点線 / 一点鎖線 / 太い破線)");
  y += PtToPx(11, dpi) * 2;
  const int styles[] = {PS_SOLID, PS_DASH, PS_DOT, PS_DASHDOT};
  for (int s : styles) {
    Line(dc, ::CreatePen(s, 1, RGB(0, 0, 0)), L, y, L + W, y);
    y += MmToPx(3, dpi);
  }
  const DWORD dash[] = {static_cast<DWORD>(MmToPx(4, dpi)), static_cast<DWORD>(MmToPx(2, dpi))};
  Line(dc, GeoPen(MmToPx(0.8, dpi), PS_USERSTYLE, RGB(200, 0, 0), dash, 2), L, y, L + W, y);
  y += MmToPx(4, dpi);
  // 斜線 (ジャギー確認)
  Line(dc, GeoPen(1), L, y, L + W, y + MmToPx(25, dpi));
  Line(dc, GeoPen(MmToPx(0.3, dpi), PS_SOLID, RGB(0, 0, 200)), L, y + MmToPx(25, dpi), L + W, y);
}

void DrawPage2(HDC dc, const RECT& a, int dpi) {
  const int L = a.left, W = a.right - a.left, H = a.bottom - a.top;
  int y = a.top;
  ::SetBkMode(dc, TRANSPARENT);
  Text(dc, L, y, L"Yu Gothic UI", 24, dpi, L"Page 2: ビットマップ", false, FW_BOLD);
  y += PtToPx(24, dpi) * 3 / 2;

  HBITMAP bmp = GetSampleBitmap();
  if (!bmp) {
    Text(dc, L, y, L"Yu Gothic UI", 12, dpi, L"sample.bmp を読み込めませんでした");
    return;
  }
  BITMAP bi{};
  ::GetObjectW(bmp, sizeof(bi), &bi);
  HDC mem = ::CreateCompatibleDC(dc);
  HGDIOBJ old = ::SelectObject(mem, bmp);
  ::SetStretchBltMode(dc, HALFTONE);
  ::SetBrushOrgEx(dc, 0, 0, nullptr);

  // 等倍 (画像 1px = 用紙 1/96 インチ相当) → 実寸に近い大きさ
  Text(dc, L, y, L"Meiryo", 10, dpi, L"① 96dpi 相当の実寸 (320x240 px)");
  y += PtToPx(10, dpi) * 2;
  const int w1 = bi.bmWidth * dpi / 96, h1 = bi.bmHeight * dpi / 96;
  ::StretchBlt(dc, L, y, w1, h1, mem, 0, 0, bi.bmWidth, bi.bmHeight, SRCCOPY);
  // 隣に 3 倍拡大
  const int w3 = std::min(W - w1 - MmToPx(5, dpi), w1 * 2);
  int h3 = 0;
  if (w3 > 0) {
    h3 = w3 * bi.bmHeight / bi.bmWidth;
    ::StretchBlt(dc, L + w1 + MmToPx(5, dpi), y, w3, w3 * bi.bmHeight / bi.bmWidth, mem, 0, 0, bi.bmWidth,
                 bi.bmHeight, SRCCOPY);
  }
  y += std::max(h1, h3) + MmToPx(6, dpi);

  // 領域幅いっぱいに拡大
  Text(dc, L, y, L"Meiryo", 10, dpi, L"② 描画領域の幅いっぱいに拡大 (縦横比維持)");
  y += PtToPx(10, dpi) * 2;
  int fw = W, fh = W * bi.bmHeight / bi.bmWidth;
  if (y + fh > a.bottom - PtToPx(14, dpi)) {
    fh = a.bottom - PtToPx(14, dpi) - y;
    fw = fh * bi.bmWidth / bi.bmHeight;
  }
  ::StretchBlt(dc, L, y, fw, fh, mem, 0, 0, bi.bmWidth, bi.bmHeight, SRCCOPY);
  // 画像の上に文字と線 (重なり)
  Line(dc, GeoPen(MmToPx(0.5, dpi), PS_SOLID, RGB(255, 0, 0)), L, y, L + fw, y + fh);
  ::SetTextColor(dc, RGB(255, 255, 255));
  Text(dc, L + MmToPx(3, dpi), y + MmToPx(3, dpi), L"Yu Gothic UI", 14, dpi, L"画像上の文字 (白)", false, FW_BOLD);
  ::SetTextColor(dc, RGB(0, 0, 0));

  ::SelectObject(mem, old);
  ::DeleteDC(mem);
  (void)H;
}

}  // namespace

void DrawPageContent(HDC hdc, const RECT& area, int pageNo, int dpi) {
  const int saved = ::SaveDC(hdc);
  // 描画領域の目印 (どこまでが描画領域か確認する用)
  HGDIOBJ oldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
  HPEN frame = ::CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
  HGDIOBJ oldPen = ::SelectObject(hdc, frame);
  ::Rectangle(hdc, area.left, area.top, area.right, area.bottom);
  ::SelectObject(hdc, oldPen);
  ::DeleteObject(frame);
  ::SelectObject(hdc, oldBrush);

  // 枠に文字がくっつかないよう少し内側に入れる
  const int pad = MmToPx(3, dpi);
  RECT in{area.left + pad, area.top + pad, area.right - pad, area.bottom - pad};
  if (pageNo == 1) DrawPage1(hdc, in, dpi);
  else DrawPage2(hdc, in, dpi);

  // フッター
  ::SetBkMode(hdc, TRANSPARENT);
  ::SetTextColor(hdc, RGB(0, 0, 0));
  Text(hdc, in.left, in.bottom - PtToPx(9, dpi) * 3 / 2, L"Yu Gothic UI", 9, dpi,
       L"- " + std::to_wstring(pageNo) + L" / " + std::to_wstring(kPageCount) + L" -  area " +
           std::to_wstring(area.right - area.left) + L"x" + std::to_wstring(area.bottom - area.top) + L"px @" +
           std::to_wstring(dpi) + L"dpi");
  ::RestoreDC(hdc, saved);
}

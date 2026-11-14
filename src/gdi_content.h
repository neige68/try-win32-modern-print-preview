// GDI で描く印刷内容。DC と描画領域だけを受け取る (出力先がプレビューでも実プリンターでも同じ)。
#pragma once
#include <windows.h>

// hdc   : 描画先 DC (MM_TEXT 前提、1 論理単位 = 1 デバイスピクセル)
// area  : 描画領域 (デバイスピクセル単位。用紙の印刷可能領域)
// pageNo: 1 または 2
// dpi   : hdc の解像度。フォント・線幅を物理サイズ (pt / mm) から換算するために使う
void DrawPageContent(HDC hdc, const RECT& area, int pageNo, int dpi);

constexpr int kPageCount = 2;

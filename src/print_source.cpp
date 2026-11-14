#include "print_source.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <DocumentSource.h>  // IPrintDocumentPageSource / IPrintPreviewPageCollection (SDK winrt/)
#include <PrintPreview.h>    // IPrintPreviewDxgiPackageTarget (SDK um/)

#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Printing.h>

#include "gdi_content.h"

using Microsoft::WRL::ComPtr;
namespace wgp = winrt::Windows::Graphics::Printing;

// ---------------------------------------------------------------- ログ
void Log(const std::wstring& msg) {
  static std::mutex m;
  std::lock_guard lock(m);
  wchar_t tmp[MAX_PATH]{};
  ::GetTempPathW(MAX_PATH, tmp);
  SYSTEMTIME t;
  ::GetLocalTime(&t);
  wchar_t head[64];
  swprintf_s(head, L"%02d:%02d:%02d.%03d [tid %lu] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
             ::GetCurrentThreadId());
  // UTF-8 で書く
  std::wstring line = std::wstring(head) + msg + L"\n";
  int n = ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string u8(n, '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, u8.data(), n, nullptr, nullptr);
  std::ofstream(std::wstring(tmp) + L"modern_print_preview.log", std::ios::app | std::ios::binary)
      .write(u8.data(), n - 1);
}

int PreviewRenderDpi() {
  wchar_t buf[16]{};
  if (::GetEnvironmentVariableW(L"PREVIEW_DPI", buf, 16)) {
    int v = _wtoi(buf);
    if (v >= 48 && v <= 600) return v;
  }
  return 192;
}

namespace {

std::wstring Fmt(const wchar_t* f, ...) {
  wchar_t buf[512];
  va_list ap;
  va_start(ap, f);
  vswprintf_s(buf, f, ap);
  va_end(ap);
  return buf;
}

// 診断用: 描画した BGRA ビットマップを BMP で保存 (環境変数 DUMP_PREVIEW=1 のとき)
void DumpBmp(const std::wstring& path, int w, int h, const void* bits) {
  BITMAPFILEHEADER fh{'MB', static_cast<DWORD>(sizeof(fh) + sizeof(BITMAPINFOHEADER) + w * h * 4), 0, 0,
                      sizeof(fh) + sizeof(BITMAPINFOHEADER)};
  BITMAPINFOHEADER ih{sizeof(ih), w, -h, 1, 32, BI_RGB, static_cast<DWORD>(w * h * 4), 0, 0, 0, 0};
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<char*>(&fh), sizeof(fh));
  f.write(reinterpret_cast<char*>(&ih), sizeof(ih));
  f.write(static_cast<const char*>(bits), static_cast<std::streamsize>(w) * h * 4);
}

// IPrintDocumentSource (WinRT のマーカーインターフェイス) + IPrintDocumentPageSource + IPrintPreviewPageCollection
struct PrintSource : winrt::implements<PrintSource, wgp::IPrintDocumentSource, IPrintDocumentPageSource,
                                       IPrintPreviewPageCollection> {
  PrintSource() { Log(L"PrintSource created"); }

  // ---- IPrintDocumentPageSource
  HRESULT __stdcall GetPreviewPageCollection(IPrintDocumentPackageTarget* target,
                                             IPrintPreviewPageCollection** collection) noexcept override {
    try {
      std::lock_guard lock(m_mutex);
      Log(L"GetPreviewPageCollection");
      m_dxgiTarget.Reset();
      winrt::check_hresult(target->GetPackageTarget(ID_PREVIEWPACKAGETARGET_DXGI,
                                                    IID_PPV_ARGS(&m_dxgiTarget)));
      if (!m_device) {
        D3D_FEATURE_LEVEL fl;
        winrt::check_hresult(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                                 &m_device, &fl, nullptr));
      }
      *collection = this;
      AddRef();
      return S_OK;
    } catch (...) {
      Log(L"GetPreviewPageCollection failed");
      return winrt::to_hresult();
    }
  }

  HRESULT __stdcall MakeDocument(::IInspectable*, IPrintDocumentPackageTarget*) noexcept override {
    // 実印刷 (用紙/XPS への出力) は本検証の対象外。
    Log(L"MakeDocument called (not implemented: real printing is out of scope)");
    return E_NOTIMPL;
  }

  // ---- IPrintPreviewPageCollection
  HRESULT __stdcall Paginate(UINT32 currentJobPage, ::IInspectable* printTaskOptions) noexcept override {
    try {
      std::lock_guard lock(m_mutex);
      winrt::Windows::Foundation::IInspectable raw{nullptr};
      winrt::copy_from_abi(raw, printTaskOptions);
      auto core = raw.as<wgp::IPrintTaskOptionsCore>();
      const UINT32 page = currentJobPage == JOB_PAGE_APPLICATION_DEFINED ? 1 : currentJobPage;
      auto d = core.GetPageDescription(page);
      m_pageW = d.PageSize.Width;
      m_pageH = d.PageSize.Height;
      m_imgRect = d.ImageableRect;
      m_dpiX = d.DpiX;
      m_dpiY = d.DpiY;
      Log(Fmt(L"Paginate(currentJobPage=%u) page=%.2fx%.2f DIP imageable=(%.2f,%.2f %.2fx%.2f) printerDpi=%ux%u",
              currentJobPage, m_pageW, m_pageH, m_imgRect.X, m_imgRect.Y, m_imgRect.Width, m_imgRect.Height, m_dpiX,
              m_dpiY));
      // 用紙・向きが変わるたびに呼ばれる。ページ数は固定 2。
      winrt::check_hresult(m_dxgiTarget->SetJobPageCount(FinalPageCount, kPageCount));
      return S_OK;
    } catch (...) {
      Log(L"Paginate failed");
      return winrt::to_hresult();
    }
  }

  HRESULT __stdcall MakePage(UINT32 desiredJobPage, FLOAT width, FLOAT height) noexcept override {
    try {
      std::lock_guard lock(m_mutex);
      const UINT32 page = desiredJobPage == JOB_PAGE_APPLICATION_DEFINED ? 1 : desiredJobPage;
      const int dpi = PreviewRenderDpi();
      // MakePage の width/height は「縮小表示サイズ」(DIP) であり、実用紙サイズではない。
      // 内容は Paginate で得た実用紙サイズ (m_pageW x m_pageH DIP = 1/96 inch) で組版し、
      // dpi で換算したサイズのサーフェスに描く。システムがサーフェスを表示サイズへ縮小する。
      const bool havePage = m_pageW > 0 && m_pageH > 0;
      const int pw = static_cast<int>(std::lround((havePage ? m_pageW : width) * dpi / 96.0));
      const int ph = static_cast<int>(std::lround((havePage ? m_pageH : height) * dpi / 96.0));
      Log(Fmt(L"MakePage(desired=%u, %.2fx%.2f DIP) -> render %dx%d px @%d dpi", desiredJobPage, width, height, pw, ph,
              dpi));

      // GDI: 32bit トップダウン DIB セクション + メモリ DC に描く
      BITMAPINFO bi{};
      bi.bmiHeader = {sizeof(BITMAPINFOHEADER), pw, -ph, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
      void* bits = nullptr;
      HDC screen = ::GetDC(nullptr);
      HDC mem = ::CreateCompatibleDC(screen);
      ::ReleaseDC(nullptr, screen);
      HBITMAP dib = ::CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
      if (!dib) {
        ::DeleteDC(mem);
        return E_OUTOFMEMORY;
      }
      HGDIOBJ old = ::SelectObject(mem, dib);
      RECT all{0, 0, pw, ph};
      ::FillRect(mem, &all, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));

      // 描画領域 = 印刷可能領域 (DIP → px)。Paginate 前に呼ばれた場合は全面。
      const double s = dpi / 96.0;
      RECT area = all;
      if (m_imgRect.Width > 0 && m_imgRect.Height > 0) {
        area = {static_cast<LONG>(std::lround(m_imgRect.X * s)), static_cast<LONG>(std::lround(m_imgRect.Y * s)),
                static_cast<LONG>(std::lround((m_imgRect.X + m_imgRect.Width) * s)),
                static_cast<LONG>(std::lround((m_imgRect.Y + m_imgRect.Height) * s))};
      }
      ::SetGraphicsMode(mem, GM_COMPATIBLE);
      DrawPageContent(mem, area, static_cast<int>(page), dpi);
      ::GdiFlush();

      // GDI は alpha を書かないので不透明 (255) に補正してからテクスチャへ
      auto* px = static_cast<uint32_t*>(bits);
      for (size_t i = 0, n = static_cast<size_t>(pw) * ph; i < n; ++i) px[i] |= 0xFF000000u;

      wchar_t dump[8]{};
      if (::GetEnvironmentVariableW(L"DUMP_PREVIEW", dump, 8)) {
        wchar_t tmp[MAX_PATH]{};
        ::GetTempPathW(MAX_PATH, tmp);
        DumpBmp(Fmt(L"%spreview_page%u_%dx%d.bmp", tmp, page, pw, ph), pw, ph, bits);
      }

      D3D11_TEXTURE2D_DESC td{};
      td.Width = pw;
      td.Height = ph;
      td.MipLevels = td.ArraySize = 1;
      td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
      D3D11_SUBRESOURCE_DATA init{bits, static_cast<UINT>(pw * 4), 0};
      ComPtr<ID3D11Texture2D> tex;
      HRESULT hr = m_device->CreateTexture2D(&td, &init, &tex);
      ::SelectObject(mem, old);
      ::DeleteObject(dib);
      ::DeleteDC(mem);
      winrt::check_hresult(hr);

      ComPtr<IDXGISurface> surf;
      winrt::check_hresult(tex.As(&surf));
      hr = m_dxgiTarget->DrawPage(page, surf.Get(), static_cast<FLOAT>(dpi), static_cast<FLOAT>(dpi));
      Log(Fmt(L"DrawPage(page=%u) -> 0x%08X", page, static_cast<unsigned>(hr)));
      return hr;
    } catch (...) {
      Log(L"MakePage failed");
      return winrt::to_hresult();
    }
  }

 private:
  std::mutex m_mutex;
  ComPtr<IPrintPreviewDxgiPackageTarget> m_dxgiTarget;
  ComPtr<ID3D11Device> m_device;
  double m_pageW = 0, m_pageH = 0;
  winrt::Windows::Foundation::Rect m_imgRect{};
  UINT32 m_dpiX = 0, m_dpiY = 0;
};

}  // namespace

wgp::IPrintDocumentSource CreatePrintSource() { return winrt::make<PrintSource>(); }

# try-win32-modern-print-preview

[日本語](README.md) | **English**

An experiment to find out whether a Win32 (C++/WinRT) app can show its **GDI drawing in the preview pane of the new
Windows 11 print UI**. Printing to a real printer is out of scope.

## Conclusion
- **Yes, it works.** Without XAML, you can implement `IPrintDocumentPageSource` / `IPrintPreviewPageCollection`
  yourself and feed GDI output to the preview.
- Works in both a plain Win32 app and an OWLNext 7 app (verified with "Microsoft Print to PDF").
- Changing the paper size (A3 → A4) or orientation (landscape → portrait) is reflected in the preview
  (`Paginate` is called again on every change).

## Layout
- `src/main.cpp` : plain Win32 version. A window with a "Print" button (`modern_print_preview.exe`)
- `src/owl_main.cpp` : OWLNext 7 version. `TApplication` / `TFrameWindow` / `TButton` (`modern_print_preview_owl.exe`)
- `src/print_ui.cpp` : `ShowPrintUi(HWND)`. `IPrintManagerInterop::GetForWindow` / `ShowPrintUIForWindowAsync`.
  Does not expose any winrt headers
- `src/print_source.cpp` : XAML-free print source (`IPrintDocumentSource` + `IPrintDocumentPageSource` +
  `IPrintPreviewPageCollection`)
- `src/gdi_content.cpp` : GDI drawing, `DrawPageContent(HDC, RECT, pageNo, dpi)` (text, lines, bitmap)
- `assets/sample.bmp` : the image that is loaded from a file (copied next to the exe)

## Approach
1. `Paginate`: get the real paper size (in DIPs) from `IPrintTaskOptionsCore::GetPageDescription`, then call
   `SetJobPageCount(2)`.
2. `MakePage`: draw with GDI into a DIB section selected into a memory DC, sized as *real paper size × `PREVIEW_DPI`*.
3. Fix up the alpha channel to 255, copy into a D3D11 texture (BGRA), and hand it to
   `IPrintPreviewDxgiPackageTarget::DrawPage`.

Alternative considered and not taken: XAML Islands + `Windows.UI.Xaml.Printing.PrintDocument` (used by KiCad). It adds a
heavy dependency for a small pure-Win32 app. It was not implemented for comparison.

## Build
VS2022 + Windows SDK 10.0.26100 (the C++/WinRT headers ship with the SDK).

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release   # or --config Debug

- The OWL version is built only when the `OWLROOT` environment variable is set (the OWLNext 7 root directory that
  contains `include/` and `lib/`; verified with 7.0.20).
- The OWL static library (`owl-7.0-v1930-x64-tu.lib`) is built with `/MT`, so both executables use `/MT`
  (Debug uses `/MTd` + `owl-7.0-v1930-x64-dtu.lib`; OWL's `__DEBUG` becomes 1 automatically from `_DEBUG`, so no extra
  setting is needed). Zero warnings in both Release and Debug.

## Running and diagnostics
- `--auto` : show the print UI automatically one second after start-up.
- Log: `%TEMP%\modern_print_preview.log` (calls to `Paginate` / `MakePage` with their sizes).
- Environment variable `PREVIEW_DPI` (default 192): rendering resolution of the preview.
- Environment variable `DUMP_PREVIEW=1`: save the rendered BGRA bitmaps to `%TEMP%\preview_page*.bmp`.

## Image quality notes
- The preview pane is only about 500 DIPs wide and the system scales the surface down. Small text (5–7 pt) is hard to
  read, but the shapes of Japanese glyphs do not break.
- 1 px hairlines become light grey after downscaling; dashed and dotted lines partly vanish.
- The bitmap holds up, but a checkerboard pattern shows some moiré.
- Whether the preview is re-rendered when you zoom in is not verified.

## Coexistence with OWLNext 7
- All that is passed to the print UI is the `HWND` from `TWindow::GetHandle()`. The OWL core does not initialize COM,
  so call `InitWinRt()` (STA) at the start of `OwlMain`.
- Do not mix OWL and C++/WinRT headers in one translation unit (`print_ui.h` includes only `windows.h`).
- OWL supplies `_tmain`, so the OWL executable needs `WIN32` (GUI subsystem) plus `/ENTRY:wmainCRTStartup`.
- OWL (`wsysinc.h`) defines `WIN32_LEAN_AND_MEAN` without a value, so it is also defined empty here
  (`WIN32_LEAN_AND_MEAN=`) to avoid warning C4005.

## Limitations and unverified items
- Real printing (`MakeDocument`) is not implemented (`E_NOTIMPL`). The dialog's "Print" button does not work.
- Not verified: re-rendering on zoom, printers other than Microsoft Print to PDF (margins = `ImageableRect`), x86,
  STA consistency when OCF/OLE is also used.
- Concerns for production use: keeping the real printing path (e.g. `ID2D1PrintControl`) consistent with the preview;
  the callbacks arrive on arbitrary threads; memory for large paper at high DPI (about 28 MB per page for A3 at
  192 dpi); there is no official C++ sample, so behavior was confirmed on a real machine.

## References
- [`IPrintManagerInterop::ShowPrintUIForWindowAsync`](https://learn.microsoft.com/en-us/windows/win32/api/printmanagerinterop/nf-printmanagerinterop-iprintmanagerinterop-showprintuiforwindowasync)
- [`IPrintDocumentPageSource`](https://learn.microsoft.com/en-us/previous-versions/jj710015(v=vs.85)) — "When using XAML or HTML, apps do
  not implement this interface"
- Windows SDK 10.0.26100 headers: `DocumentSource.h`, `PrintPreview.h`, `PrintManagerInterop.h`

## License
MIT License (see `LICENSE`). OWLNext is not bundled; obtain it separately under its own license.

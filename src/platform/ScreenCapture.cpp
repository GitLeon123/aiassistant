#include "platform/ScreenCapture.h"

#include <dwmapi.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>

#include "core/Util.h"

using Microsoft::WRL::ComPtr;

namespace aia {

ForegroundInfo QueryForegroundWindow() {
  return QueryWindow(GetForegroundWindow());
}

ForegroundInfo QueryWindow(HWND hwnd) {
  ForegroundInfo info;
  if (!hwnd || !IsWindow(hwnd)) return info;
  info.hwnd = hwnd;
  wchar_t title[512] = {};
  GetWindowTextW(hwnd, title, ARRAYSIZE(title));
  info.title = Narrow(title);
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &info.rect, sizeof(info.rect)))) {
    GetWindowRect(hwnd, &info.rect);
  }
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
    wchar_t path[MAX_PATH * 2];
    DWORD size = ARRAYSIZE(path);
    if (QueryFullProcessImageNameW(process, 0, path, &size)) info.process = Narrow(PathFindFileNameW(path));
    CloseHandle(process);
  }
  return info;
}

int MonitorNumber(HMONITOR monitor) {
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(monitor, &info)) return 0;
  // Match the DISPLAY number Windows exposes, rather than cursor/enum order.
  const std::wstring device = info.szDevice;
  const auto at = device.find(L"DISPLAY");
  return at == std::wstring::npos ? 0 : _wtoi(device.c_str() + at + 7);
}

ScreenImage CaptureMonitor(HMONITOR monitor) {
  ScreenImage img;
  MONITORINFO mi{sizeof(mi)};
  if (!GetMonitorInfoW(monitor, &mi)) return img;
  RECT r = mi.rcMonitor;
  int w = r.right - r.left, h = r.bottom - r.top;
  if (w <= 0 || h <= 0) return img;

  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bmp && bits) {
    HGDIOBJ old = SelectObject(mem, bmp);
    if (BitBlt(mem, 0, 0, w, h, screen, r.left, r.top, SRCCOPY | CAPTUREBLT)) {
      GdiFlush();
      img.bounds = r;
      img.width = w;
      img.height = h;
      img.pixels.assign(static_cast<uint8_t*>(bits), static_cast<uint8_t*>(bits) + size_t(w) * h * 4);
    }
    SelectObject(mem, old);
  }
  if (bmp) DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return img;
}

namespace {

IWICImagingFactory* Factory() {
  // Created on first use on the calling thread's apartment; WIC factories are free-threaded.
  static ComPtr<IWICImagingFactory> factory = [] {
    ComPtr<IWICImagingFactory> f;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    return f;
  }();
  return factory.Get();
}

}  // namespace

std::vector<uint8_t> EncodeRegion(const ScreenImage& image, RECT region, int maxSide, double scale,
                                  ImageFormat format, float jpegQuality) {
  std::vector<uint8_t> out;
  IWICImagingFactory* factory = Factory();
  if (!factory || image.Empty()) return out;

  // Region in image coordinates, clamped.
  int x0 = std::clamp<int>(region.left - image.bounds.left, 0, image.width);
  int y0 = std::clamp<int>(region.top - image.bounds.top, 0, image.height);
  int x1 = std::clamp<int>(region.right - image.bounds.left, 0, image.width);
  int y1 = std::clamp<int>(region.bottom - image.bounds.top, 0, image.height);
  if (x1 - x0 < 2 || y1 - y0 < 2) return out;
  int cw = x1 - x0, ch = y1 - y0;

  double s = std::min(1.0, scale);
  if (maxSide > 0) s = std::min(s, double(maxSide) / std::max(cw, ch));
  UINT ow = std::max(1u, UINT(cw * s + 0.5)), oh = std::max(1u, UINT(ch * s + 0.5));

  ComPtr<IWICBitmap> bitmap;
  const UINT stride = UINT(image.width) * 4;
  if (FAILED(factory->CreateBitmapFromMemory(UINT(image.width), UINT(image.height), GUID_WICPixelFormat32bppBGR,
                                             stride, UINT(image.pixels.size()),
                                             const_cast<BYTE*>(image.pixels.data()), &bitmap))) {
    return out;
  }
  ComPtr<IWICBitmapClipper> clipper;
  WICRect clip{x0, y0, cw, ch};
  if (FAILED(factory->CreateBitmapClipper(&clipper)) || FAILED(clipper->Initialize(bitmap.Get(), &clip))) return out;

  ComPtr<IWICBitmapSource> source = clipper;
  if (ow != UINT(cw) || oh != UINT(ch)) {
    ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
        FAILED(scaler->Initialize(source.Get(), ow, oh, WICBitmapInterpolationModeHighQualityCubic))) {
      return out;
    }
    source = scaler;
  }
  ComPtr<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(&converter)) ||
      FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom))) {
    return out;
  }

  ComPtr<IStream> stream;
  stream.Attach(SHCreateMemStream(nullptr, 0));
  if (!stream) return out;
  ComPtr<IWICBitmapEncoder> encoder;
  if (FAILED(factory->CreateEncoder(format == ImageFormat::Jpeg ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng,
                                    nullptr, &encoder)) ||
      FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
    return out;
  }
  ComPtr<IWICBitmapFrameEncode> frame;
  ComPtr<IPropertyBag2> props;
  if (FAILED(encoder->CreateNewFrame(&frame, &props))) return out;
  if (format == ImageFormat::Jpeg && props) {
    PROPBAG2 opt{};
    opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_R4;
    v.fltVal = jpegQuality;
    props->Write(1, &opt, &v);
  }
  WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
  if (FAILED(frame->Initialize(props.Get())) || FAILED(frame->SetSize(ow, oh)) ||
      FAILED(frame->SetPixelFormat(&fmt)) || FAILED(frame->WriteSource(converter.Get(), nullptr)) ||
      FAILED(frame->Commit()) || FAILED(encoder->Commit())) {
    return out;
  }

  STATSTG stat{};
  if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) return out;
  out.resize(size_t(stat.cbSize.QuadPart));
  LARGE_INTEGER zero{};
  stream->Seek(zero, STREAM_SEEK_SET, nullptr);
  ULONG read = 0;
  stream->Read(out.data(), ULONG(out.size()), &read);
  out.resize(read);
  return out;
}

}  // namespace aia

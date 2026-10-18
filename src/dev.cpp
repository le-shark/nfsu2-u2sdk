#include "u2sdk/dev.h"

#include <d3d9.h>
#include <windows.h>
#include <wincodec.h>

#include <vector>

#include "u2sdk/log.h"
#include "u2sdk/u2math.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace u2::dev {
namespace {
std::string g_shotPath;
int g_shotScale = 1;

void Shot(IDirect3DDevice9* dev) {
  std::string path = g_shotPath;
  g_shotPath.clear();
  IDirect3DSurface9 *bb = nullptr, *rt = nullptr, *sys = nullptr;
  bool ok = false;
  if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) {
    D3DSURFACE_DESC d;
    bb->GetDesc(&d);
    if (SUCCEEDED(dev->CreateRenderTarget(d.Width, d.Height, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, nullptr)) &&
        SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr)) &&
        SUCCEEDED(dev->StretchRect(bb, nullptr, rt, nullptr, D3DTEXF_NONE)) && SUCCEEDED(dev->GetRenderTargetData(rt, sys))) {
      D3DLOCKED_RECT lr;
      if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
        int s = g_shotScale, w = static_cast<int>(d.Width) / s, h = static_cast<int>(d.Height) / s;
        std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
        for (int y = 0; y < h; ++y)
          for (int x = 0; x < w; ++x) {
            unsigned sum[3] = {};
            for (int dy = 0; dy < s; ++dy) {
              auto p = static_cast<const unsigned char*>(lr.pBits) + static_cast<size_t>(y * s + dy) * lr.Pitch + x * s * 4;
              for (int dx = 0; dx < s; ++dx, p += 4) { sum[0] += p[2]; sum[1] += p[1]; sum[2] += p[0]; }
            }
            for (int c = 0; c < 3; ++c) rgb[(static_cast<size_t>(y) * w + x) * 3 + c] = static_cast<unsigned char>(sum[c] / (s * s));
          }
        sys->UnlockRect();
        ok = SavePng(path, w, h, rgb.data());
      }
    }
  }
  if (sys) sys->Release();
  if (rt) rt->Release();
  if (bb) bb->Release();
  U2LOG("dev: shot %s %s", path.c_str(), ok ? "ok" : "FAILED");
}
}  // namespace

void RequestShot(const std::string& path, int scale) {
  g_shotPath = path;
  g_shotScale = ClampInt(scale, 1, 4);
}

namespace {
template <class T>
void SafeRelease(T*& p) {
  if (p) p->Release();
  p = nullptr;
}

// Windows' own PNG encoder (WIC), fed one row at a time: no buffer the size of the image, and every failure is an
// HRESULT. stb_image_write builds the whole filtered image and its zlib stream in memory and, in a release build,
// writes through the pointer of a failed realloc (stbiw__sbgrowf only asserts): a 2x 4K photo needs about 300 MB
// more in a 2 GB process (the exe is not large-address-aware; 2026-10-18 review).
bool SavePngWic(const std::wstring& path, int w, int h, const unsigned char* rgb, HRESULT* fail) {
  // Factory 1: the SDK maps CLSID_WICImagingFactory to version 2, which Windows 7 lacks without its platform update.
  IWICImagingFactory* f = nullptr;
  IWICStream* s = nullptr;
  IWICBitmapEncoder* enc = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
  if (SUCCEEDED(hr)) hr = f->CreateStream(&s);
  if (SUCCEEDED(hr)) hr = s->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
  if (SUCCEEDED(hr)) hr = f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
  if (SUCCEEDED(hr)) hr = enc->Initialize(s, WICBitmapEncoderNoCache);
  if (SUCCEEDED(hr)) hr = enc->CreateNewFrame(&frame, nullptr);
  if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
  if (SUCCEEDED(hr)) hr = frame->SetSize(static_cast<UINT>(w), static_cast<UINT>(h));
  WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
  if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&fmt);
  if (SUCCEEDED(hr) && fmt != GUID_WICPixelFormat24bppBGR) hr = E_FAIL;
  if (SUCCEEDED(hr)) {
    std::vector<unsigned char> row(static_cast<size_t>(w) * 3);
    for (int y = 0; y < h && SUCCEEDED(hr); ++y) {
      const unsigned char* src = rgb + static_cast<size_t>(y) * w * 3;
      for (int x = 0; x < w; ++x) row[x * 3] = src[x * 3 + 2], row[x * 3 + 1] = src[x * 3 + 1], row[x * 3 + 2] = src[x * 3];
      hr = frame->WritePixels(1, static_cast<UINT>(w) * 3, static_cast<UINT>(row.size()), row.data());
    }
  }
  if (SUCCEEDED(hr)) hr = frame->Commit();
  if (SUCCEEDED(hr)) hr = enc->Commit();
  SafeRelease(frame);
  SafeRelease(enc);
  SafeRelease(s);
  SafeRelease(f);
  *fail = hr;
  return SUCCEEDED(hr);
}

std::wstring Wide(const std::string& s) {
  int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
  std::wstring w(n > 0 ? n - 1 : 0, L'\0');
  if (n > 1) MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, w.data(), n);
  return w;
}
}  // namespace

bool SavePng(const std::string& path, int w, int h, const unsigned char* rgb) {
  // Written under a temporary name and renamed when complete, so a quit during the save leaves no broken PNG.
  std::wstring final = Wide(path), tmp = final + L".tmp";
  HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  HRESULT hr = S_OK;
  bool ok = SavePngWic(tmp, w, h, rgb, &hr);
  if (SUCCEEDED(init)) CoUninitialize();
  if (!ok) {
    DeleteFileW(tmp.c_str());
    // Fallback for systems without the WIC PNG encoder: stb, only where its buffers fit comfortably.
    U2LOG("png: the Windows encoder failed (hr=0x%08lX)%s", static_cast<unsigned long>(hr),
          static_cast<double>(w) * h * 3 <= 64.0 * 1024 * 1024 ? "; trying stb_image_write" : "");
    if (static_cast<double>(w) * h * 3 > 64.0 * 1024 * 1024) return false;
    std::string tmpA = path + ".tmp";
    ok = stbi_write_png(tmpA.c_str(), w, h, 3, rgb, w * 3) != 0;
    if (!ok) {
      DeleteFileA(tmpA.c_str());
      return false;
    }
  }
  return MoveFileExW(tmp.c_str(), final.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

void OnEndScene(IDirect3DDevice9* dev) {
  if (!g_shotPath.empty()) Shot(dev);
}
}  // namespace u2::dev

#include "u2sdk/dev.h"

#include <d3d9.h>
#include <windows.h>

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

bool SavePng(const std::string& path, int w, int h, const unsigned char* rgb) {
  return stbi_write_png(path.c_str(), w, h, 3, rgb, w * 3) != 0;
}

void OnEndScene(IDirect3DDevice9* dev) {
  if (!g_shotPath.empty()) Shot(dev);
}
}  // namespace u2::dev

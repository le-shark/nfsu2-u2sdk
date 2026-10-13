#include "u2sdk/texture.h"

#include <d3d9.h>

#include <algorithm>
#include <cstring>

#include "u2sdk/game.h"
#include "u2sdk/log.h"

namespace u2::texture {

IDirect3DTexture9* LoadDds(const uint8_t* d, size_t size, const char* name) {
  auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, d + o, 4); return v; };
  if (!d || size < 128 || u32(0) != 0x20534444) { U2LOG("tex: %s is not a DDS file", name); return nullptr; }
  uint32_t h = u32(12), w = u32(16), mips = std::max<uint32_t>(1, u32(28));
  uint32_t pfFlags = u32(80), fourCC = u32(84), bits = u32(88), rMask = u32(92), aMask = u32(104);
  D3DFORMAT fmt = D3DFMT_UNKNOWN;
  int blockBytes = 0;
  if (pfFlags & 0x4) {
    if (fourCC == MAKEFOURCC('D', 'X', 'T', '1')) { fmt = D3DFMT_DXT1; blockBytes = 8; }
    if (fourCC == MAKEFOURCC('D', 'X', 'T', '3')) { fmt = D3DFMT_DXT3; blockBytes = 16; }
    if (fourCC == MAKEFOURCC('D', 'X', 'T', '5')) { fmt = D3DFMT_DXT5; blockBytes = 16; }
  } else if ((pfFlags & 0x40) && bits == 32 && rMask == 0x00FF0000) {
    fmt = ((pfFlags & 0x1) && aMask) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
  }
  if (fmt == D3DFMT_UNKNOWN || !w || !h) { U2LOG("tex: %s: unsupported DDS format", name); return nullptr; }
  IDirect3DDevice9* dev = u2::Device();
  IDirect3DTexture9* t = nullptr;
  if (!dev || FAILED(dev->CreateTexture(w, h, mips, 0, fmt, D3DPOOL_MANAGED, &t, nullptr))) {
    U2LOG("tex: CreateTexture %ux%u failed for %s", w, h, name);
    return nullptr;
  }
  size_t off = 128;
  for (uint32_t lv = 0; lv < mips; ++lv) {
    uint32_t lw = std::max<uint32_t>(1, w >> lv), lh = std::max<uint32_t>(1, h >> lv);
    uint32_t rows = blockBytes ? std::max<uint32_t>(1, (lh + 3) / 4) : lh;
    uint32_t rowBytes = blockBytes ? std::max<uint32_t>(1, (lw + 3) / 4) * blockBytes : lw * 4;
    if (off + size_t(rows) * rowBytes > size) { U2LOG("tex: %s truncated at mip %u", name, lv); break; }
    D3DLOCKED_RECT r;
    if (FAILED(t->LockRect(lv, &r, nullptr, 0))) break;
    for (uint32_t y = 0; y < rows; ++y)
      std::memcpy(static_cast<uint8_t*>(r.pBits) + size_t(y) * r.Pitch, d + off + size_t(y) * rowBytes, rowBytes);
    t->UnlockRect(lv);
    off += size_t(rows) * rowBytes;
  }
  U2LOG("tex: loaded %s %ux%u mips %u fmt %08X", name, w, h, mips, static_cast<unsigned>(fmt));
  return t;
}

IDirect3DTexture9* CreateArgb(uint32_t w, uint32_t h, const std::vector<std::vector<uint32_t>>& levels, const char* name) {
  IDirect3DDevice9* dev = u2::Device();
  IDirect3DTexture9* t = nullptr;
  UINT n = static_cast<UINT>(levels.size());
  if (!dev || !n || FAILED(dev->CreateTexture(w, h, n, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr))) {
    U2LOG("tex: CreateTexture %ux%u (%u levels) failed for %s", w, h, n, name);
    return nullptr;
  }
  for (UINT lv = 0; lv < n; ++lv) {
    uint32_t lw = std::max<uint32_t>(1, w >> lv), lh = std::max<uint32_t>(1, h >> lv);
    if (levels[lv].size() < size_t(lw) * lh) { U2LOG("tex: %s level %u too small", name, lv); t->Release(); return nullptr; }
    D3DLOCKED_RECT r;
    if (FAILED(t->LockRect(lv, &r, nullptr, 0))) { U2LOG("tex: %s LockRect level %u failed", name, lv); t->Release(); return nullptr; }
    for (uint32_t y = 0; y < lh; ++y)
      std::memcpy(static_cast<uint8_t*>(r.pBits) + size_t(y) * r.Pitch, levels[lv].data() + size_t(y) * lw, lw * 4);
    t->UnlockRect(lv);
  }
  U2LOG("tex: created %s %ux%u levels %u", name, w, h, n);
  return t;
}

}  // namespace u2::texture

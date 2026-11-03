#pragma once
// JDLZ decompression (EA's LZ format of the .lzc bundles, e.g. GLOBAL\InGameCommon.lzc). Pure (no game calls),
// unit-tested. Ported from tools/shadows/carshadow_sim/jdlz.py (community algorithm, Nikki CoreExtensions JDLZ), which
// decodes the installed InGameCommon.lzc byte-identical to InGameCommon.bun (checked 2026-11-03). Bounds-checked: a
// damaged stream returns false instead of reading or writing outside the buffers.
//
// Header (16 bytes): "JDLZ", u8 2, u8 0x10, u16 0, u32 decompressed size, u32 compressed size (header included).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace u2::jdlz {

inline bool IsJdlz(const uint8_t* data, size_t size) { return size >= 16 && std::memcmp(data, "JDLZ", 4) == 0; }

inline bool Decompress(const uint8_t* data, size_t size, std::vector<uint8_t>* out) {
  if (!IsJdlz(data, size)) return false;
  uint32_t usize, csize;
  std::memcpy(&usize, data + 8, 4);
  std::memcpy(&csize, data + 12, 4);
  if (csize > size || csize < 16) return false;
  out->assign(usize, 0);
  uint8_t* o = out->data();
  size_t i = 16, w = 0;
  const size_t end = csize;
  unsigned f1 = 1, f2 = 1;
  while (i < end && w < usize) {
    if (f1 == 1) {
      f1 = data[i++] | 0x100u;
      if (i >= end) break;
    }
    if (f2 == 1) {
      f2 = data[i++] | 0x100u;
      if (i >= end) break;
    }
    if (f1 & 1) {
      if (i + 2 > end) return false;
      size_t len, back;
      if (f2 & 1) {
        len = (data[i + 1] | ((data[i] & 0xF0u) << 4)) + 3;
        back = (data[i] & 0x0Fu) + 1;
      } else {
        back = (data[i + 1] | ((data[i] & 0xE0u) << 3)) + 17;
        len = (data[i] & 0x1Fu) + 3;
      }
      i += 2;
      if (back > w || len > usize - w) return false;
      for (size_t k = 0; k < len; ++k) o[w + k] = o[w + k - back];  // may overlap: byte by byte
      w += len;
      f2 >>= 1;
    } else {
      o[w++] = data[i++];
    }
    f1 >>= 1;
  }
  return w == usize;
}

}  // namespace u2::jdlz

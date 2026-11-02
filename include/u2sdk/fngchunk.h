#pragma once
// Finds an FEng package's layout chunk in a bundle file's bytes. Pure (no game calls), unit-tested.
//
// A bundle (.bun) is a run of top-level chunks {u32 id, u32 size, size bytes}. A package layout is chunk 0x30203 (the
// id the registration 0x51BD30 accepts, with 0x30210); its package name is a C string at +0x30 of the chunk
// (GLOBAL\InGameCommon.bun: UI_PauseOptions.fng at 0x14B780, size 0x1D420 + 8; native-screen.md, refs/data
// UI_PauseOptions.fngchunk is the same bytes).
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace u2::fngchunk {

constexpr uint32_t kLayoutChunk = 0x30203;
constexpr size_t kNameOffset = 0x30;

// The whole chunk (header included): its offset and length in data, or false.
inline bool Find(const uint8_t* data, size_t size, const char* name, size_t* off, size_t* len) {
  const size_t nameLen = std::strlen(name);
  for (size_t p = 0; p + 8 <= size;) {
    uint32_t id, body;
    std::memcpy(&id, data + p, 4);
    std::memcpy(&body, data + p + 4, 4);
    if (body > size - p - 8) return false;  // truncated or not a bundle
    if (id == kLayoutChunk && body >= kNameOffset - 8 + nameLen + 1) {
      const char* n = reinterpret_cast<const char*>(data + p + kNameOffset);
      bool same = true;
      for (size_t i = 0; i <= nameLen && same; ++i) {
        char a = n[i], b = name[i];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        same = a == b;
      }
      if (same) {
        *off = p;
        *len = body + 8;
        return true;
      }
    }
    p += 8 + static_cast<size_t>(body);
  }
  return false;
}

}  // namespace u2::fngchunk

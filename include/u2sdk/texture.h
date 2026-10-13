#pragma once
// Our own textures on the game's device: a minimal DDS reader (DXT1/3/5 or 32-bit, with mips) into a
// D3DPOOL_MANAGED texture, which survives device resets. From the sky spike (scripts/spikes/sky_spike, [game]
// 2026-10-05: the dome drew a DDS loaded this way). No game file is touched.
#include <cstddef>
#include <cstdint>
#include <vector>

struct IDirect3DTexture9;

namespace u2::texture {

// data: a whole .dds file in memory (e.g. an RCDATA resource). name: for the log. Null on any failure (logged).
IDirect3DTexture9* LoadDds(const uint8_t* data, size_t size, const char* name);

// A MANAGED A8R8G8B8 texture from texels computed in code: levels[0] is w x h, each next level half the size (as
// many levels as given). Null on any failure (logged).
IDirect3DTexture9* CreateArgb(uint32_t w, uint32_t h, const std::vector<std::vector<uint32_t>>& levels, const char* name);

}  // namespace u2::texture

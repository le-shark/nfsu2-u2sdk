#pragma once
// Our own textures on the game's device: a minimal DDS reader (DXT1/3/5 or 32-bit, with mips) into a
// D3DPOOL_MANAGED texture, which survives device resets. From the sky spike (scripts/spikes/sky_spike, [game]
// 2026-10-05: the dome drew a DDS loaded this way). No game file is touched.
#include <cstddef>
#include <cstdint>

struct IDirect3DTexture9;

namespace u2::texture {

// data: a whole .dds file in memory (e.g. an RCDATA resource). name: for the log. Null on any failure (logged).
IDirect3DTexture9* LoadDds(const uint8_t* data, size_t size, const char* name);

}  // namespace u2::texture

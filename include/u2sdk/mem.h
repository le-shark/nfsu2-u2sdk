#pragma once
// Raw memory access and code patching in the game process. All writes go through VirtualProtect.
#include <cstddef>
#include <cstdint>

namespace u2::mem {

void WriteBytes(uintptr_t addr, const void* data, size_t size);

template <typename T>
inline void Write(uintptr_t addr, const T& value) { WriteBytes(addr, &value, sizeof(T)); }

void MakeJmp(uintptr_t at, const void* to);   // E9 rel32

}  // namespace u2::mem

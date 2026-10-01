#pragma once
// Raw memory access and code patching in the game process. All writes go through VirtualProtect.
#include <cstddef>
#include <cstdint>

namespace u2::mem {

template <typename T>
inline T& Ref(uintptr_t addr) { return *reinterpret_cast<T*>(addr); }

void WriteBytes(uintptr_t addr, const void* data, size_t size);

template <typename T>
inline void Write(uintptr_t addr, const T& value) { WriteBytes(addr, &value, sizeof(T)); }

void Nop(uintptr_t addr, size_t size);
void MakeJmp(uintptr_t at, const void* to);   // E9 rel32
void MakeCall(uintptr_t at, const void* to);  // E8 rel32

// Destination of an E8/E9 rel32 instruction at `at`, or 0 if it isn't one.
uintptr_t BranchTarget(uintptr_t at);

}  // namespace u2::mem

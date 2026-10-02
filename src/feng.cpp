#include "u2sdk/feng.h"

#include "u2sdk/game.h"

namespace u2::feng {
namespace {
template <typename Fn>
Fn At(uintptr_t a) { return reinterpret_cast<Fn>(a); }
}  // namespace

uint32_t HashUpper(const char* s) { return At<uint32_t(__cdecl*)(const char*)>(0x505450)(s); }

FEPackage* FindPackage(const char* pkg) { return At<FEPackage*(__cdecl*)(const char*)>(0x52CEF0)(pkg); }
FEObject* FindObject(const char* pkg, uint32_t h) { return At<FEObject*(__cdecl*)(const char*, uint32_t)>(0x5379C0)(pkg, h); }
void SetInvisible(FEObject* o) { At<void(__cdecl*)(FEObject*)>(0x50CA00)(o); }

void Print(const char* pkg, uint32_t hash, const char* text) {
  At<void(__cdecl*)(const char*, uint32_t, const char*, ...)>(0x537B80)(pkg, hash, "%s", text);
}

FEPackage* FirstActivePackage() {
  auto ui = Var<uint8_t*>(addr::pFEng);
  if (!ui) return nullptr;
  auto feng = *reinterpret_cast<uint8_t**>(ui + 8);
  return feng ? *reinterpret_cast<FEPackage**>(feng + 0xE4) : nullptr;
}
}  // namespace u2::feng

#include "u2sdk/feng.h"

#include "u2sdk/game.h"

namespace u2::feng {
namespace {
template <typename Fn>
Fn At(uintptr_t a) { return reinterpret_cast<Fn>(a); }
}  // namespace

uint32_t HashUpper(const char* s) { return At<uint32_t(__cdecl*)(const char*)>(0x505450)(s); }
uint32_t StringHash(const char* s) { return At<uint32_t(__cdecl*)(const char*)>(0x43DB50)(s); }

FEPackage* FindPackage(const char* pkg) { return At<FEPackage*(__cdecl*)(const char*)>(0x52CEF0)(pkg); }
FEObject* FindObject(const char* pkg, uint32_t h) { return At<FEObject*(__cdecl*)(const char*, uint32_t)>(0x5379C0)(pkg, h); }
FEObject* FindString(const char* pkg, uint32_t h) { return At<FEObject*(__cdecl*)(const char*, uint32_t)>(0x537A10)(pkg, h); }
void SetVisible(FEObject* o) { At<void(__cdecl*)(FEObject*)>(0x50CA50)(o); }
void SetInvisible(FEObject* o) { At<void(__cdecl*)(FEObject*)>(0x50CA00)(o); }

void Print(const char* pkg, uint32_t hash, const char* text) {
  At<void(__cdecl*)(const char*, uint32_t, const char*, ...)>(0x537B80)(pkg, hash, "%s", text);
}

void SetCenter(FEObject* o, float x, float y) { At<void(__cdecl*)(FEObject*, float, float)>(0x51D4D0)(o, x, y); }
void GetCenter(FEObject* o, float* x, float* y) { At<void(__cdecl*)(FEObject*, float*, float*)>(0x51D620)(o, x, y); }
void SetColor(FEObject* o, uint32_t argb) { At<void(__cdecl*)(FEObject*, uint32_t)>(0x51D7F0)(o, argb); }
void ShowPackage(const char* pkg) { At<void(__cdecl*)(const char*, void*)>(0x555E80)(pkg, nullptr); }
void RemovePackage(const char* pkg) { At<void(__cdecl*)(const char*)>(0x5379A0)(pkg); }

namespace {
void* CFEng() {
  auto ui = Var<uint8_t*>(addr::pFEng);
  return ui ? *reinterpret_cast<void**>(ui + 8) : nullptr;
}
}  // namespace

void PushOverlayPackage(const char* pkg) {
  if (void* f = CFEng()) At<void(__fastcall*)(void*, void*, const char*, int, unsigned)>(0x555860)(f, nullptr, pkg, 0, 0);
}
void PopOverlayPackage(FEPackage* p) {
  if (void* f = CFEng()) At<void(__fastcall*)(void*, void*, FEPackage*)>(0x5362E0)(f, nullptr, p);
}

FEPackage* FirstActivePackage() {
  auto ui = Var<uint8_t*>(addr::pFEng);
  if (!ui) return nullptr;
  auto feng = *reinterpret_cast<uint8_t**>(ui + 8);
  return feng ? *reinterpret_cast<FEPackage**>(feng + 0xE4) : nullptr;
}
}  // namespace u2::feng

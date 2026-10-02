#pragma once
// Thin wrappers over the game's FEng (FrontEnd engine) functions. All cdecl in v1.2.
// See docs/kb/feng-ui.md for the object model.
#include <cstdint>

namespace u2::feng {

struct FEObject;   // +0x04 next, +0x10 name hash, +0x18 type (5 = group), +0x1C flags, +0x2C data, group +0x64 first child
struct FEPackage;  // +0x04 next, +0x0C const char* name, +0x10 name hash, +0x24 port mask, +0x4C object list head

uint32_t HashUpper(const char* s);    // FEHashUpper 0x505450: object/package names

FEPackage* FindPackage(const char* pkg);                // 0x52CEF0 (searches active + idle lists)
FEObject* FindObject(const char* pkg, uint32_t hash);   // 0x5379C0
void SetInvisible(FEObject* o);                         // 0x50CA00
void Print(const char* pkg, uint32_t hash, const char* text);  // FEPrintf 0x537B80 with "%s"

// Active package list: cFEng = *(pUIData+8); head at cFEng+0xE4, next at pkg+0x04.
FEPackage* FirstActivePackage();
inline FEPackage* NextPackage(FEPackage* p) { return *reinterpret_cast<FEPackage**>(reinterpret_cast<uint8_t*>(p) + 0x04); }
inline const char* PackageName(FEPackage* p) { return *reinterpret_cast<const char**>(reinterpret_cast<uint8_t*>(p) + 0x0C); }
inline void*& PackageObjects(FEPackage* p) { return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(p) + 0x4C); }

}  // namespace u2::feng

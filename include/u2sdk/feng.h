#pragma once
// Thin wrappers over the game's FEng (FrontEnd engine) functions. All cdecl in v1.2.
// See docs/kb/feng-ui.md for the object model.
#include <cstdint>

namespace u2::feng {

struct FEObject;   // +0x10 name hash, +0x18 type (2 = string), +0x1C flags (bit0 hidden), +0x2C data
struct FEPackage;  // +0x04 next, +0x0C const char* name, +0x10 name hash, +0x4C object list head

uint32_t HashUpper(const char* s);    // FEHashUpper 0x505450: object/package names
uint32_t StringHash(const char* s);   // bStringHash 0x43DB50: language/texture hashes

FEPackage* FindPackage(const char* pkg);                // 0x52CEF0 (searches active + idle lists)
FEObject* FindObject(const char* pkg, uint32_t hash);   // 0x5379C0
FEObject* FindString(const char* pkg, uint32_t hash);   // 0x537A10 (type 2 only)
void SetVisible(FEObject* o);                           // 0x50CA50
void SetInvisible(FEObject* o);                         // 0x50CA00
void Print(const char* pkg, uint32_t hash, const char* text);  // FEPrintf 0x537B80 with "%s"
void SetCenter(FEObject* o, float x, float y);          // 0x51D4D0, 640x480 canvas, origin centre
void GetCenter(FEObject* o, float* x, float* y);        // 0x51D620
void SetColor(FEObject* o, uint32_t argb);              // 0x51D7F0
void ShowPackage(const char* pkg);                      // AddFngToUIObject_1 0x555E80 (pkg, 0)
void RemovePackage(const char* pkg);                    // RemoveFNGFromUIObject 0x5379A0
// Like ShowPackage, but with controlMask 0 so the package never takes gameplay/pause input.
// UIData::AddFNGToUIObject 0x555D00(this, flags 0, pkg, parent 0, mask 0, data 0).
// (Calling cFEng::PushPackage 0x555860 directly crashes.) Remove with RemovePackage.
void PushOverlayPackage(const char* pkg);

// Active package list: cFEng = *(pUIData+8); head at cFEng+0xE4, next at pkg+0x04.
FEPackage* FirstActivePackage();
inline FEPackage* NextPackage(FEPackage* p) { return *reinterpret_cast<FEPackage**>(reinterpret_cast<uint8_t*>(p) + 0x04); }
inline const char* PackageName(FEPackage* p) { return *reinterpret_cast<const char**>(reinterpret_cast<uint8_t*>(p) + 0x0C); }
inline void*& PackageObjects(FEPackage* p) { return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(p) + 0x4C); }
inline uint32_t& ObjectFlags(FEObject* o) { return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(o) + 0x1C); }

}  // namespace u2::feng

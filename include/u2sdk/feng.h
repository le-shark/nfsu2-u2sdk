#pragma once
// Thin wrappers over the game's FEng (FrontEnd engine) functions. All cdecl in v1.2.
// See docs/kb/feng-ui.md for the object model.
#include <cstdint>

namespace u2::feng {

struct FEObject;   // +0x04 next, +0x10 name hash, +0x18 type (5 = group), +0x1C flags, +0x2C data, group +0x64 first child
struct FEPackage;  // +0x04 next, +0x0C const char* name, +0x10 name hash, +0x1C..+0x28 control masks (+0x24 port
                   // mask), +0x4C object list head

uint32_t HashUpper(const char* s);    // FEHashUpper 0x505450: object/package names

FEPackage* FindPackage(const char* pkg);                // 0x52CEF0 (searches active + idle lists)
FEObject* FindObject(const char* pkg, uint32_t hash);   // 0x5379C0
void SetInvisible(FEObject* o);                         // 0x50CA00
void Print(const char* pkg, uint32_t hash, const char* text);  // FEPrintf 0x537B80 with "%s"
// FEngSetControlMask 0x537950 (ExOpts InGameFunctions.h:38): finds the package by name (0x52CEF0) and writes the mask into
// its four fields +0x1C..+0x28 (0x537965..0x53796E). The game turns a screen's input off with mask 0 before it pushes a
// screen over it (0x4E4CA4..0x4E4CBF: its own package 0, then the new one with 0xFF; 0x5643D7..0x5643FE), and back on
// with the player's port mask when it returns (0x4EE1FE). The FE skips a package with port mask 0 for pads and keys
// (ProcessPadsForPackage 0x54D4BC) and the mouse (0x54E029), and its input messages with control mask 0 (0x55B602).
void SetControlMask(const char* pkg, uint32_t mask);
// Every active package's objects drawn anew from their current state next frame: 0x50C7E0 (thiscall pUIData [0x8384C4])
// walks them with visitor 0x79ABF0, whose 0x537630 sets flag 0x400000 and frees the object's drawn copy (+0x58). The draw
// 0x5370A0 otherwise reuses that copy unless flag 0x2000000 is set (0x53710B), and FEngine::Update clears the change
// flags once a frame was rendered (cFEng+0x5250, set at 0x5367B5 by every cFEng::Render). The game calls it when a pad
// comes or goes (0x56633F) and after the language reload (0x512459).
void RedrawAll();
inline uint32_t* PackageMasks(FEPackage* p) { return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x1C); }

// Active package list: cFEng = *(pUIData+8); head at cFEng+0xE4, next at pkg+0x04.
FEPackage* FirstActivePackage();
inline FEPackage* NextPackage(FEPackage* p) { return *reinterpret_cast<FEPackage**>(reinterpret_cast<uint8_t*>(p) + 0x04); }
inline const char* PackageName(FEPackage* p) { return *reinterpret_cast<const char**>(reinterpret_cast<uint8_t*>(p) + 0x0C); }
inline void*& PackageObjects(FEPackage* p) { return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(p) + 0x4C); }

}  // namespace u2::feng

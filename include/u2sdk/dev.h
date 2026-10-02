#pragma once
// Debug screenshots of the displayed frame (incl. HUD/FE) and PNG writing.
#include <string>

struct IDirect3DDevice9;

namespace u2::dev {
void RequestShot(const std::string& path, int scale);  // saved at the next OnEndScene, at 1/scale size (1..4)
void OnEndScene(IDirect3DDevice9* dev);     // call from the main frame's EndScene (before original)
bool SavePng(const std::string& path, int w, int h, const unsigned char* rgb);
}  // namespace u2::dev

#pragma once
// Developer tooling for automated in-game testing (tools/drive/u2drive.py).
//
// Command file: <scripts>\<ModuleName>.cmd. Each line is "<command> [args]". The file is read
// and deleted on the next frame. Built-in commands:
//   shot <scale> <path>   save the next displayed frame (incl. HUD/FE) as PNG at 1/scale size (1..4)
//   log <text>            write a marker line to the log
// Scripts can register more commands.
#include <functional>
#include <string>

struct IDirect3DDevice9;

namespace u2::dev {
using Handler = std::function<void(const std::string& args)>;
void Register(const std::string& command, Handler h);
void Poll();                                // call once per frame (game thread)
void RequestShot(const std::string& path, int scale);  // saved at the next OnEndScene
void OnEndScene(IDirect3DDevice9* dev);     // call from the main frame's EndScene (before original)
bool SavePng(const std::string& path, int w, int h, const unsigned char* rgb);  // utility
}  // namespace u2::dev

#pragma once
// Typed access to NFSU2 v1.2 (SPEED2.EXE, entry point 0x75BCC7) game state.
// Every address here is documented with its source / confidence in docs/kb/.
#include <cstdint>

#include "u2sdk/u2math.h"

struct IDirect3DDevice9;
typedef struct HWND__* HWND;

namespace u2 {

namespace addr {
// --- version
constexpr uintptr_t kEntryPoint = 0x75BCC7;
// --- globals
constexpr uintptr_t GameFlowState = 0x8654A4;        // int: 3 FE, 4/5 loading, 6 in race / free roam
constexpr uintptr_t pRaceCoordinator = 0x8900D8;     // *(int*)p == 6 -> game paused
constexpr uintptr_t PCHelpBarUp = 0x8384DC;          // byte: PC help bar object exists (set 0x55229C,
                                                     // cleared 0x552221); up with pause/options and dialogs
constexpr uintptr_t GameSpeed = 0x7A5730;            // float Tweak_GameSpeed (1.0)
constexpr uintptr_t hWnd = 0x870990;
constexpr uintptr_t IsLostFocus = 0x8709E0;          // bool
constexpr uintptr_t D3DDevice = 0x870974;            // IDirect3DDevice9*
constexpr uintptr_t DrawHud = 0x7F91C4;              // int, gates *all* FEng rendering
constexpr uintptr_t pFEng = 0x8384C4;                // cFEng*/UIData* instance
constexpr uintptr_t FECursorVisible = 0x7FA29C;      // byte, FE cursor drawn (read at 0x5378FD)
constexpr uintptr_t FECanvasScaleX = 0x79AC10;       // float 2/640; WidescreenFix hor+ rewrites it
constexpr uintptr_t MouseWheel = 0x8763EC;           // int DIMOUSESTATE.lZ of this frame
constexpr uintptr_t CarSlots = 0x89CCF8;             // Car*[22]; slot 0 = player
constexpr uintptr_t Network = 0x89CF48;              // nfsu2-re vars.txt: network89CF48
constexpr uintptr_t pWorld = 0x890080;               // World*, null outside the world (see RacerCars)
constexpr uintptr_t g_MotionBlurEnable = 0x870CDC;   // int
constexpr uintptr_t g_DepthOfFieldEnable = 0x870CF0; // int
constexpr uintptr_t CameraStopUpdating = 0x828640;   // int
constexpr uintptr_t PlayerCamera = 0x8760E0;         // Camera (eView 1 and 4)
constexpr uintptr_t FrameLimit = 0x865558;           // float s: busy-wait limit and lower dt clamp in 0x57EAD0
                                                     // (0x57EB0A); WSF FPSLimit writes 1/refresh. dt = ticks/1800,
                                                     // so never below 1/1800 (fdiv [0x865198] at 0x44345F)
// RT[0] of the RT table 0x86E768 (eRenderTarget, stride 0x1C; view 1): +8 surface, +0xC depth,
// +0x14 width, +0x18 height. Rewritten from the screen size every frame by 0x5C21E0 (0x5C2341).
constexpr uintptr_t RT0Surface = 0x86E770;
constexpr uintptr_t RT0Depth = 0x86E774;
constexpr uintptr_t RT0Width = 0x86E77C;
constexpr uintptr_t RT0Height = 0x86E780;
constexpr uintptr_t BackBufferSurface = 0x870850;    // GetBackBuffer(0,0) (0x5BA785); post FX use it directly
constexpr uintptr_t AutoDepthSurface = 0x870854;     // GetDepthStencilSurface (0x5BA7A7)
constexpr uintptr_t LightStreaksEnable = 0x870CE0;   // int g_LightStreaksEnable; with 0x870CE4 gates the
                                                     // screen-size glow/streak pass 0x5CBB00 (0x5CB565/0x5CB577)
                                                     // and overbright 0x8707CE (0x5CB768)
constexpr uintptr_t HorizonFogEnable = 0x870D0C;     // int g_HorizonFogEnable; 0x8707CF read per frame (0x5CB79B)
constexpr uintptr_t BleachEnable = 0x870D00;         // int g_BleachByPassEnable (Enhanced Contrast)
constexpr uintptr_t RainEnable = 0x870D10;           // int g_RainEnable; rain splatter 0x5C7AB0 gate (0x5CB7E6).
                                                     // Only ever turn it off: it also gates RT creation (0x5BB875)
// Light streak / overbright chain (verified-in-binary.md "Light streaks, glow and overbright chain"):
constexpr uintptr_t StreakTexture = 0x86F2E4;        // IDirect3DTexture9*, W x H feedback buffer (0x5BBA18)
constexpr uintptr_t StreakSurface = 0x86FF70;        // its level 0 (0x5BBA31); bound by the streak pass 0x5CBB00
constexpr uintptr_t FullTexture0 = 0x86EB88;         // full-size ping-pong textures [0], [1] (0x5BB90A), used by
constexpr uintptr_t FullTexture1 = 0x86EB8C;         // bleach (0x5C6AA4) and the overbright composite (0x5CCBFA),
constexpr uintptr_t FullSurface0 = 0x86EB0C;         // which copy them back over [0x870850]; surfaces [0], [1]
constexpr uintptr_t FullSurface1 = 0x86EB10;
constexpr uintptr_t FsaaLevel = 0x870D08;            // int g_FSAALevel; the streak pass uses the MSAA RT when > 0
                                                     // (0x5CBB1C, 0x5CBB96, 0x5CBCAC, 0x5C6C0F)
constexpr uintptr_t MirrorOverride = 0x8707E4;       // int: 0x5C21E0 uses it instead of 0x8707DC when non-zero;
                                                     // 1 = rear-view mirror view 3 off. No game code writes it
// --- functions
constexpr uintptr_t RenderFrame = 0x5D2850;             // cdecl(), called from GameLoop 0x5814D6
constexpr uintptr_t Camera_SetCameraMatrix = 0x447280;  // thiscall(Camera*, bMatrix4*, float), ret 8
constexpr uintptr_t CubicCameraMover_Update = 0x453570; // thiscall(mover), ret
constexpr uintptr_t eCreateLookAtMatrix = 0x5BA9A0;     // cdecl(bMatrix4*, bVector4* from, to, up)
constexpr uintptr_t eViewPlatInfo_Update = 0x5C7EC0;    // builds view/proj for a view
constexpr uintptr_t cFEng_Render = 0x536520;            // thiscall(cFEng*)
constexpr uintptr_t GameResetDevice = 0x5D1C40;         // cdecl(); WidescreenFix also hooks it
constexpr uintptr_t Render3D = 0x5CAAD0;                // cdecl(); only caller 0x5D2B55. Ends with RT0 = backbuffer,
                                                        // flushed, all game post FX done, no FE yet (0x5CBA1C..)
}  // namespace addr

// bVector3/bVector4 are 16-byte aligned 4-float vectors in the game.
struct bVector4 { float x, y, z, w; };
struct bMatrix4 { float m[4][4]; };

struct CameraParams {          // Camera::CurrentKey, 0xD0 bytes
  bMatrix4 Matrix;             // 0x00 world -> view
  bVector4 Position;           // 0x40
  bVector4 Direction;          // 0x50
  bVector4 Target;             // 0x60
  bVector4 Noise[4];           // 0x70
  float TargetDistance;        // 0xB0
  float FocalDistance;         // 0xB4 native DOF focus (MW layout, consumed at 0x5D2995)
  float DepthOfField;          // 0xB8 native DOF amount
  float NearZ;                 // 0xBC
  float FarZ;                  // 0xC0
  uint16_t FieldOfView;        // 0xC4 binary angle
  uint16_t pad_c6;
  uint8_t pad_c8[8];
};
static_assert(sizeof(CameraParams) == 0xD0);

struct eView {
  void* PlatInfo;              // 0x00 eViewPlatInfo*: +0x00 View, +0x40 Proj, +0x80 ViewProj
  int Id;                      // 0x04
  uint8_t Active;              // 0x08
  uint8_t pad[0x37];
  CameraParams* Camera;        // 0x40
  uint8_t pad2[0x2C];
};
static_assert(sizeof(eView) == 0x70);

// --- queries
bool IsSupportedExe();                 // checks PE entry point == 0x75BCC7
int GameFlowState();
bool IsInGame();                       // flow state 6
bool IsGamePaused();                   // help bar up (pause/options, dialogs) or pause state;
                                       // the same test as WidescreenFix ComVars.ixx
bool IsNetworkSession();               // GameLoop never pauses then (0x581306 -> 0x5F00F0)
HWND Window();
bool WindowHasFocus();
IDirect3DDevice9* Device();
CameraParams* PlayerCamera();
bool IsPauseMenuState();               // RaceCoordinator state 6 (GameLoop 0x581319)

// Player and AI racer cars. World (0x890080) +0x10 car count, +0x14 player count, +0x18 player +
// racer count, +0x1C Car*[] sorted players, racers, traffic (World::RebuildCarArray 0x5F0D20; the
// game walks it at 0x5F14FC; NFSU2FastTravel FromGame.h). Cars live for the whole race/free roam.
// Car: +0x14 DriverInfo* (+4 type), +0x24 driver class, +0x28 movement mode (0 = none:
// parked/inactive), +0x50 position, +0x90 orientation rows (row 0 forward, 1 left, 2 up).
struct RacerCar {
  uint8_t* car;
  int racer;  // 0 = player, else 1.. by World array slot (stable while other racers drop out)
};
constexpr int kMaxCars = 22;           // size of the Car* slot table at 0x89CCF8
int RacerCars(RacerCar* out, int max); // active ones only (the player is always included)
void LogCars();                        // whole World car array, for diagnostics
// Every car in the World array that is in play (movement mode != 0), player included; traffic too.
int ActiveCars(uint8_t** out, int max);

// Nearest hit of the ray from + dir * t (dir unit length, t <= maxLen) on the static world's
// collision polygons (track, walls, invisible barriers; not cars). Only sections whose collision
// pack is streamed in are seen. Main thread only. See game.cpp for the engine calls.
bool WorldRayHit(const Vec3& from, const Vec3& dir, float maxLen, float* dist);
uint8_t* PlayerCar();                  // null if there is none
inline Vec3 CarPosition(const uint8_t* car) {
  auto p = reinterpret_cast<const float*>(car + 0x50);
  return {p[0], p[1], p[2]};
}
inline void CarAxes(const uint8_t* car, Vec3 out[3]) {
  auto m = reinterpret_cast<const float*>(car + 0x90);
  for (int r = 0; r < 3; ++r) out[r] = {m[r * 4], m[r * 4 + 1], m[r * 4 + 2]};
}

template <typename T>
inline T& Var(uintptr_t a) { return *reinterpret_cast<T*>(a); }

// --- engine calls
void CreateLookAtMatrix(bMatrix4* out, const Vec3& from, const Vec3& to, const Vec3& up);

inline Vec3 ToVec3(const bVector4& v) { return {v.x, v.y, v.z}; }
}  // namespace u2

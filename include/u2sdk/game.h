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
// --- functions
constexpr uintptr_t RenderFrame = 0x5D2850;             // cdecl(), called from GameLoop 0x5814D6
constexpr uintptr_t Camera_SetCameraMatrix = 0x447280;  // thiscall(Camera*, bMatrix4*, float), ret 8
constexpr uintptr_t CubicCameraMover_Update = 0x453570; // thiscall(mover), ret
constexpr uintptr_t eCreateLookAtMatrix = 0x5BA9A0;     // cdecl(bMatrix4*, bVector4* from, to, up)
constexpr uintptr_t eViewPlatInfo_Update = 0x5C7EC0;    // builds view/proj for a view
constexpr uintptr_t cFEng_Render = 0x536520;            // thiscall(cFEng*)
constexpr uintptr_t GameResetDevice = 0x5D1C40;         // cdecl(); WidescreenFix also hooks it
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

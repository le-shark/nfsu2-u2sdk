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
constexpr uintptr_t PauseFEActive = 0x8384DC;        // byte, non-zero while pause/options FE is up
constexpr uintptr_t GameSpeed = 0x7A5730;            // float Tweak_GameSpeed (1.0)
constexpr uintptr_t FrameDt = 0x865198;              // float real frame dt
constexpr uintptr_t hWnd = 0x870990;
constexpr uintptr_t IsLostFocus = 0x8709E0;          // bool
constexpr uintptr_t D3DDevice = 0x870974;            // IDirect3DDevice9*
constexpr uintptr_t ScreenWidth = 0x870980;
constexpr uintptr_t ScreenHeight = 0x870984;
constexpr uintptr_t DrawHud = 0x7F91C4;              // int, gates *all* FEng rendering
constexpr uintptr_t pFEng = 0x8384C4;                // cFEng*/UIData* instance
constexpr uintptr_t MouseWheel = 0x8763EC;           // int DIMOUSESTATE.lZ of this frame
constexpr uintptr_t PlayerCarState = 0x89CCF8;       // CarState*: +0x50 position
constexpr uintptr_t g_MotionBlurEnable = 0x870CDC;   // int
constexpr uintptr_t g_DepthOfFieldEnable = 0x870CF0; // int
constexpr uintptr_t g_FSAALevel = 0x870D08;          // int
constexpr uintptr_t DofActiveThisFrame = 0x8707CC;   // byte
constexpr uintptr_t CameraStopUpdating = 0x828640;   // int
constexpr uintptr_t PlayerCamera = 0x8760E0;         // Camera (eView 1 and 4)
constexpr uintptr_t Views = 0x832DE0;                // eView[22], stride 0x70
// --- functions
constexpr uintptr_t Camera_SetCameraMatrix = 0x447280;  // thiscall(Camera*, bMatrix4*, float), ret 8
constexpr uintptr_t eCreateLookAtMatrix = 0x5BA9A0;     // cdecl(bMatrix4*, bVector3* from, to, up)
constexpr uintptr_t eViewPlatInfo_Update = 0x5C7EC0;    // builds view/proj for a view
constexpr uintptr_t cFEng_Render = 0x536520;            // thiscall(cFEng*)
constexpr uintptr_t PostFxInsertPoint = 0x5CC0CA;       // after 3D + native post FX, before HUD
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
bool IsGamePaused();                   // game's own pause (menu) is active
HWND Window();
bool WindowHasFocus();
IDirect3DDevice9* Device();
CameraParams* PlayerCamera();
eView* GetView(int id);
bool PlayerCarPosition(Vec3* out);     // false if no car

template <typename T>
inline T& Var(uintptr_t a) { return *reinterpret_cast<T*>(a); }

// --- engine calls
void CreateLookAtMatrix(bMatrix4* out, const Vec3& from, const Vec3& to, const Vec3& up);

inline Mat4& AsMat4(bMatrix4& m) { return reinterpret_cast<Mat4&>(m); }
inline Vec3 ToVec3(const bVector4& v) { return {v.x, v.y, v.z}; }
}  // namespace u2

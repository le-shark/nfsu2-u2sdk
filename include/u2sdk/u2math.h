#pragma once
// Pure math shared by scripts and unit tests (no game / Windows dependencies).
// Conventions match the game's D3D9 renderer: row vectors, v' = v * M, matrices row-major,
// left-handed view space (x right, y up, z forward), projection with P[2][3] = 1.
#include <cmath>
#include <cstdint>

namespace u2 {

constexpr float kPi = 3.14159265358979f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};
inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
inline Vec3 Normalize(const Vec3& v) {
  float l = Length(v);
  return l > 1e-8f ? v * (1.0f / l) : Vec3{0, 0, 0};
}
// Rotate v around unit axis k by angle a (Rodrigues).
inline Vec3 RotateAxis(const Vec3& v, const Vec3& k, float a) {
  float c = std::cos(a), s = std::sin(a);
  return v * c + Cross(k, v) * s + k * (Dot(k, v) * (1 - c));
}

struct Vec4 { float x = 0, y = 0, z = 0, w = 0; };

struct Mat4 {
  float m[4][4];
  static Mat4 Identity() {
    Mat4 r{};
    for (int i = 0; i < 4; ++i) r.m[i][i] = 1;
    return r;
  }
};
inline Mat4 Mul(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
  return r;
}
inline Vec4 Transform(const Vec4& v, const Mat4& m) {
  return {v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0],
          v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1],
          v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2],
          v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3]};
}

// ---- game FOV <-> photographic focal length ---------------------------------------------
// Camera+0xC4 is a u16 binary angle (65536 = 360 deg). In the projection builder (0x5C7EC0)
// the vertical view angle of the player view is FOV * (h/w) * (1.1/0.8696); at 4:3 that is
// FOV * 0.94875. With WidescreenFix (Hor+) the vertical angle stays at the 4:3 value.
// `vertScale` is that factor; it's an INI option (FovCalibration) so it can be tuned in-game.
constexpr float kDefaultVertFovScale = 0.75f * 1.1f / 0.8695652f;  // 0.94875
constexpr float kSensorHeightMm = 24.0f;                            // 35 mm full frame

inline float FovU16ToDeg(uint16_t f) { return f * (360.0f / 65536.0f); }
inline uint16_t DegToFovU16(float deg) {
  float v = deg * (65536.0f / 360.0f) + 0.5f;
  return static_cast<uint16_t>(v < 1 ? 1 : (v > 65535 ? 65535 : v));
}
inline float VerticalFovDegFromFocal(float mm) { return 2.0f * std::atan(kSensorHeightMm * 0.5f / mm) * kRadToDeg; }
inline float FocalFromVerticalFovDeg(float deg) { return kSensorHeightMm * 0.5f / std::tan(deg * 0.5f * kDegToRad); }
inline uint16_t FocalToGameFov(float mm, float vertScale = kDefaultVertFovScale) {
  return DegToFovU16(VerticalFovDegFromFocal(mm) / vertScale);
}
inline float GameFovToFocal(uint16_t f, float vertScale = kDefaultVertFovScale) {
  return FocalFromVerticalFovDeg(FovU16ToDeg(f) * vertScale);
}

// ---- sampling -------------------------------------------------------------------------------
inline float Halton(uint32_t i, uint32_t base) {
  float f = 1, r = 0;
  while (i) {
    f /= static_cast<float>(base);
    r += f * static_cast<float>(i % base);
    i /= base;
  }
  return r;
}
// Point i of n on the unit disc, evenly spread (Vogel / golden-angle spiral).
inline void GoldenDisc(uint32_t i, uint32_t n, float* x, float* y) {
  float r = std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(n));
  float t = static_cast<float>(i) * 2.39996323f;
  *x = r * std::cos(t);
  *y = r * std::sin(t);
}

// ---- projection adjustments for accumulation capture ----------------------------------------
struct ProjAdjust {
  // Thin-lens: eye shifted by (lensX, lensY) metres in view space; focus plane at focusDist.
  float lensX = 0, lensY = 0, focusDist = 10;
  // Sub-pixel jitter in NDC units of the *final* image (applied before tiling).
  float jitterX = 0, jitterY = 0;
  // Tiling: image split into tiles x tiles; this frame renders tile (tx, ty), ty = 0 is the top row.
  int tiles = 1, tx = 0, ty = 0;
};

// View matrix: translate the eye by (lensX, lensY) in view space (v_view' = v_view - lens).
inline Mat4 AdjustView(const Mat4& view, const ProjAdjust& a) {
  Mat4 r = view;
  r.m[3][0] -= a.lensX;
  r.m[3][1] -= a.lensY;
  return r;
}

// Projection: shear so the focus plane stays fixed for the shifted eye, add jitter, then zoom
// into the requested tile. Works for any standard perspective P with P[2][3] = 1.
inline Mat4 AdjustProj(const Mat4& proj, const ProjAdjust& a) {
  Mat4 p = proj;
  p.m[2][0] += p.m[0][0] * a.lensX / a.focusDist + a.jitterX;
  p.m[2][1] += p.m[1][1] * a.lensY / a.focusDist + a.jitterY;
  if (a.tiles > 1) {
    float n = static_cast<float>(a.tiles);
    float cx = -1.0f + (2.0f * a.tx + 1.0f) / n;
    float cy = 1.0f - (2.0f * a.ty + 1.0f) / n;
    for (int r = 0; r < 4; ++r) {
      p.m[r][0] = n * (p.m[r][0] - cx * p.m[r][3]);
      p.m[r][1] = n * (p.m[r][1] - cy * p.m[r][3]);
    }
  }
  return p;
}

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace u2

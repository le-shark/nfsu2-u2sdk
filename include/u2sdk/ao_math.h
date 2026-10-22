#pragma once
// View 1's camera as camera-relative rays: Daylight's screen-space AO (docs/specs/2026-10-10-ssao-design.md) and
// photo mode's focus probe. The per-frame camera constants, pure code for the unit tests.
// Everything is camera-relative world space (world minus the eye): no view-matrix axis conventions, small numbers
// for fp32 in the shader. The source is view 1's ViewProj (eViewPlatInfo +0x80, row-vector D3D form, built by
// 0x5C7EC0 every frame; WidescreenFix's FixFOV is already in it) and the camera position.
#include <cmath>

namespace u2::aomath {

// Row-vector 4x4 product a * b (D3D form), in double.
inline void Mul(const double a[16], const double b[16], double o[16]) {
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      o[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
}

// General 4x4 inverse (cofactors); false if singular.
inline bool Inverse(const double m[16], double o[16]) {
  double inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (std::fabs(det) < 1e-300) return false;
  for (int i = 0; i < 16; ++i) o[i] = inv[i] / det;
  return true;
}

// T(eye): translation row = eye, so (rel, 1) * T = (rel + eye, 1).
inline void Translation(const double eye[3], double o[16]) {
  for (int i = 0; i < 16; ++i) o[i] = (i % 5 == 0) ? 1 : 0;
  o[12] = eye[0], o[13] = eye[1], o[14] = eye[2];
}

struct Frame {
  float vpRel[16];   // camera-relative world -> clip (row-vector); clip.w = view depth (P23 = 1)
  float rayTL[3];    // camera-relative world position at view depth 1 for the screen's top-left corner (u, v = 0)
  float rayDU[3];    // its change from u = 0 to u = 1 (left -> right)
  float rayDV[3];    // and from v = 0 to v = 1 (top -> bottom)
};

// The ray through screen point (u, v) at view depth 1: (u, v) -> ndc (2u - 1, 1 - 2v), unprojected at the far plane
// with inv(VPrel), divided by its view depth (clip w). Rays are linear in u and v at a fixed view depth.
inline bool RayAt(const double inv[16], const double vp[16], double u, double v, double out[3]) {
  double x = 2 * u - 1, y = 1 - 2 * v;
  double q[4];
  for (int c = 0; c < 4; ++c) q[c] = x * inv[c] + y * inv[4 + c] + 1.0 * inv[8 + c] + inv[12 + c];
  if (std::fabs(q[3]) < 1e-30) return false;
  double p[3] = {q[0] / q[3], q[1] / q[3], q[2] / q[3]};
  double w = p[0] * vp[3] + p[1] * vp[7] + p[2] * vp[11] + vp[15];  // view depth of p
  if (std::fabs(w) < 1e-30) return false;
  for (int i = 0; i < 3; ++i) out[i] = p[i] / w;
  return true;
}

inline bool BuildFrame(const float viewProj[16], const float eyeF[3], Frame* f) {
  double vp[16], t[16], rel[16], inv[16], eye[3] = {eyeF[0], eyeF[1], eyeF[2]};
  for (int i = 0; i < 16; ++i) vp[i] = viewProj[i];
  Translation(eye, t);
  Mul(t, vp, rel);
  if (!Inverse(rel, inv)) return false;
  double tl[3], tr[3], bl[3];
  if (!RayAt(inv, rel, 0, 0, tl) || !RayAt(inv, rel, 1, 0, tr) || !RayAt(inv, rel, 0, 1, bl)) return false;
  for (int i = 0; i < 16; ++i) f->vpRel[i] = static_cast<float>(rel[i]);
  for (int i = 0; i < 3; ++i) {
    f->rayTL[i] = static_cast<float>(tl[i]);
    f->rayDU[i] = static_cast<float>(tr[i] - tl[i]);
    f->rayDV[i] = static_cast<float>(bl[i] - tl[i]);
  }
  return true;
}

// world -> map (shadowmap::ShaderMatrix) made camera-relative: T(eye) * S.
inline void RelativeMap(const float s[16], const float eyeF[3], float out[16]) {
  double sd[16], t[16], o[16], eye[3] = {eyeF[0], eyeF[1], eyeF[2]};
  for (int i = 0; i < 16; ++i) sd[i] = s[i];
  Translation(eye, t);
  Mul(t, sd, o);
  for (int i = 0; i < 16; ++i) out[i] = static_cast<float>(o[i]);
}

// The ambient-only darkening (design "Sky light only"): the sky term is multiplied by ao, the sun term is not.
// skyL / sunL: luminance of the two lights on this pixel (sunL already x sun visibility x N.L).
// inSun 0..1: how much plain darkening (ao on everything) is mixed in (tuner "In sunlight").
inline float Factor(float ao, float skyL, float sunL, float inSun) {
  float total = skyL + sunL;
  float ambientOnly = total > 1e-6f ? (skyL * ao + sunL) / total : ao;
  return ambientOnly + (ao - ambientOnly) * inSun;
}

}  // namespace u2::aomath

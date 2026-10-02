#include "test.h"
#include "u2sdk/u2math.h"

using namespace u2;

namespace {
// D3D-style LH perspective with the game's layout (P[2][3] = 1), negative P11 like the game's.
Mat4 Perspective(float hfovDeg, float aspectHW, float n, float f) {
  Mat4 p{};
  float sx = 1.0f / std::tan(hfovDeg * 0.5f * kDegToRad);
  p.m[0][0] = sx;
  p.m[1][1] = -sx / aspectHW;
  p.m[2][2] = f / (f - n);
  p.m[2][3] = 1;
  p.m[3][2] = -n * f / (f - n);
  return p;
}
void Ndc(const Vec4& v, const Mat4& view, const Mat4& proj, float* x, float* y) {
  Vec4 c = Transform(Transform(v, view), proj);
  *x = c.x / c.w;
  *y = c.y / c.w;
}
Mat4 SomeView() {
  // Rotation + translation (orthonormal), row-vector convention.
  Mat4 v = Mat4::Identity();
  float a = 0.3f, c = std::cos(a), s = std::sin(a);
  v.m[0][0] = c;  v.m[0][2] = s;
  v.m[2][0] = -s; v.m[2][2] = c;
  v.m[3][0] = 1.5f; v.m[3][1] = -0.4f; v.m[3][2] = 2.0f;
  return v;
}
}  // namespace

TEST(FovU16RoundTrip) {
  CHECK_NEAR(FovU16ToDeg(DegToFovU16(55.0f)), 55.0, 0.01);
  CHECK_NEAR(FovU16ToDeg(16384), 90.0, 1e-4);
}

TEST(FocalLengthMapping) {
  // 50 mm full frame -> ~27 deg vertical FOV
  CHECK_NEAR(VerticalFovDegFromFocal(50), 26.99, 0.05);
  CHECK_NEAR(FocalFromVerticalFovDeg(VerticalFovDegFromFocal(85)), 85, 1e-3);
  for (float mm : {10.f, 24.f, 35.f, 50.f, 135.f, 300.f})
    CHECK_NEAR(GameFovToFocal(FocalToGameFov(mm)), mm, mm * 0.01);
  CHECK(FocalToGameFov(24) > FocalToGameFov(200));  // wider lens -> bigger FOV value
}

TEST(HaltonAndDisc) {
  CHECK_NEAR(Halton(1, 2), 0.5, 1e-6);
  CHECK_NEAR(Halton(2, 2), 0.25, 1e-6);
  CHECK_NEAR(Halton(1, 3), 1.0 / 3, 1e-6);
  double sx = 0, sy = 0;
  const int n = 256;
  for (int i = 0; i < n; ++i) {
    float x, y;
    GoldenDisc(i, n, &x, &y);
    CHECK(x * x + y * y <= 1.0001f);
    sx += x;
    sy += y;
  }
  CHECK_NEAR(sx / n, 0, 0.02);
  CHECK_NEAR(sy / n, 0, 0.02);
}

TEST(LensShearKeepsFocusPlaneFixed) {
  Mat4 view = SomeView(), proj = Perspective(70, 0.5625f, 0.5f, 15000);
  ProjAdjust a;
  a.lensX = 0.02f;
  a.lensY = -0.015f;
  a.focusDist = 12;
  Mat4 v2 = AdjustView(view, a), p2 = AdjustProj(proj, a);
  // A world point that lands exactly on the focus plane (view z = 12).
  // Map a view-space point back to world space (view is an orthonormal rotation + translation).
  float vx = 1.3f, vy = -0.7f, vz = 12;
  // inverse of orthonormal rotation + translation
  float tx = vx - view.m[3][0], ty = vy - view.m[3][1], tz = vz - view.m[3][2];
  Vec4 world{tx * view.m[0][0] + ty * view.m[0][1] + tz * view.m[0][2],
             tx * view.m[1][0] + ty * view.m[1][1] + tz * view.m[1][2],
             tx * view.m[2][0] + ty * view.m[2][1] + tz * view.m[2][2], 1};
  Vec4 chk = Transform(world, view);
  CHECK_NEAR(chk.z, 12, 1e-3);
  float x0, y0, x1, y1;
  Ndc(world, view, proj, &x0, &y0);
  Ndc(world, v2, p2, &x1, &y1);
  CHECK_NEAR(x1, x0, 1e-4);
  CHECK_NEAR(y1, y0, 1e-4);
  // A point at a different depth must move (that's the blur).
  Vec4 nearPt{world.x * 0.3f, world.y * 0.3f, world.z * 0.3f, 1};
  Ndc(nearPt, view, proj, &x0, &y0);
  Ndc(nearPt, v2, p2, &x1, &y1);
  CHECK(std::fabs(x1 - x0) > 1e-3f || std::fabs(y1 - y0) > 1e-3f);
}

TEST(JitterShiftsNdc) {
  Mat4 view = Mat4::Identity(), proj = Perspective(60, 0.75f, 1, 1000);
  ProjAdjust a;
  a.jitterX = 0.004f;
  a.jitterY = -0.002f;
  float x0, y0, x1, y1;
  Vec4 p{0.5f, 0.2f, 30, 1};
  Ndc(p, view, proj, &x0, &y0);
  Ndc(p, view, AdjustProj(proj, a), &x1, &y1);
  CHECK_NEAR(x1 - x0, 0.004, 1e-5);
  CHECK_NEAR(y1 - y0, -0.002, 1e-5);
}

TEST(TilesCoverTheImage) {
  Mat4 view = Mat4::Identity(), proj = Perspective(60, 0.75f, 1, 1000);
  const int n = 3;
  for (int ty = 0; ty < n; ++ty)
    for (int tx = 0; tx < n; ++tx) {
      ProjAdjust a;
      a.tiles = n;
      a.tx = tx;
      a.ty = ty;
      Mat4 p2 = AdjustProj(proj, a);
      // Tile centre in full-image NDC must map to the tile's NDC centre (0,0).
      float cx = -1.0f + (2.0f * tx + 1.0f) / n, cy = 1.0f - (2.0f * ty + 1.0f) / n;
      // find a view point that projects to (cx, cy) in the full image at z=50
      float z = 50;
      Vec4 p{cx * z / proj.m[0][0], cy * z / proj.m[1][1], z, 1};
      float x, y;
      Ndc(p, view, proj, &x, &y);
      CHECK_NEAR(x, cx, 1e-4);
      CHECK_NEAR(y, cy, 1e-4);
      Ndc(p, view, p2, &x, &y);
      CHECK_NEAR(x, 0, 1e-4);
      CHECK_NEAR(y, 0, 1e-4);
      // The tile's top-left corner maps to (-1, +1).
      float lx = cx - 1.0f / n, ty_ = cy + 1.0f / n;
      Vec4 q{lx * z / proj.m[0][0], ty_ * z / proj.m[1][1], z, 1};
      Ndc(q, view, p2, &x, &y);
      CHECK_NEAR(x, -1, 1e-4);
      CHECK_NEAR(y, 1, 1e-4);
    }
}

TEST(MulMatchesTransformChain) {
  Mat4 a = SomeView(), b = Perspective(60, 0.75f, 1, 1000);
  Vec4 v{1.5f, -2, 7, 1};
  Vec4 p = Transform(Transform(v, a), b), q = Transform(v, Mul(a, b));
  CHECK_NEAR(p.x, q.x, 1e-3);
  CHECK_NEAR(p.y, q.y, 1e-3);
  CHECK_NEAR(p.z, q.z, 1e-3);
  CHECK_NEAR(p.w, q.w, 1e-3);
}

TEST(IntHelpers) {
  CHECK(u2::Wrap(-1, 4) == 3 && u2::Wrap(4, 4) == 0 && u2::Wrap(9, 4) == 1);
  CHECK(u2::ClampInt(-3, 0, 5) == 0 && u2::ClampInt(7, 0, 5) == 5 && u2::ClampInt(2, 0, 5) == 2);
  CHECK(u2::Length(u2::Normalize(Vec3{0, 0, 0})) == 0);
}

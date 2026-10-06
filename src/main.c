#include <eadk.h>
#include "sim.h"
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumBeam3D";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#define MAXW 208
#define MAXH 146
static int lw = 160, lh = 112, hor = 44; static float foc = 130.f, zmax = 72.f, zsc = 3.4f;
#define LW lw
#define LH lh
#define FOC foc
#define HOR hor
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
enum { S_MENU, S_GARAGE, S_GAME, S_PAUSE, S_DMG, S_SET, S_CTRL, S_KEYS, S_QUICK };
static uint16_t fb[MAXW * MAXH], buf[320 * 16]; static uint8_t zb[MAXW * MAXH], xmap[320];
static int structure, CXc = 80, curZ = -1, showTop = 1, crashI = 2;
static const int CRASHV[5] = {30, 50, 80, 110, 140};
static float camhx = 0, camhz = 1, camx, camy = 5, camz, oHood, oDoor, oTrunk, tHood, tDoor, tTrunk;
static uint64_t pk; static float wspin;

static inline void px(int x, int y, uint16_t c) { if ((unsigned)x < LW && (unsigned)y < LH) { if (curZ >= 0 && zb[y * LW + x] < curZ) return; fb[y * LW + x] = c; } }
static void line(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (int i = 0; i < 300; i++) { px(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
}
static void quad(const float *xs, const float *ys, uint16_t c) {
  float mn = 1e9f, mx = -1e9f;
  for (int i = 0; i < 4; i++) { if (ys[i] < mn) mn = ys[i]; if (ys[i] > mx) mx = ys[i]; }
  for (int y = (int)mn < 0 ? 0 : (int)mn; y <= (int)mx && y < LH; y++) {
    float lo = 1e9f, hi = -1e9f, yy = y + .5f;
    for (int i = 0; i < 4; i++) {
      int j = (i + 1) & 3; float a = ys[i], b = ys[j]; if ((a <= yy) == (b <= yy)) continue;
      float x = xs[i] + (yy - a) / (b - a) * (xs[j] - xs[i]); if (x < lo) lo = x; if (x > hi) hi = x;
    }
    for (int x = (int)lo; x <= (int)hi; x++) px(x, y, c);
  }
}
static void disc(int cx, int cy, int r, uint16_t c) { for (int y = -r; y <= r; y++) { int hw = (int)fsqrt((float)(r * r - y * y)); for (int x = -hw; x <= hw; x++) px(cx + x, cy + y, c); } }
static void terrain(int xl) {
  int top[MAXW]; float rx = camhz, rz = -camhx;
  for (int y = 0; y < LH; y++) { int yy = y * 112 / LH; uint16_t s = C(60 + yy, 120 + yy, 215 + (yy > 39 ? 40 : yy)); for (int x = 0; x < LW; x++) { fb[y * LW + x] = s; zb[y * LW + x] = 255; } }
  for (int i = 0; i < LW; i++) top[i] = LH;
  for (float z = 1.2f; z < zmax; z += .3f + z * .05f) {
    float t = z / zmax, fx = camx + camhx * z, fz = camz + camhz * z; int zq = (int)(z * zsc);
    for (int i = xl; i < LW; i++) {
      float k = (i - CXc + .5f) / FOC * z, X = fx + rx * k, Z = fz + rz * k, h = gh(X, Z);
      int sy = (int)(HOR + (camy - h) * FOC / z); if (sy < 0) sy = 0;
      if (sy >= top[i]) continue;
      float ad = gdist < 0 ? -gdist : gdist; int r, g, b, ck = ((int)(X * .5f + 1000) ^ (int)(Z * .5f + 1000)) & 1, st = (int)((X + Z) * .5f + 1000) & 1;
      if (gfeat > .25f) { if (mapId == 1) { r = 175; g = 170; b = 150; } else { r = st ? 230 : 200; g = st ? 230 : 45; b = g; } }
      else if (mapId == 0 && ad < 6.f) { r = 70; g = 70; b = 76; if (Z > -1.5f && Z < 1.5f && X > TA - 6 && X < TA + 6) r = g = b = (((int)(X * 1.5f + 1000) + (int)(Z * 1.5f + 1000)) & 1) ? 240 : 25; else if (ad > 5.5f) r = g = b = 200; }
      else if (mapId == 0 && ad < 7.4f) { r = st ? 220 : 235; g = st ? 40 : 235; b = g; }
      else if (mapId == 1 && ad < 6.f) { r = 72; g = 72; b = 78; if (ad < .15f && (((int)(Z / 3.f)) & 1)) { r = 230; g = 210; b = 70; } if (ad > 5.6f) r = g = b = 200; }
      else if (mapId == 2 && ad < 12.f) { r = 95 + ck * 6; g = 95 + ck * 6; b = 100 + ck * 6; if (ad > 11.4f) r = g = b = 210; else if (ad < .2f && (((int)(Z / 3.f)) & 1)) r = g = b = 220; }
      else { r = 50 + ck * 8; g = 130 + (int)(h * 5) + ck * 10; b = 45; if (g > 200) g = 200; if (g < 60) g = 60; }
      r += (int)((100 - r) * t); g += (int)((170 - g) * t); b += (int)((225 - b) * t);
      uint16_t col = C(r, g, b);
      for (int y = sy; y < top[i]; y++) { fb[y * LW + i] = col; zb[y * LW + i] = (uint8_t)(zq > 254 ? 254 : zq); }
      top[i] = sy;
    }
  }
}
static int proj(float x, float y, float z, float *sx, float *sy) {
  float dx = x - camx, dz = z - camz, d = dx * camhx + dz * camhz; if (d < .4f) return 0;
  *sx = CXc + (dx * camhz - dz * camhx) / d * FOC; *sy = HOR - (y - camy) / d * FOC; return 1;
}
static void polyn(const float *xs, const float *ys, int m, uint16_t c) {
  float mn = 1e9f, mx = -1e9f;
  for (int i = 0; i < m; i++) { if (ys[i] < mn) mn = ys[i]; if (ys[i] > mx) mx = ys[i]; }
  for (int y = (int)mn < 0 ? 0 : (int)mn; y <= (int)mx && y < LH; y++) {
    float lo = 1e9f, hi = -1e9f, yy = y + .5f;
    for (int i = 0; i < m; i++) { int j = (i + 1) % m; float a = ys[i], b = ys[j]; if ((a <= yy) == (b <= yy)) continue;
      float x = xs[i] + (yy - a) / (b - a) * (xs[j] - xs[i]); if (x < lo) lo = x; if (x > hi) hi = x; }
    for (int x = (int)lo; x <= (int)hi; x++) px(x, y, c);
  }
}
// real 3D wheel: a 10-sided cylinder around the horizontal axle (ax,az), with tread, side wall, rim and spinning spokes
static void wheel3d(float wx, float wy, float wz, float r, float ax, float az, float spin, uint16_t rim) {
  static const float CS[10] = {1, .809f, .309f, -.309f, -.809f, -1, -.809f, -.309f, .309f, .809f}, SN[10] = {0, .588f, .951f, .951f, .588f, 0, -.588f, -.951f, -.951f, -.588f};
  float al = fsqrt(ax * ax + az * az) + 1e-4f; ax /= al; az /= al; float fx = az, fz = -ax, hw = r * .4f;
  float sg = ((camx - wx) * ax + (camz - wz) * az) > 0 ? 1.f : -1.f, nx[10], ny[10], ffx[10], ffy[10];
  for (int i = 0; i < 10; i++) {
    float x = wx + r * CS[i] * fx, y = wy + r * SN[i], z = wz + r * CS[i] * fz;
    if (!proj(x + sg * hw * ax, y, z + sg * hw * az, &nx[i], &ny[i]) || !proj(x - sg * hw * ax, y, z - sg * hw * az, &ffx[i], &ffy[i])) return;
  }
  float xs[4], ys[4];
  for (int i = 0; i < 10; i++) { int j = (i + 1) % 10; xs[0] = ffx[i]; xs[1] = ffx[j]; xs[2] = nx[j]; xs[3] = nx[i]; ys[0] = ffy[i]; ys[1] = ffy[j]; ys[2] = ny[j]; ys[3] = ny[i]; quad(xs, ys, i & 1 ? C(16, 16, 19) : C(30, 30, 34)); }
  polyn(nx, ny, 10, C(38, 38, 42));
  float ncx, ncy; if (!proj(wx + sg * hw * ax, wy, wz + sg * hw * az, &ncx, &ncy)) return;
  float rxs[10], rys[10]; for (int i = 0; i < 10; i++) { rxs[i] = ncx + (nx[i] - ncx) * .62f; rys[i] = ncy + (ny[i] - ncy) * .62f; }
  polyn(rxs, rys, 10, rim);
  for (int k = 0; k < 3; k++) { float th = spin + k * 2.0944f, ex, ey;
    if (proj(wx + sg * hw * ax + .6f * r * fsin(th + 1.5708f) * fx, wy + .6f * r * fsin(th), wz + sg * hw * az + .6f * r * fsin(th + 1.5708f) * fz, &ex, &ey)) line((int)ncx, (int)ncy, (int)ex, (int)ey, C(55, 55, 60)); }
}
// ---- car: real nodes + virtual points (panels that open, engine block)
static float PX[64], PY[64], PZ[64], sxp[64], syp[64], zr[64];
static int np;
static int vp(float x, float y, float z) { PX[np] = x; PY[np] = y; PZ[np] = z; return np++; }
static int lerpn(int a, int b, float t) { return vp(PX[a] + (PX[b] - PX[a]) * t, PY[a] + (PY[b] - PY[a]) * t, PZ[a] + (PZ[b] - PZ[a]) * t); }
static int rot(int p, int a, int b, float ang, int mode, float cx) {
  float ax = PX[b] - PX[a], ay = PY[b] - PY[a], az = PZ[b] - PZ[a], l = fsqrt(ax * ax + ay * ay + az * az) + 1e-4f; ax /= l; ay /= l; az /= l;
  float vx = PX[p] - PX[a], vy = PY[p] - PY[a], vz = PZ[p] - PZ[a], best = -1e9f, rx = 0, ry = 0, rz = 0;
  for (int sg = -1; sg <= 1; sg += 2) {
    float c = fsin(ang + 1.5708f), s = fsin(ang) * sg, d = ax * vx + ay * vy + az * vz;
    float x = vx * c + (ay * vz - az * vy) * s + ax * d * (1 - c), y = vy * c + (az * vx - ax * vz) * s + ay * d * (1 - c), z = vz * c + (ax * vy - ay * vx) * s + az * d * (1 - c);
    float sc = mode ? ((PX[a] + x - cx) * (PX[a] > cx ? 1 : -1)) : y;
    if (sc > best) { best = sc; rx = x; ry = y; rz = z; }
  }
  return vp(PX[a] + rx, PY[a] + ry, PZ[a] + rz);
}
static float shade(const float *x, const float *y, const float *z, int a, int b, int c2) {
  float ax = x[b] - x[a], ay = y[b] - y[a], az = z[b] - z[a], bx = x[c2] - x[a], by = y[c2] - y[a], bz = z[c2] - z[a];
  float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
  float nl = fsqrt(nx * nx + ny * ny + nz * nz) + 1e-4f, lit = (.3f * nx + .8f * ny + .5f * nz) / nl / .97f; if (lit < 0) lit = -lit;
  return .5f + .5f * lit;
}
static void car(void) {
  float cxm = 0; np = NC; curZ = -1;
  for (int i = 0; i < NC; i++) { PX[i] = n[i].x; PY[i] = n[i].y; PZ[i] = n[i].z; cxm += n[i].x; } cxm /= NC;
  int h0 = 20, h1 = 21;
  if (oHood > .02f && pAtt[0] > 0) { h0 = rot(20, 23, 22, oHood * 1.05f, 0, cxm); h1 = rot(21, 23, 22, oHood * 1.05f, 0, cxm); }
  int tg6 = 6, tg7 = 7;
  if (oTrunk > .02f) { tg6 = rot(6, 10, 11, oTrunk * 1.2f, 0, cxm); tg7 = rot(7, 10, 11, oTrunk * 1.2f, 0, cxm); }
  int dq[2][4];
  for (int s = 0; s < 2; s++) {
    int a = s ? 1 : 0, b = s ? 3 : 2, c1 = s ? 11 : 10, c2 = s ? 13 : 12;
    int p0 = lerpn(a, b, .3f), p1 = lerpn(a, b, .65f), q1 = lerpn(c1, c2, .85f), q0 = lerpn(c1, c2, .1f);
    float o = s ? .03f : -.03f; PX[p0] += o; PX[p1] += o; PX[q0] += o; PX[q1] += o;
    if (oDoor > .02f) { int r0 = rot(p0, p1, q1, oDoor * 1.1f, 1, cxm), r1 = rot(q0, p1, q1, oDoor * 1.1f, 1, cxm); p0 = r0; q0 = r1; }
    dq[s][0] = p0; dq[s][1] = p1; dq[s][2] = q1; dq[s][3] = q0;
  }
  uint8_t fa[22][4]; uint8_t kind[22]; int m = 0;
#define F(k, a, b, c, d) do { fa[m][0] = a; fa[m][1] = b; fa[m][2] = c; fa[m][3] = d; kind[m++] = k; } while (0)
  F(0, 0, 2, 8, 6); F(0, 1, 3, 9, 7); F(0, 0, 1, 7, 6); F(0, 2, 3, 9, 8); F(0, 0, 1, 3, 2); F(0, 10, 11, 13, 12);
  F(1, 12, 13, 19, 18); F(1, 6, 10, 12, 18); F(1, 7, 11, 13, 19); F(1, tg6, tg7, 11, 10);
  F(3, dq[0][0], dq[0][1], dq[0][2], dq[0][3]); F(3, dq[1][0], dq[1][1], dq[1][2], dq[1][3]);
  if (pAtt[0] > 0 || 1) F(0, 23, 22, h1, h0);
  F(4, 24, 25, 26, 27); F(4, 28, 29, 30, 31);
  if (oHood > .1f || pAtt[0] <= 0) {
    int cn[8] = {8, 9, 19, 18, 2, 3, lerpn(5, 3, .4f), lerpn(4, 2, .4f)};
    float mx = 0, my = 0, mz = 0; for (int i = 0; i < 8; i++) { mx += PX[cn[i]] / 8; my += PY[cn[i]] / 8; mz += PZ[cn[i]] / 8; }
    int e[8]; for (int i = 0; i < 8; i++) e[i] = vp(PX[cn[i]] + (mx - PX[cn[i]]) * .28f, PY[cn[i]] + (my - PY[cn[i]]) * .2f - (i < 4 ? .1f : 0), PZ[cn[i]] + (mz - PZ[cn[i]]) * .28f);
    F(2, e[0], e[1], e[2], e[3]); F(2, e[0], e[1], e[5], e[4]); F(2, e[3], e[2], e[6], e[7]); F(2, e[0], e[3], e[7], e[4]); F(2, e[1], e[2], e[6], e[5]);
  }
  for (int i = 0; i < np; i++) {
    float dx = PX[i] - camx, dz = PZ[i] - camz; zr[i] = dx * camhx + dz * camhz; if (zr[i] < .4f) zr[i] = .4f;
    sxp[i] = CXc + (dx * camhz - dz * camhx) / zr[i] * FOC; syp[i] = HOR - (PY[i] - camy) / zr[i] * FOC;
  }
  if (structure) {
    for (int i = 0; i < nbc; i++) if (bm[i].f != 2 && bm[i].o == 0 && bm[i].f != 3) {
      float d = (bm[i].l0 - bm[i].lr) / bm[i].lr; d = (d < 0 ? -d : d) * 12; if (d > 1) d = 1;
      line((int)sxp[bm[i].a], (int)syp[bm[i].a], (int)sxp[bm[i].b], (int)syp[bm[i].b], C(255, 255 - (int)(d * 230), 255 - (int)(d * 255)));
    }
  }
  int id[44]; float dp[44]; int k2 = 0;
  for (int f = 0; f < m && !structure; f++) { id[k2] = f; dp[k2++] = (zr[fa[f][0]] + zr[fa[f][1]] + zr[fa[f][2]] + zr[fa[f][3]]) * .25f - (kind[f] == 3 ? .1f : 0) - (kind[f] == 2 ? .3f : 0); }
  float hx_, hz_, ccx = 0, ccz = 0; heading(&hx_, &hz_); for (int i = 0; i < 20; i++) { ccx += n[i].x / 20; ccz += n[i].z / 20; }
  float camSide = (camx - ccx) * hz_ - (camz - ccz) * hx_;
  for (int w = 14; w < 18; w++) { int sw = (w & 1) ? 1 : -1; id[k2] = 100 + w; dp[k2++] = zr[w] + (sw * camSide > 0 ? -.6f : 1.5f); }
  for (int i = 1; i < k2; i++) { int a = id[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { id[j + 1] = id[j]; dp[j + 1] = dp[j]; j--; } id[j + 1] = a; dp[j + 1] = v; }
  const Veh *V = &VEH[cv]; const Eng *E = &ENG[ce]; const Whl *W = &WHL[cw];
  for (int k = 0; k < k2; k++) {
    if (id[k] >= 100) { int w = id[k] - 100; float ax = hz_, az = -hx_;
      if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); }
      wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); continue; }
    int f = id[k]; float xs[4], ys[4]; const uint8_t *q = fa[f];
    for (int i = 0; i < 4; i++) { xs[i] = sxp[q[i]]; ys[i] = syp[q[i]]; }
    float sh = shade(PX, PY, PZ, q[0], q[1], q[3]), lit = (sh - .5f) * 2, dk = 1.f - dmg * .006f; uint16_t col;
    if (kind[f] == 1) col = C((int)(120 * (.8f + .2f * lit)), (int)(170 * (.8f + .2f * lit)), 210);
    else if (kind[f] == 2) col = C((int)(E->r * sh), (int)(E->g * sh), (int)(E->b * sh));
    else if (kind[f] == 4) col = C((int)(75 * sh), (int)(75 * sh), (int)(80 * sh));
    else { float d2 = kind[f] == 3 ? .85f : 1.f; col = C((int)(V->r * sh * dk * d2), (int)(V->g * sh * dk * d2), (int)(V->b * sh * dk * d2)); }
    quad(xs, ys, col);
  }
}
// ---- boxes (objects, trailer)
static void draw_box(int n0, int cr, int cg, int cb) {
  float x[8], y[8], z[8], sx[8], sy[8], zz[8], dsum = 0;
  for (int i = 0; i < 8; i++) {
    x[i] = n[n0 + i].x; y[i] = n[n0 + i].y; z[i] = n[n0 + i].z;
    float dx = x[i] - camx, dz = z[i] - camz; zz[i] = dx * camhx + dz * camhz; if (zz[i] < .5f) return; dsum += zz[i];
    sx[i] = CXc + (dx * camhz - dz * camhx) / zz[i] * FOC; sy[i] = HOR - (y[i] - camy) / zz[i] * FOC;
  }
  static const uint8_t FB[6][4] = {{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}};
  int zq = (int)(dsum / 8 * zsc) + 3; curZ = zq > 255 ? 255 : zq;
  int id[6]; float dp[6];
  for (int f = 0; f < 6; f++) { id[f] = f; dp[f] = (zz[FB[f][0]] + zz[FB[f][1]] + zz[FB[f][2]] + zz[FB[f][3]]) * .25f; }
  for (int i = 1; i < 6; i++) { int a = id[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { id[j + 1] = id[j]; dp[j + 1] = dp[j]; j--; } id[j + 1] = a; dp[j + 1] = v; }
  for (int k = 0; k < 6; k++) {
    const uint8_t *q = FB[id[k]]; float xs[4], ys[4]; for (int i = 0; i < 4; i++) { xs[i] = sx[q[i]]; ys[i] = sy[q[i]]; }
    float sh = shade(x, y, z, q[0], q[1], q[3]); quad(xs, ys, C((int)(cr * sh), (int)(cg * sh), (int)(cb * sh)));
  }
  curZ = -1;
}
static void trailer_wheels(int nearPass) {
  float tx = n[ntr+4].x + n[ntr+5].x + n[ntr+6].x + n[ntr+7].x - n[ntr].x - n[ntr+1].x - n[ntr+2].x - n[ntr+3].x, tz = n[ntr+4].z + n[ntr+5].z + n[ntr+6].z + n[ntr+7].z - n[ntr].z - n[ntr+1].z - n[ntr+2].z - n[ntr+3].z;
  float l = fsqrt(tx * tx + tz * tz) + 1e-4f, ax = tz / l, az = -tx / l, cx = (n[ntr+8].x + n[ntr+9].x) * .5f, cz = (n[ntr+8].z + n[ntr+9].z) * .5f, camSide = (camx - cx) * ax + (camz - cz) * az;
  for (int w = 8; w < 10; w++) { float side = (n[ntr + w].x - cx) * ax + (n[ntr + w].z - cz) * az; int near = side * camSide > 0;
    if (near == nearPass) wheel3d(n[ntr + w].x, n[ntr + w].y, n[ntr + w].z, .33f, ax, az, wspin, C(150, 150, 155)); }
}
// ---- top-down damage view
static void frame(float *hx, float *hz, float *cx, float *cy, float *cz) {
  heading(hx, hz); *cx = *cy = *cz = 0; for (int i = 0; i < 20; i++) { *cx += n[i].x / 20; *cy += n[i].y / 20; *cz += n[i].z / 20; }
}
static void draw_top(int cx0, int cy0, float S, int full) {
  float hx, hz, ccx, ccy, ccz; frame(&hx, &hz, &ccx, &ccy, &ccz); curZ = -1;
#define TX(i) (cx0 + ((n[i].x - ccx) * hz - (n[i].z - ccz) * hx) * S)
#define TY(i) (cy0 - ((n[i].x - ccx) * hx + (n[i].z - ccz) * hz) * S)
  for (int i = 0; i < nbc; i++) {
    Beam *b = &bm[i]; if (b->o != 0 || b->f > 1 || b->t0 > 1) continue;
    float d = (b->l0 - b->lr) / b->lr; d = (d < 0 ? -d : d) * 14; if (d > 1) d = 1;
    int r = d < .5f ? 60 + (int)(d * 390) : 255, g = d < .5f ? 200 : 200 - (int)((d - .5f) * 380);
    line((int)TX(b->a), (int)TY(b->a), (int)TX(b->b), (int)TY(b->b), C(r, g, 50));
  }
  for (int g = 0; g < 3; g++) {
    int b0 = 20 + 4 * g; float u = 0, v = g == 0 ? 1.4f : (g == 1 ? 2.4f : -2.4f);
    if (pAtt[g] > 0) { for (int i = 0; i < 4; i++) line((int)TX(b0 + i), (int)TY(b0 + i), (int)TX(b0 + ((i + 1) & 3)), (int)TY(b0 + ((i + 1) & 3)), C(230, 230, 235)); }
    else { int x = (int)(cx0 + u * S), y = (int)(cy0 - v * S * gsz), r = full ? 6 : 3; line(x - r, y - r, x + r, y + r, C(255, 40, 40)); line(x - r, y + r, x + r, y - r, C(255, 40, 40)); }
  }
  for (int w = 0; w < 4; w++) {
    int i = 14 + w; float su = (w & 1 ? 1 : -1) * 1.1f * gsx, sv = (w >> 1 ? 1.3f : -1.3f) * gsz; int x = (int)(cx0 + su * S), y = (int)(cy0 - sv * S);
    if (wAtt[w] > 0) { for (int q = -1; q <= 1; q++) { int hh = full ? 5 : 2, ww = full ? 2 : 1; for (int yy = -hh; yy <= hh; yy++) for (int xx = -ww; xx <= ww; xx++) px((int)TX(i) + xx, (int)TY(i) + yy, C(150, 150, 160)); } }
    else { int r = full ? 6 : 3; line(x - r, y - r, x + r, y + r, C(255, 40, 40)); line(x - r, y + r, x + r, y - r, C(255, 40, 40)); }
  }
}
static void zones(int *zp) {
  float hx, hz, ccx, ccy, ccz, sum[5] = {0, 0, 0, 0, 0}; int cnt[5] = {0, 0, 0, 0, 0}; frame(&hx, &hz, &ccx, &ccy, &ccz);
  for (int i = 0; i < nbc; i++) {
    Beam *b = &bm[i]; if (b->o != 0 || b->t0 != 0) continue;
    float mx = (n[b->a].x + n[b->b].x) * .5f - ccx, mz = (n[b->a].z + n[b->b].z) * .5f - ccz, my = (n[b->a].y + n[b->b].y) * .5f - ccy;
    float u = mx * hz - mz * hx, v = mx * hx + mz * hz, d = b->f == 2 ? .7f : (b->l0 - b->lr) / b->lr; if (d < 0) d = -d;
    int z[5] = {v > .9f * gsz, v < -.9f * gsz, u < -.45f, u > .45f, my > .35f};
    for (int k = 0; k < 5; k++) if (z[k]) { sum[k] += d; cnt[k]++; }
  }
  for (int k = 0; k < 5; k++) { int p = cnt[k] ? (int)(sum[k] / cnt[k] * 100.f * 2.5f) : 0; zp[k] = p > 100 ? 100 : p; }
}
static void present(int xd0) {
  int w = 320 - xd0;
  for (int by = 0; by < 224; by += 16) {
    for (int r = 0; r < 16; r++) { const uint16_t *src = &fb[((by + r) * lh / 224) * lw]; uint16_t *d = &buf[r * w]; for (int x = 0; x < w; x++) d[x] = src[xmap[xd0 + x]]; }
    eadk_display_push_rect((eadk_rect_t){xd0, by, w, 16}, buf);
  }
}
static float depth_of(float x, float z) { return (x - camx) * camhx + (z - camz) * camhz; }
static void scene(int xd0) {
  int xl = xd0 * lw / 320; terrain(xl);
  struct { int ty, idx; float d; } L[12]; int m = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  L[m].ty = 0; L[m].idx = 0; L[m++].d = depth_of(cx, cz);
  if (ntr >= 0) { L[m].ty = 1; L[m].idx = 0; L[m++].d = depth_of(n[ntr + 4].x, n[ntr + 4].z); }
  for (int o = 0; o < nobj && m < 12; o++) { int b = nob0 + o * 8; L[m].ty = 2; L[m].idx = o; L[m++].d = depth_of(n[b].x, n[b].z); }
  for (int i = 1; i < m; i++) { __typeof__(L[0]) v = L[i]; int j = i - 1; while (j >= 0 && L[j].d < v.d) { L[j + 1] = L[j]; j--; } L[j + 1] = v; }
  for (int k = 0; k < m; k++) {
    if (L[k].ty == 0) car();
    else if (L[k].ty == 1) { trailer_wheels(0); draw_box(ntr, 150, 150, 160); trailer_wheels(1); }
    else if (L[k].d > 1 && L[k].d < zmax - 2) { const Obj *O = &OBJ[okind[L[k].idx]]; draw_box(nob0 + L[k].idx * 8, O->r, O->g, O->b); }
  }
  if (showTop && xd0 == 0) { int bw = lw / 4, bh = lh * 40 / 112; for (int y = lh - bh; y < lh; y++) for (int x = 0; x < bw; x++) fb[y * lw + x] = C(18, 20, 30); draw_top(bw / 2, lh - bh / 2, 6.5f * lw / 160, 0); }
}
// ---- settings, controls
enum { A_ACC, A_BRK, A_LEFT, A_RIGHT, A_RESET, A_HOOD, A_DOORS, A_TRUNK, A_ALL, A_BEAMS, A_TOP, A_DMG, A_CL, A_CR, A_CU, A_CD, A_ZI, A_ZO, A_CRESET, A_QUICK, A_TURBO, NA };
static const char *AN[NA] = {"Accelerer", "Freiner", "Gauche", "Droite", "Remettre/Rejouer", "Capot", "Portes", "Coffre", "Tout ouvrir", "Poutres", "Vue dessus", "Degats", "Camera gauche", "Camera droite", "Camera haut", "Camera bas", "Zoom +", "Zoom -", "Camera reset", "Menu rapide", "Turbo (tenir)"};
static const int BDEF[NA] = {eadk_key_up, eadk_key_down, eadk_key_left, eadk_key_right, eadk_key_ok, eadk_key_var, eadk_key_xnt, eadk_key_exp, eadk_key_shift, eadk_key_toolbox, eadk_key_ln, eadk_key_log, eadk_key_four, eadk_key_six, eadk_key_eight, eadk_key_two, eadk_key_seven, eadk_key_nine, eadk_key_five, eadk_key_exe, eadk_key_backspace};
static int bind[NA];
static const char *KN[53] = {[0]="Gauche",[1]="Haut",[2]="Bas",[3]="Droite",[4]="OK",[5]="Retour",[6]="Home",[8]="On/Off",[12]="Shift",[13]="Alpha",[14]="X,n,t",[15]="Var",[16]="Boite outils",[17]="Effacer",[18]="Exp",[19]="Ln",[20]="Log",[21]="i",[22]="Virgule",[23]="Puissance",[24]="Sin",[25]="Cos",[26]="Tan",[27]="Pi",[28]="Racine",[29]="Carre",[30]="7",[31]="8",[32]="9",[33]="(",[34]=")",[36]="4",[37]="5",[38]="6",[39]="x",[40]="/",[42]="1",[43]="2",[44]="3",[45]="+",[46]="-",[48]="0",[49]=".",[50]="EE",[51]="Ans",[52]="EXE"};
static int qual = 1, fovI = 1, vdI = 1, steerL = 3, camL = 3;
static void setq(void) {
  static const int WQ[3] = {128, 160, 208}, HQ[3] = {90, 112, 146}; static const float FV[3] = {1.3f, 1.f, .78f}, ZM[3] = {45.f, 72.f, 110.f};
  lw = WQ[qual]; lh = HQ[qual]; foc = lw * .8125f * FV[fovI]; zmax = ZM[vdI]; zsc = 250.f / zmax;
  for (int x = 0; x < 320; x++) xmap[x] = (uint8_t)(x * lw / 320);
}
static int bdown(uint64_t k, int a) { return (k >> bind[a]) & 1; }
static int bpress(uint64_t k, int a) { return ((k >> bind[a]) & 1) && !((pk >> bind[a]) & 1); }
// ---- text and menus (ASCII only)
static char *cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *num(char *s, int v) { char t[8]; int k = 0; if (v < 0) v = 0; do { t[k++] = '0' + v % 10; v /= 10; } while (v && k < 7); while (k) *s++ = t[--k]; *s = 0; return s; }
static void txt(const char *s, int x, int y, int big, uint16_t fg, uint16_t bg) { eadk_display_draw_string(s, (eadk_point_t){x, y}, big, fg, bg); }
static void row(const char *s, int x, int y, int w, int h, int sel) {
  uint16_t bg = sel ? C(40, 90, 200) : C(15, 18, 28);
  eadk_display_push_rect_uniform((eadk_rect_t){x, y, w, h}, bg); txt(s, x + 6, y + (h - 14) / 2, 0, 0xFFFF, bg);
}
static int pressed(uint64_t k, int key) { return ((k >> key) & 1) && !((pk >> key) & 1); }
static int down(uint64_t k, int key) { return (k >> key) & 1; }
static void clear(void) { eadk_display_push_rect_uniform(eadk_screen_rect, C(15, 18, 28)); }
static void garage_car(void) { car_init(mapId == 0 ? TA : 0, 0, 0, 1, .02f, 0); }
static int ct, stopT, score; static float vpk, impV;
static void startpos(void) {
  car_init(mapId == 0 ? TA : 0, 0, 0, 1, .4f, 1); ct = 0; stopT = 0; vpk = 0; impV = 0; score = 0;
  if (mapId == 2) for (int i = 0; i < (ntr >= 0 ? ntr + 11 : NC); i++) n[i].vz = CRASHV[crashI] / 3.6f;
}
static const char *MAPN[3] = {"Circuit", "Route", "Crash-test"}, *SOLN[3] = {"Robuste", "Normale", "Fragile"};
static const char *MN[8] = {"Jouer", "Garage", "Carte", "Solidite", "Test", "Reglages", "Commandes", "Quitter"};
static void draw_menu(int sel) {
  clear(); txt("NumBeam 3D", 90, 6, 1, C(230, 60, 50), C(15, 18, 28)); txt("simulateur de collision", 82, 32, 0, C(150, 160, 190), C(15, 18, 28));
  for (int i = 0; i < 8; i++) { char s[40], *p = s; p = cat(p, MN[i]);
    if (i == 2) { p = cat(p, ": "); cat(p, MAPN[mapId]); } else if (i == 3) { p = cat(p, ": "); cat(p, SOLN[solid]); } else if (i == 4) { p = cat(p, ": "); p = num(p, CRASHV[crashI]); cat(p, " km/h"); }
    row(s, 80, 52 + i * 21, 160, 18, i == sel); }
  txt("Haut/Bas, Gauche/Droite, OK", 66, 222, 0, C(110, 120, 150), C(15, 18, 28));
}
static void draw_garage(int sel, int gv) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 160, 240}, C(15, 18, 28));
  const char *lab[11] = {"Vue (OK)", "Vehicule", "Roues", "Susp.", "Moteur", "Chassis", "Remorque", "Capot", "Portes", "Coffre", "JOUER"};
  const char *val[10] = {"", VEH[cv].nm, WHL[cw].nm, SUS[cs].nm, ENG[ce].nm, CHA[cc].nm, trl ? "oui" : "non", tHood > .5f ? "ouvert" : "ferme", tDoor > .5f ? "ouvertes" : "fermees", tTrunk > .5f ? "ouvert" : "ferme"};
  for (int i = 0; i < 11; i++) { char s[32], *p = s; p = cat(p, lab[i]); if (i > 0 && i < 10) { p = cat(p, ": "); cat(p, val[i]); } row(s, 4, 3 + i * 17, 152, 16, i == sel); }
  if (gv) { txt("Fleches: tourner/hauteur", 4, 196, 0, C(255, 210, 80), C(15, 18, 28)); txt("OK ou Retour: fin", 4, 214, 0, C(255, 210, 80), C(15, 18, 28)); return; }
  char s[32], *p = s; p = cat(p, "Puiss "); p = num(p, (int)(ENG[ce].a * VEH[cv].pw * 10)); p = cat(p, " Vmax "); num(p, (int)(ENG[ce].v * 3.6f));
  txt(s, 6, 196, 0, C(180, 190, 220), C(15, 18, 28));
  p = s; p = cat(p, "Grip "); p = num(p, (int)(WHL[cw].gr * VEH[cv].gr * 100)); p = cat(p, " Solid. "); num(p, (int)(CHA[cc].k / 140 * (solid == 0 ? 1.6f : (solid == 2 ? .65f : 1.f))));
  txt(s, 6, 214, 0, C(180, 190, 220), C(15, 18, 28));
}
static void draw_pause(int sel) {
  const char *it[8] = {"Reprendre", "Recommencer", "Options rapides", "Degats (dessus)", "Garage", "Reglages", "Commandes", "Menu principal"};
  eadk_display_push_rect_uniform((eadk_rect_t){70, 8, 180, 224}, C(15, 18, 28)); txt("PAUSE", 140, 12, 0, 0xFFFF, C(15, 18, 28));
  for (int i = 0; i < 8; i++) row(it[i], 80, 30 + i * 25, 160, 21, i == sel);
}
static void draw_set(int sel) {
  static const char *QN[3] = {"Basse", "Normale", "Haute"}, *FN[3] = {"Etroit", "Normal", "Large"}, *VN[3] = {"Courte", "Normale", "Longue"};
  clear(); txt("REGLAGES", 120, 6, 1, C(230, 60, 50), C(15, 18, 28));
  for (int i = 0; i < 7; i++) { char s[40], *p = s;
    static const char *lb[7] = {"Qualite", "Direction", "Camera", "Champ de vision", "Distance de vue", "Test des touches", "Retour"};
    p = cat(p, lb[i]);
    if (i == 0) { p = cat(p, ": "); cat(p, QN[qual]); } else if (i == 1) { p = cat(p, ": "); num(p, steerL); } else if (i == 2) { p = cat(p, ": "); num(p, camL); }
    else if (i == 3) { p = cat(p, ": "); cat(p, FN[fovI]); } else if (i == 4) { p = cat(p, ": "); cat(p, VN[vdI]); }
    row(s, 40, 36 + i * 24, 240, 20, i == sel); }
  txt("Direction/Camera: sensibilite 1 a 5", 38, 206, 0, C(150, 160, 190), C(15, 18, 28)); txt("Gauche/Droite: changer", 38, 222, 0, C(150, 160, 190), C(15, 18, 28));
}
static void draw_ctrl(int sel, int cap) {
  clear(); txt("COMMANDES", 115, 2, 1, C(230, 60, 50), C(15, 18, 28));
  int top = sel - 4; if (top < 0) top = 0; if (top > NA + 1 - 9) top = NA + 1 - 9;
  for (int i = 0; i < 9; i++) { int a = top + i; char s[48], *p = s; if (a > NA) break;
    if (a == NA) cat(s, "Restaurer les defauts"); else { p = cat(p, AN[a]); p = cat(p, ": "); cat(p, KN[bind[a]] ? KN[bind[a]] : "?"); if (cap && a == sel) cat(s, "  (appuie sur une touche)"); }
    row(s, 10, 24 + i * 21, 300, 19, a == sel); }
  txt(cap ? "Retour: annuler" : "OK: changer  Retour: quitter", 10, 220, 0, C(150, 160, 190), C(15, 18, 28));
}
static int turboT, gi, ri; static float tscale = 1.f;
static const char *QL[10] = {"Reparer tout", "Turbo", "Boost !", "Voiture", "Moteur", "Remorque", "Route", "Gravite", "Temps", "Retour au depart"};
static void draw_quick(int sel) {
  static const char *RN[3] = {"Seche", "Mouillee", "Glace"}, *GN[3] = {"Normale", "Lune", "Forte"};
  eadk_display_push_rect_uniform((eadk_rect_t){40, 0, 240, 240}, C(15, 18, 28)); txt("OPTIONS RAPIDES", 108, 3, 0, C(255, 210, 80), C(15, 18, 28));
  for (int i = 0; i < 10; i++) { char s[40], *p = s; p = cat(p, QL[i]);
    if (i == 1) cat(p, turboT ? ": oui" : ": non"); else if (i == 3) { p = cat(p, ": "); cat(p, VEH[cv].nm); } else if (i == 4) { p = cat(p, ": "); cat(p, ENG[ce].nm); }
    else if (i == 5) cat(p, trl ? ": oui" : ": non"); else if (i == 6) { p = cat(p, ": "); cat(p, RN[ri]); } else if (i == 7) { p = cat(p, ": "); cat(p, GN[gi]); } else if (i == 8) cat(p, tscale < .5f ? ": ralenti" : ": normal");
    row(s, 50, 19 + i * 22, 220, 19, i == sel); }
}
static void draw_dmg(void) {
  for (int i = 0; i < LW * LH; i++) fb[i] = C(15, 18, 28);
  curZ = -1; draw_top(lw * 45 / 160, lh / 2, 20.f * lw / 160, 1); present(0);
  int zp[5]; zones(zp); const char *zn[5] = {"Avant", "Arriere", "Gauche", "Droite", "Toit"};
  txt("DEGATS", 220, 6, 1, C(230, 60, 50), C(15, 18, 28));
  for (int i = 0; i < 5; i++) { char s[24], *p = s; p = cat(p, zn[i]); p = cat(p, ": "); p = num(p, zp[i]); cat(p, "%"); txt(s, 190, 36 + i * 17, 0, zp[i] > 60 ? C(255, 90, 80) : (zp[i] > 25 ? C(255, 210, 80) : C(120, 220, 120)), C(15, 18, 28)); }
  char s[24], *p = s; p = cat(p, "Capot: "); cat(p, pAtt[0] > 0 ? "ok" : "arrache"); txt(s, 190, 128, 0, 0xFFFF, C(15, 18, 28));
  p = s; p = cat(p, "Pare-ch.AV: "); cat(p, pAtt[1] > 0 ? "ok" : "perdu"); txt(s, 190, 145, 0, 0xFFFF, C(15, 18, 28));
  p = s; p = cat(p, "Pare-ch.AR: "); cat(p, pAtt[2] > 0 ? "ok" : "perdu"); txt(s, 190, 162, 0, 0xFFFF, C(15, 18, 28));
  int wl = 0; for (int w = 0; w < 4; w++) wl += wAtt[w] > 0; p = s; p = cat(p, "Roues: "); p = num(p, wl); cat(p, "/4"); txt(s, 190, 179, 0, 0xFFFF, C(15, 18, 28));
  p = s; p = cat(p, "Total: "); p = num(p, (int)dmg); cat(p, "%"); txt(s, 190, 196, 0, C(255, 210, 80), C(15, 18, 28));
  if (mapId == 2 && ct == 2) { p = s; p = cat(p, "SCORE "); num(p, score); txt(s, 6, 226, 1, C(255, 210, 80), C(15, 18, 28)); txt("OK : rejouer", 190, 226, 0, 0xFFFF, C(15, 18, 28)); }
  else txt("Une touche pour revenir", 6, 226, 0, C(150, 160, 190), C(15, 18, 28));
}
static void hud(int kmh, int gear, int lap, int ms, int best) {
  char s[64], *p = s; p = num(p, kmh); p = cat(p, "km/h "); if (gear < 0) p = cat(p, "R"); else { p = cat(p, "G"); p = num(p, gear); }
  p = cat(p, " Deg "); p = num(p, (int)dmg); p = cat(p, "%"); if (turbo) p = cat(p, " TURBO");
  if (mapId == 0) { p = cat(p, " T"); p = num(p, lap); p = cat(p, " "); p = num(p, ms / 60000); p = cat(p, ":"); int sc = ms / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); p = cat(p, "."); p = num(p, ms / 100 % 10);
    if (best) { p = cat(p, " B"); p = num(p, best / 60000); p = cat(p, ":"); sc = best / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); } }
  else if (mapId == 2) { p = cat(p, " Test "); p = num(p, CRASHV[crashI]); if (ct == 2) { p = cat(p, " SCORE "); p = num(p, score); } else if (ct == 1) p = cat(p, " IMPACT!"); }
  eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14)); txt(s, 4, 225, 0, 0xFFFF, C(10, 10, 14));
}
int main(void) {
  int st = S_MENU, sel = 0, redraw = 1, gsel = 0, gview = 0, retS = S_MENU, cap = 0, fr = 0, lap = 1, best = 0, cp = 0;
  float acc = 0, stw = 0, pz = 0, gorb = .6f, gelev = 3.4f, shx = 0, shz = 1, yawO = 0, hO = 0, dO = 0; uint64_t last = eadk_timing_millis(), lapT = last;
  for (int i = 0; i < NA; i++) bind[i] = BDEF[i];
#ifdef TESTQ
  qual = TESTQ;
#endif
  setq();
#ifdef TESTST
  st = TESTST; tHood = oHood = TESTO; tDoor = oDoor = TESTO; tTrunk = oTrunk = TESTO; cv = TESTV; ce = 3; cw = 1; trl = TESTT; mapId = TESTM; crashI = 3; gview = TESTGV;
  if (st == S_GARAGE) garage_car(); else startpos();
#endif
  for (;;) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    int U = pressed(k, eadk_key_up), D = pressed(k, eadk_key_down), L = pressed(k, eadk_key_left), R = pressed(k, eadk_key_right), O = pressed(k, eadk_key_ok), B = pressed(k, eadk_key_back);
    if (st == S_MENU) {
      if (redraw) { draw_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 7) % 8; redraw = 1; } if (D) { sel = (sel + 1) % 8; redraw = 1; }
      if ((L || R || O) && sel >= 2 && sel <= 4) { int d = L ? -1 : 1; redraw = 1;
        if (sel == 2) mapId = (mapId + 3 + d) % 3; else if (sel == 3) solid = (solid + 3 + d) % 3; else crashI = (crashI + 5 + d) % 5; }
      else if (O) { if (sel == 0) { startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 1) { garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 5) { retS = S_MENU; sel = 0; st = S_SET; redraw = 1; }
        else if (sel == 6) { retS = S_MENU; sel = 0; cap = 0; st = S_CTRL; redraw = 1; } else if (sel == 7) return 0; }
      eadk_timing_msleep(30);
    } else if (st == S_GARAGE) {
      if (redraw) { draw_garage(gsel, gview); redraw = 0; }
      if (gview) {                                       // free orbit with the arrow keys
        if (down(k, eadk_key_left)) gorb -= .05f; if (down(k, eadk_key_right)) gorb += .05f;
        if (down(k, eadk_key_up)) gelev += .1f; if (down(k, eadk_key_down)) gelev -= .1f; if (gelev > 9.f) gelev = 9.f; if (gelev < .3f) gelev = .3f;
        if (O || B) { gview = 0; redraw = 1; }
      } else {
        int ch = 0;
        if (U) { gsel = (gsel + 10) % 11; redraw = 1; } if (D) { gsel = (gsel + 1) % 11; redraw = 1; }
        if (O && gsel == 0) { gview = 1; redraw = 1; }
        else if (L || R || (O && gsel >= 7 && gsel <= 9)) { int d = R ? 1 : 3; redraw = 1; ch = 1;
          if (gsel == 1) cv = (cv + d) % 4; else if (gsel == 2) cw = (cw + d) % 4; else if (gsel == 3) cs = (cs + d) % 4; else if (gsel == 4) ce = (ce + d) % 4; else if (gsel == 5) cc = (cc + (R ? 1 : 2)) % 3;
          else if (gsel == 6) trl ^= 1; else if (gsel == 7) tHood = tHood > .5f ? 0 : 1; else if (gsel == 8) tDoor = tDoor > .5f ? 0 : 1; else if (gsel == 9) tTrunk = tTrunk > .5f ? 0 : 1; else ch = 0; }
        if (ch && gsel <= 6 && gsel >= 1) garage_car();
        if (O && gsel == 10) { startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        if (B) { st = S_MENU; redraw = 1; }
      }
      if (st == S_GARAGE) {
        oHood += (tHood - oHood) * .15f; oDoor += (tDoor - oDoor) * .15f; oTrunk += (tTrunk - oTrunk) * .15f;
        float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); float dist = trl ? 14.f : 11.f;
        camhx = fsin(gorb); camhz = fsin(gorb + 1.5708f); camx = cx - camhx * dist; camz = cz - camhz * dist; camy = cy + gelev; CXc = lw * 3 / 4;
        hor = (int)(lh * .66f - gelev * foc / dist);
        int sv = showTop; showTop = 0; scene(160); showTop = sv; present(160);
      }
    } else if (st == S_GAME) {
      if (B) { st = S_PAUSE; sel = 0; redraw = 1; pk = k; continue; }
      CXc = lw / 2;
      float hx, hz, cx, cy, cz, vx = 0, vz = 0; frame(&hx, &hz, &cx, &cy, &cz);
      for (int i = 0; i < 20; i++) { vx += n[i].vx / 20; vz += n[i].vz / 20; }
      float vf = vx * hx + vz * hz, sp = fsqrt(vx * vx + vz * vz);
      if (bpress(k, A_RESET)) { if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); }
      if (bpress(k, A_BEAMS)) structure ^= 1;
      if (bpress(k, A_TOP)) showTop ^= 1;
      if (bpress(k, A_DMG)) { st = S_DMG; redraw = 1; pk = k; continue; }
      if (bpress(k, A_QUICK)) { st = S_QUICK; sel = 0; redraw = 1; pk = k; continue; }
      if (bpress(k, A_HOOD)) tHood = tHood > .5f ? 0 : 1; if (bpress(k, A_DOORS)) tDoor = tDoor > .5f ? 0 : 1; if (bpress(k, A_TRUNK)) tTrunk = tTrunk > .5f ? 0 : 1;
      if (bpress(k, A_ALL)) { float v = (tHood > .5f || tDoor > .5f || tTrunk > .5f) ? 0 : 1; tHood = tDoor = tTrunk = v; }
      oHood += (tHood - oHood) * .2f; oDoor += (tDoor - oDoor) * .2f; oTrunk += (tTrunk - oTrunk) * .2f;
      float cs = camL * .0133f;                           // free camera: orbit, height, zoom
      if (bdown(k, A_CL)) yawO += cs; if (bdown(k, A_CR)) yawO -= cs; if (yawO > 3.1416f) yawO -= 6.2832f; if (yawO < -3.1416f) yawO += 6.2832f;
      if (bdown(k, A_CU)) hO += cs * 5.f; if (bdown(k, A_CD)) hO -= cs * 5.f; if (hO > 9.f) hO = 9.f; if (hO < -1.f) hO = -1.f;
      if (bdown(k, A_ZI)) dO -= cs * 8.f; if (bdown(k, A_ZO)) dO += cs * 8.f; if (dO > 12.f) dO = 12.f; if (dO < -4.f) dO = -4.f;
      if (bpress(k, A_CRESET)) { yawO = 0; hO = 0; dO = 0; }
      turbo = turboT || bdown(k, A_TURBO); thr = bdown(k, A_ACC) ? 1.f : 0; brk = 0;
      if (bdown(k, A_BRK)) { if (vf > 1.f) brk = 1; else thr = -.6f; }
      float tgt = (bdown(k, A_RIGHT) ? 1.f : 0) - (bdown(k, A_LEFT) ? 1.f : 0);
      stw += (tgt * (.30f + .05f * steerL) / (1.f + sp * .06f) - stw) * (.1f + .05f * steerL); steer = stw;
      uint64_t now = eadk_timing_millis(); float dtf = (float)(now - last) * tscale; wspin += vf * dtf * .001f / .35f; acc += dtf; last = now; if (acc > 60) acc = 60;
      while (acc >= 3.5f) { step(.0035f); acc -= 3.5f; }
      if (mapId == 0) {
        if (cx < -TA * .8f && cz > -20 && cz < 20) cp = 1;
        if (cp && pz < 0 && cz >= 0 && cx > 0) { int t = (int)(now - lapT); if (!best || t < best) best = t; lap++; lapT = now; cp = 0; }
        pz = cz;
      } else if (mapId == 2) {
        if (sp > vpk) vpk = sp;
        if (ct == 0 && dmg > 3.f) { ct = 1; impV = vpk; }
        if (ct == 1) { stopT = sp < 1.5f ? stopT + 1 : 0; if (stopT > 70) { ct = 2; score = (int)(dmg * impV * 3.6f / 5.f); st = S_DMG; redraw = 1; pk = k; continue; } }
      }
      shx += (hx - shx) * .1f; shz += (hz - shz) * .1f; float l = fsqrt(shx * shx + shz * shz) + 1e-4f; shx /= l; shz /= l;
      float c = fsin(yawO + 1.5708f), s = fsin(yawO); camhx = shx * c + shz * s; camhz = shz * c - shx * s;
      float dist = (ntr >= 0 ? 10.f : 8.5f) + dO; if (dist < 3.f) dist = 3.f;
      camx = cx - camhx * dist; camz = cz - camhz * dist;
      float gy = gh(camx, camz) + 1.4f, ty = cy + 2.6f + hO; if (ty < gy) ty = gy; camy += (ty - camy) * .15f;
      hor = (int)(lh * .72f - (camy - cy) * foc / dist);
      scene(0); present(0);
      if (++fr % 6 == 0) { int g = 1 + (int)(sp / (engV / 5.2f)); if (g > 5) g = 5; hud((int)(sp * 3.6f), vf < -.5f ? -1 : g, lap, (int)(now - lapT), best); }
    } else if (st == S_DMG) {
      if (redraw) { draw_dmg(); redraw = 0; }
      if (U || D || L || R || O || B) { if (O && mapId == 2 && ct == 2) { startpos(); } st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
      eadk_timing_msleep(30);
    } else if (st == S_PAUSE) {
      if (redraw) { draw_pause(sel); redraw = 0; }
      if (U) { sel = (sel + 7) % 8; redraw = 1; } if (D) { sel = (sel + 1) % 8; redraw = 1; }
      if (B) { sel = 0; O = 1; }
      if (O) { if (sel == 0) { st = S_GAME; last = eadk_timing_millis(); acc = 0; }
        else if (sel == 1) { startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; }
        else if (sel == 2) { st = S_QUICK; sel = 0; redraw = 1; } else if (sel == 3) { st = S_DMG; redraw = 1; }
        else if (sel == 4) { garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 5) { retS = S_PAUSE; sel = 0; st = S_SET; redraw = 1; }
        else if (sel == 6) { retS = S_PAUSE; sel = 0; cap = 0; st = S_CTRL; redraw = 1; } else { st = S_MENU; sel = 0; redraw = 1; } }
      eadk_timing_msleep(30);
    } else if (st == S_SET) {
      if (redraw) { draw_set(sel); redraw = 0; }
      if (U) { sel = (sel + 6) % 7; redraw = 1; } if (D) { sel = (sel + 1) % 7; redraw = 1; }
      if (L || R) { int d = R ? 1 : -1; redraw = 1;
        if (sel == 0) { qual = (qual + 3 + d) % 3; setq(); } else if (sel == 1) { steerL += d; if (steerL < 1) steerL = 1; if (steerL > 5) steerL = 5; }
        else if (sel == 2) { camL += d; if (camL < 1) camL = 1; if (camL > 5) camL = 5; } else if (sel == 3) { fovI = (fovI + 3 + d) % 3; setq(); } else if (sel == 4) { vdI = (vdI + 3 + d) % 3; setq(); } }
      if (O && sel == 5) { st = S_KEYS; redraw = 1; }
      if ((O && sel == 6) || B) { if (retS == S_PAUSE) { st = S_PAUSE; sel = 4; } else { st = S_MENU; sel = 5; } redraw = 1; }
      eadk_timing_msleep(30);
    } else if (st == S_CTRL) {
      if (redraw) { draw_ctrl(sel, cap); redraw = 0; }
      if (cap) {                                          // wait for the new key
        if (B) { cap = 0; redraw = 1; }
        else { uint64_t nw = k & ~pk; for (int key = 0; key < 53; key++) if (((nw >> key) & 1) && key != eadk_key_back && key != eadk_key_home && key != eadk_key_on_off && KN[key]) {
            int old = bind[sel]; for (int a = 0; a < NA; a++) if (bind[a] == key) bind[a] = old; bind[sel] = key; cap = 0; redraw = 1; break; } }
      } else {
        if (U) { sel = (sel + NA) % (NA + 1); redraw = 1; } if (D) { sel = (sel + 1) % (NA + 1); redraw = 1; }
        if (O) { if (sel == NA) { for (int a = 0; a < NA; a++) bind[a] = BDEF[a]; redraw = 1; } else { cap = 1; redraw = 1; } }
        if (B) { if (retS == S_PAUSE) { st = S_PAUSE; sel = 5; } else { st = S_MENU; sel = 6; } redraw = 1; }
      }
      eadk_timing_msleep(30);
    } else if (st == S_QUICK) {
      if (redraw) { draw_quick(sel); redraw = 0; }
      if (U) { sel = (sel + 9) % 10; redraw = 1; } if (D) { sel = (sel + 1) % 10; redraw = 1; }
      if (L || R || O) { int d = L ? -1 : 1, close = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); redraw = 1;
        static const float RF[3] = {1.f, .6f, .25f}, GV[3] = {14.f, 5.f, 25.f};
        if (sel == 0 && O) { repair(cx, cz, hx, hz); close = 1; }
        else if (sel == 1) turboT ^= 1;
        else if (sel == 2 && O) { for (int i = 0; i < (ntr >= 0 ? ntr + 11 : NC); i++) { n[i].vx += hx * 10.f; n[i].vz += hz * 10.f; } close = 1; }
        else if (sel == 3) { cv = (cv + 4 + d) % 4; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
        else if (sel == 4) { ce = (ce + 4 + d) % 4; engA = ENG[ce].a * VEH[cv].pw; engV = ENG[ce].v; }
        else if (sel == 5) { trl ^= 1; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
        else if (sel == 6) { ri = (ri + 3 + d) % 3; gripF = RF[ri]; } else if (sel == 7) { gi = (gi + 3 + d) % 3; G = GV[gi]; }
        else if (sel == 8) tscale = tscale > .5f ? .4f : 1.f;
        else if (sel == 9 && O) { startpos(); lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); close = 1; }
        if (close) { st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
      }
      if (B || bpress(k, A_QUICK)) { st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
      eadk_timing_msleep(30);
    } else {                                              // S_KEYS: key tester
      if (redraw) { clear(); txt("TEST DES TOUCHES", 90, 8, 1, C(230, 60, 50), C(15, 18, 28)); txt("Appuie sur des touches. Retour: sortir", 30, 40, 0, C(150, 160, 190), C(15, 18, 28)); redraw = 2; }
      if (B) { st = S_SET; sel = 5; redraw = 1; }
      else if (k != pk || redraw == 2) {
        eadk_display_push_rect_uniform((eadk_rect_t){0, 70, 320, 150}, C(15, 18, 28)); int line = 0, col = 0;
        for (int key = 0; key < 53; key++) if (((k >> key) & 1) && KN[key]) { txt(KN[key], 20 + col * 100, 80 + line * 20, 0, C(255, 210, 80), C(15, 18, 28)); if (++col == 3) { col = 0; line++; } }
        redraw = 3;
      }
      eadk_timing_msleep(30);
    }
    pk = k;
  }
}

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
static int structure, CXc = 80, curZ = -1, showTop = 1, crashI = 2, camInterior;
typedef struct { float x, z, h; } CityBlock;
static CityBlock cityBlocks[16];
static const int CRASHV[5] = {30, 50, 80, 110, 140};
static float camhx = 0, camhz = 1, camx, camy = 5, camz, oHood, oDoor, oTrunk, tHood, tDoor, tTrunk;
static uint64_t pk; static float wspin;

static int blendA;
static inline void px(int x, int y, uint16_t c) {
  if ((unsigned)x < LW && (unsigned)y < LH) { if (curZ >= 0 && zb[y * LW + x] < curZ) return;
    if (blendA) { uint16_t o = fb[y * LW + x]; int a = blendA, r = (((o >> 11) & 31) * (256 - a) + ((c >> 11) & 31) * a) >> 8, g = (((o >> 5) & 63) * (256 - a) + ((c >> 5) & 63) * a) >> 8, b = ((o & 31) * (256 - a) + (c & 31) * a) >> 8; c = (uint16_t)((r << 11) | (g << 5) | b); }
    fb[y * LW + x] = c; } }
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
    int x0 = (int)lo, x1 = (int)hi; if (x0 < 0) x0 = 0; if (x1 >= LW) x1 = LW - 1;
    for (int x = x0; x <= x1; x++) px(x, y, c);
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
      if (mapId == 3) {
        if (X < -CITY_X_LIMIT || X > CITY_X_LIMIT || Z < -CITY_Z_LIMIT || Z > CITY_Z_LIMIT) { r = 48 + ck * 5; g = 105 + ck * 7; b = 51; }
        else {
          int rx, rz0; city_nearest(X, Z, &rx, &rz0);
          float cxg, czg; city_center(rx, rz0, &cxg, &czg); float ax = X - cxg, az = Z - czg; if (ax < 0) ax = -ax; if (az < 0) az = -az;
          float roadX = cityPlan == 1 ? 5.2f : (cityPlan == 2 ? 3.1f : 3.8f), roadZ = cityPlan == 1 ? 4.8f : (cityPlan == 2 ? 3.1f : 3.8f);
          if (ax < roadX || az < roadZ) { r = 48; g = 53; b = 57; if ((ax < .12f || az < .12f) && (((int)(X + Z) / 4) & 1)) { r = 220; g = 190; b = 95; } }
          else if (ax < roadX + 1.5f || az < roadZ + 1.5f) { r = 118; g = 116; b = 105; }
          else { r = 73 + ck * 5; g = 103 + ck * 5; b = 74; if (ad < 18.f && st) { r = 52; g = 80; b = 58; } }
          int cross = (ax < roadX + 4.f && az < roadZ + 4.f) && (((int)(X * 1.8f) & 3) == 0 || ((int)(Z * 1.8f) & 3) == 0);
          if (cross) { r = 205; g = 200; b = 178; }
          if (cityPlan == 2 && ax < roadX && az < roadZ && (((int)(X * 3.f + Z * 2.f) & 15) == 0)) { r = 215; g = 195; b = 115; }
          if (weatherMode == 1 && (ax < roadX || az < roadZ)) { r = (r * 3) / 4; g = (g * 4) / 5; b = (b * 9) / 10; }
        }
      }
      else if (gfeat > .25f) { if (mapId == 1) { r = 175; g = 170; b = 150; } else { r = st ? 230 : 200; g = st ? 230 : 45; b = g; } }
      else if (mapId == 0 && ad < 6.f) { r = 70; g = 70; b = 76; if (Z > -1.5f && Z < 1.5f && X > TA - 6 && X < TA + 6) r = g = b = (((int)(X * 1.5f + 1000) + (int)(Z * 1.5f + 1000)) & 1) ? 240 : 25; else if (ad > 5.5f) r = g = b = 200; }
      else if (mapId == 0 && ad < 7.4f) { r = st ? 220 : 235; g = st ? 40 : 235; b = g; }
      else if (mapId == 1 && ad < 6.f) { r = 72; g = 72; b = 78; if (ad < .15f && (((int)(Z / 3.f)) & 1)) { r = 230; g = 210; b = 70; } if (ad > 5.6f) r = g = b = 200; }
      else if (mapId == 2 && ad < 12.f) { r = 95 + ck * 6; g = 95 + ck * 6; b = 100 + ck * 6; if (ad > 11.4f) r = g = b = 210; else if (ad < .2f && (((int)(Z / 3.f)) & 1)) r = g = b = 220; }
      else { r = 50 + ck * 8; g = 130 + (int)(h * 5) + ck * 10; b = 45; if (g > 200) g = 200; if (g < 60) g = 60; }
      if (weatherMode == 2) t += (1.f - t) * .48f;
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
    int x0 = (int)lo, x1 = (int)hi; if (x0 < 0) x0 = 0; if (x1 >= LW) x1 = LW - 1;
    for (int x = x0; x <= x1; x++) px(x, y, c);
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
// ---- car: curved patches skinned to the physics nodes, plus interior, engine, seats and trunk
static float PX[128], PY[128], PZ[128]; static int np;
static int vp(float x, float y, float z) { PX[np] = x; PY[np] = y; PZ[np] = z; return np++; }
static int rot(int p, int a, int b, float ang, int mode, float cx, float cz, float hx, float hz) {
  float ax = PX[b] - PX[a], ay = PY[b] - PY[a], az = PZ[b] - PZ[a], l = fsqrt(ax * ax + ay * ay + az * az) + 1e-4f; ax /= l; ay /= l; az /= l;
  float vx = PX[p] - PX[a], vy = PY[p] - PY[a], vz = PZ[p] - PZ[a], side0 = (PX[p] - cx) * hz - (PZ[p] - cz) * hx, best = -1e9f, rx = 0, ry = 0, rz = 0;
  for (int sg = -1; sg <= 1; sg += 2) {
    float c = fsin(ang + 1.5708f), s = fsin(ang) * sg, d = ax * vx + ay * vy + az * vz;
    float x = vx * c + (ay * vz - az * vy) * s + ax * d * (1 - c), y = vy * c + (az * vx - ax * vz) * s + ay * d * (1 - c), z = vz * c + (ax * vy - ay * vx) * s + az * d * (1 - c);
    float side = (PX[a] + x - cx) * hz - (PZ[a] + z - cz) * hx, sc = mode ? side * (side0 >= 0 ? 1 : -1) : y;
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
enum { M_BODY, M_GLASS, M_BLACK, M_CHROME, M_HEAD, M_TAIL, M_PLAS, M_INT, M_PLATE, M_INNER };
enum { K_FLOOR, K_ARCH, K_ROCK, K_WALL, K_PILL, K_FRONT, K_REAR, K_ROOF, K_WIN, K_HOOD, K_BUMF, K_BUMR, K_TRUNK, K_DOOR };
typedef struct { uint8_t n[4]; float s0, s1, t0, t1; uint8_t nu, nv, kind, grp, two, cls; float bulge; } Pat;
#define NPAT 26
static const Pat PAT[NPAT] = {
  {{0,1,3,2},0,1,0,1,1,1,K_FLOOR,255,0,0,0},
  {{0,2,8,6},0,.31f,0,1,3,2,K_ARCH,255,0,0,.05f},{{0,2,8,6},.69f,1,0,1,3,2,K_ARCH,255,0,0,.05f},{{0,2,8,6},.31f,.69f,0,.16f,2,1,K_ROCK,255,0,0,0},
  {{1,3,9,7},0,.31f,0,1,3,2,K_ARCH,255,0,0,.05f},{{1,3,9,7},.69f,1,0,1,3,2,K_ARCH,255,0,0,.05f},{{1,3,9,7},.31f,.69f,0,.16f,2,1,K_ROCK,255,0,0,0},
  {{0,2,8,6},.31f,.69f,.16f,1,1,1,K_WALL,255,0,1,0},{{20,18,12,10},0,1,0,1,1,1,K_WALL,255,0,1,0},
  {{1,3,9,7},.31f,.69f,.16f,1,1,1,K_WALL,255,0,1,0},{{21,19,13,11},0,1,0,1,1,1,K_WALL,255,0,1,0},
  {{20,18,12,10},0,.22f,0,1,1,2,K_PILL,255,0,0,0},{{21,19,13,11},0,.22f,0,1,1,2,K_PILL,255,0,0,0},
  {{2,3,9,8},0,1,0,1,8,4,K_FRONT,255,0,0,.03f},{{0,1,7,6},0,1,0,1,8,4,K_REAR,255,0,0,.03f},
  {{10,11,13,12},0,1,0,1,6,5,K_ROOF,255,0,0,.07f},{{20,21,11,10},0,1,0,1,7,5,K_WIN,255,0,0,.03f},
  {{22,23,24,25},0,1,0,1,6,5,K_HOOD,0,1,0,.07f},{{26,27,28,29},0,1,0,1,6,3,K_BUMF,1,1,0,.09f},{{30,31,32,33},0,1,0,1,6,3,K_BUMR,2,1,0,.09f},
  {{34,35,36,37},0,1,0,1,6,4,K_TRUNK,3,1,0,.05f},
  {{38,39,40,41},0,1,0,1,5,6,K_DOOR,4,1,0,.03f},{{42,43,44,45},0,1,0,1,5,6,K_DOOR,5,1,0,.03f},
  {{46,47,48,49},0,1,0,1,6,4,K_WIN,6,1,0,.04f},
  {{0,2,8,6},0,.01f,0,.01f,1,1,K_FLOOR,255,0,2,0},{{1,3,9,7},0,.01f,0,.01f,1,1,K_FLOOR,255,0,2,0}};   // last two unused placeholders
static int8_t psg[NPAT]; static int psgInit;
static int cellmat(const Pat *p, int i, int j) {
  switch (p->kind) {
    case K_FLOOR: return M_BLACK;
    case K_ARCH: return (i == 1 && j == 0) ? M_BLACK : M_BODY;
    case K_ROCK: return M_PLAS;
    case K_WALL: return M_INT;
    case K_FRONT: if (j == 0) return M_BLACK; if (j == 1) return (i == 0 || i == p->nu - 1) ? M_HEAD : ((i == p->nu / 2 || i == p->nu / 2 - 1) ? M_CHROME : M_BLACK); return M_BODY;
    case K_REAR: if (j == 0) return M_BLACK; if (j == 1) return (i == 0 || i == p->nu - 1) ? M_TAIL : ((i == p->nu / 2 || i == p->nu / 2 - 1) ? M_PLATE : M_BODY); return M_BODY;
    case K_WIN: return (i >= 1 && i <= p->nu - 2 && j >= 1 && j <= p->nv - 2) ? M_GLASS : M_BODY;
    case K_BUMF: return j == 0 ? ((i == 0 || i == p->nu - 1) ? M_HEAD : M_PLAS) : M_BODY;
    case K_BUMR: return j == 0 ? M_PLAS : ((i == 0 || i == p->nu - 1) ? M_TAIL : M_BODY);
    case K_DOOR: if (j <= 2) return (i == 1 && j == 2) ? M_CHROME : M_BODY; return (i >= 1 && i <= 3 && j <= 4) ? M_GLASS : M_BODY;
    default: return M_BODY;
  }
}
static uint16_t matcol(int m, float sh, float spec, float dk, const Veh *V) {
  float r, g, b; spec *= 150.f;
  switch (m) {
    case M_BODY: r = V->r * sh * dk + spec; g = V->g * sh * dk + spec; b = V->b * sh * dk + spec; break;
    case M_GLASS: r = 95 * (.7f + .3f * sh) + spec; g = 135 * (.7f + .3f * sh) + spec; b = 175 * (.7f + .3f * sh) + spec; break;
    case M_BLACK: r = g = b = 24 * sh; break;
    case M_CHROME: r = 190 * sh + spec; g = 190 * sh + spec; b = 200 * sh + spec; break;
    case M_HEAD: r = 255; g = 250; b = 200; break;
    case M_TAIL: r = brk > 0 ? 255 : 185; g = brk > 0 ? 40 : 18; b = brk > 0 ? 40 : 18; break;
    case M_PLAS: r = g = 44 * sh; b = 50 * sh; break;
    case M_INT: r = 46 * sh; g = 44 * sh; b = 52 * sh; break;
    case M_PLATE: r = 235 * sh; g = 235 * sh; b = 222 * sh; break;
    default: r = 76 * sh; g = 78 * sh; b = 84 * sh;
  }
  if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; return C((int)r, (int)g, (int)b);
}
// frames for solids: 0 = chassis (rigid fit), 1 = engine, 2/3 = seats
static float CFO[3], CFX[3], CFY[3], CFZ[3], bxm, bym, bzm;
static void fpt(int fr, float a, float b, float c, float *x, float *y, float *z) {
  if (fr == 0) { a -= bxm; b -= bym; c -= bzm; *x = CFO[0] + a * CFX[0] + b * CFY[0] + c * CFZ[0]; *y = CFO[1] + a * CFX[1] + b * CFY[1] + c * CFZ[1]; *z = CFO[2] + a * CFX[2] + b * CFY[2] + c * CFZ[2]; return; }
  int i0, i1, i2, i3 = -1; if (fr == 1) { i0 = 50; i1 = 51; i2 = 52; i3 = 53; } else if (fr == 2) { i0 = 54; i1 = 55; i2 = 56; } else { i0 = 57; i1 = 58; i2 = 59; }
  float ex = PX[i1] - PX[i0], ey = PY[i1] - PY[i0], ez = PZ[i1] - PZ[i0], fx = PX[i2] - PX[i0], fy = PY[i2] - PY[i0], fz = PZ[i2] - PZ[i0], ux, uy, uz;
  if (i3 >= 0) { ux = PX[i3] - PX[i0]; uy = PY[i3] - PY[i0]; uz = PZ[i3] - PZ[i0]; }
  else { ux = fy * ez - fz * ey; uy = fz * ex - fx * ez; uz = fx * ey - fy * ex; float l = fsqrt(ux * ux + uy * uy + uz * uz) + 1e-4f, w = fsqrt(ex * ex + ey * ey + ez * ez); ux *= w / l; uy *= w / l; uz *= w / l; }
  *x = PX[i0] + a * ex + b * ux + c * fx; *y = PY[i0] + a * ey + b * uy + c * fy; *z = PZ[i0] + a * ez + b * uz + c * fz;
}
typedef struct { uint8_t fr, cond; float a0, a1, b0, b1, c0, c1; uint8_t r, g, b; } Bx;
// cond: 0 always, 1 trunk visible, 2 engine bay visible, 3 engine visible, 4 turbo only
static const Bx BXS[] = {
  {0,0,-.82f,.82f,.7f,.98f,.5f,.8f,52,50,58},{0,0,-.62f,-.3f,.9f,.95f,.4f,.52f,30,30,34},{0,0,-.48f,-.44f,.78f,.9f,.45f,.6f,30,30,34},{0,0,-.1f,.1f,.45f,.72f,-.3f,.55f,55,55,62},
  {0,0,-.8f,.8f,.45f,.62f,-1.f,-.45f,70,68,76},{0,0,-.8f,.8f,.62f,1.1f,-1.08f,-.95f,70,68,76},{0,0,-.85f,.85f,.38f,.44f,-1.1f,.8f,48,46,52},
  {0,1,-.82f,.82f,.5f,.56f,-1.98f,-1.3f,60,58,60},{0,1,-.88f,-.82f,.56f,.95f,-1.98f,-1.3f,70,70,74},{0,1,.82f,.88f,.56f,.95f,-1.98f,-1.3f,70,70,74},{0,1,-.82f,.82f,.56f,.98f,-1.34f,-1.28f,66,64,70},
  {0,1,-.38f,.38f,.56f,.7f,-1.85f,-1.45f,30,30,34},{0,1,.1f,.72f,.56f,.86f,-1.9f,-1.55f,140,64,40},{0,1,-.82f,.82f,.56f,.95f,-1.99f,-1.93f,80,80,86},
  {0,2,-.85f,.85f,.36f,.4f,.8f,2.05f,50,50,55},{0,2,-.9f,-.85f,.4f,.88f,.8f,2.05f,64,64,70},{0,2,.85f,.9f,.4f,.88f,.8f,2.05f,64,64,70},{0,2,-.85f,.85f,.4f,.95f,.76f,.82f,58,58,64},
  {0,2,-.7f,.7f,.42f,.85f,1.95f,2.02f,35,35,40},{0,2,.45f,.8f,.4f,.62f,1.55f,1.8f,25,30,60},{0,2,-.8f,-.6f,.4f,.55f,1.3f,1.5f,50,90,200},
  {1,3,0,1,0,.62f,0,1,0,0,0},{1,3,.08f,.92f,.62f,.82f,.1f,.9f,170,170,180},{1,3,.3f,.7f,.82f,1.f,.2f,.75f,95,95,100},{1,3,.15f,.4f,.15f,.4f,1.f,1.1f,200,200,205},
  {1,3,.6f,.9f,.1f,.4f,1.f,1.08f,40,40,44},{1,3,1.f,1.1f,.1f,.5f,.1f,.9f,150,80,40},{1,4,-.4f,0,.15f,.5f,.2f,.6f,230,90,40},
  {1,3,.1f,.43f,.2f,.58f,.15f,.88f,125,130,138},{1,3,.57f,.9f,.2f,.58f,.15f,.88f,125,130,138},
  {1,3,.34f,.66f,.83f,.94f,.18f,.78f,78,84,94},{1,3,.38f,.62f,.08f,.18f,.72f,.98f,45,48,54},
  {1,3,.05f,.14f,.14f,.28f,.22f,.82f,205,135,55},{1,3,.86f,.95f,.14f,.28f,.22f,.82f,205,135,55},
  {2,0,0,1,0,.28f,0,1,45,45,55},{3,0,0,1,0,.28f,0,1,45,45,55},{2,0,0,1,.28f,1.35f,-.35f,.1f,50,50,60},{3,0,0,1,.28f,1.35f,-.35f,.1f,50,50,60},{2,0,.25f,.75f,1.35f,1.7f,-.3f,.05f,45,45,55},{3,0,.25f,.75f,1.35f,1.7f,-.3f,.05f,45,45,55}};
#define NBX ((int)(sizeof(BXS) / sizeof(BXS[0])))
static float gx[64], gy[64], gz3[64], g3x[64], g3y[64], g3z[64];
static const uint8_t HULLS = 0;
// draw one patch; pass 0 = glass of far-side patches, 2 = opaque cells, 3 = glass cells of near patches
#ifndef DBG_HIDE
#define DBG_HIDE 0
#endif
static void drawpat(int pi, const Pat *p, int pass, int facing, float cxm) {
  if (((DBG_HIDE & 1) && PAT[pi].kind == K_HOOD) || ((DBG_HIDE & 2) && PAT[pi].kind == K_TRUNK) || ((DBG_HIDE & 4) && PAT[pi].kind == K_DOOR) || ((DBG_HIDE & 8) && (PAT[pi].kind == K_ROOF || PAT[pi].kind == K_WIN))) return;
  int nu = p->nu, nv = p->nv; const Veh *V = &VEH[cv]; float dk = 1.f - dmg * .006f;
  float c[4][3]; for (int k = 0; k < 4; k++) { c[k][0] = PX[p->n[k]]; c[k][1] = PY[p->n[k]]; c[k][2] = PZ[p->n[k]]; }
  float ax = c[1][0] - c[0][0], ay = c[1][1] - c[0][1], az = c[1][2] - c[0][2], bx = c[3][0] - c[0][0], by = c[3][1] - c[0][1], bz = c[3][2] - c[0][2]; uint8_t visible[64];
  float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx, nl = fsqrt(nx * nx + ny * ny + nz * nz) + 1e-4f, sg = psg[pi]; nx *= sg / nl; ny *= sg / nl; nz *= sg / nl;
  for (int j = 0; j <= nv; j++) for (int i = 0; i <= nu; i++) {
    float s = p->s0 + (p->s1 - p->s0) * i / nu, t = p->t0 + (p->t1 - p->t0) * j / nv, w = p->bulge * 16.f * s * (1 - s) * t * (1 - t); if (w < 0) w = 0;
    float x = (1 - s) * (1 - t) * c[0][0] + s * (1 - t) * c[1][0] + s * t * c[2][0] + (1 - s) * t * c[3][0] + nx * w, y = (1 - s) * (1 - t) * c[0][1] + s * (1 - t) * c[1][1] + s * t * c[2][1] + (1 - s) * t * c[3][1] + ny * w,
          z = (1 - s) * (1 - t) * c[0][2] + s * (1 - t) * c[1][2] + s * t * c[2][2] + (1 - s) * t * c[3][2] + nz * w; int k = j * (nu + 1) + i;
    g3x[k] = x; g3y[k] = y; g3z[k] = z; gz3[k] = (x - camx) * camhx + (z - camz) * camhz;
    visible[k] = gz3[k] >= .401f; if (visible[k]) proj(x, y, z, &gx[k], &gy[k]);
  }
  int i0 = gz3[0] > gz3[nu] ? 0 : nu - 1, di = i0 == 0 ? 1 : -1, j0 = gz3[0] > gz3[nv * (nu + 1)] ? 0 : nv - 1, dj = j0 == 0 ? 1 : -1;
  for (int jj = 0, j = j0; jj < nv; jj++, j += dj) for (int ii = 0, i = i0; ii < nu; ii++, i += di) {
    int m = cellmat(p, i, j), glass = m == M_GLASS;
    if (pass == 2 && glass) continue; if (pass != 2 && !glass) continue;
    int k0 = j * (nu + 1) + i, k1 = k0 + 1, k2 = k1 + nu + 1, k3 = k0 + nu + 1;
    float ex = g3x[k1] - g3x[k0], ey = g3y[k1] - g3y[k0], ez = g3z[k1] - g3z[k0], fx = g3x[k3] - g3x[k0], fy = g3y[k3] - g3y[k0], fz = g3z[k3] - g3z[k0];
    float qx = ey * fz - ez * fy, qy = ez * fx - ex * fz, qz = ex * fy - ey * fx, ql = fsqrt(qx * qx + qy * qy + qz * qz) + 1e-4f;
    float lit = (.3f * qx + .8f * qy + .5f * qz) / ql / .97f; if (lit < 0) lit = -lit;
    float sh = .45f + .55f * lit, spec = lit > .86f ? (lit - .86f) * 7.f : 0; if (spec > 1) spec = 1;
    if (m == M_BODY) { unsigned tex = (unsigned)(pi * 73856093u) ^ (unsigned)(i * 19349663u) ^ (unsigned)(j * 83492791u); tex ^= tex >> 13; sh *= 1.f + ((int)(tex & 7) - 3) * .012f; }
    int ids[4] = {k0, k1, k2, k3}, count = 0; float wx[8], wy[8], wz[8], wd[8], xs[8], ys[8];
    for (int e = 0; e < 4; e++) {
      int a = ids[e], b = ids[(e + 1) & 3]; float da = gz3[a], db = gz3[b]; int ina = visible[a], inb = visible[b];
      if (ina && inb) { wx[count] = g3x[b]; wy[count] = g3y[b]; wz[count] = g3z[b]; wd[count++] = db; }
      else if (ina != inb) { float t = (.401f - da) / (db - da); wx[count] = g3x[a] + (g3x[b] - g3x[a]) * t; wy[count] = g3y[a] + (g3y[b] - g3y[a]) * t; wz[count] = g3z[a] + (g3z[b] - g3z[a]) * t; wd[count++] = .401f;
        if (!ina) { wx[count] = g3x[b]; wy[count] = g3y[b]; wz[count] = g3z[b]; wd[count++] = db; } }
    }
    if (count < 3) continue;
    for (int v = 0; v < count; v++) { float dx = wx[v] - camx, dz = wz[v] - camz; xs[v] = CXc + (dx * camhz - dz * camhx) / wd[v] * FOC; ys[v] = HOR - (wy[v] - camy) / wd[v] * FOC; }
    if (!facing && p->two && !glass) m = M_INNER;
    uint16_t col = matcol(m, sh, (m == M_BODY || m == M_GLASS || m == M_CHROME) ? spec : 0, dk, V);
    if (glass) { blendA = 150; polyn(xs, ys, count, col); blendA = 0; } else polyn(xs, ys, count, col);
  }
  if (pass != 2 && p->kind == K_WIN && p->grp == 6 && (pAtt[6] < pAtt0[6] || dmg > 40)) {   // cracked windshield
    int lx[8] = {0, nu, 0, nu, nu / 2, nu / 2, 0, nu}, ly[8] = {0, 0, nv, nv, 0, nv, nv / 2, nv / 2}; int ck = (nv / 2) * (nu + 1) + nu / 2;
    for (int q = 0; q < 8; q++) { int k = ly[q] * (nu + 1) + lx[q]; if (visible[ck] && visible[k]) line((int)gx[ck], (int)gy[ck], (int)gx[k], (int)gy[k], C(235, 240, 245)); }
  }
}
static void drawbx(int bi) {
  const Bx *B = &BXS[bi]; float x[8], y[8], z[8], zz[8]; int any = 0;
  for (int i = 0; i < 8; i++) { fpt(B->fr, (i & 1) ? B->a1 : B->a0, ((i >> 1) & 1) ? B->b1 : B->b0, ((i >> 2) & 1) ? B->c1 : B->c0, &x[i], &y[i], &z[i]);
    float dx = x[i] - camx, dz = z[i] - camz; zz[i] = dx * camhx + dz * camhz; if (zz[i] >= .401f) any = 1; }
  if (!any) return;
  static const uint8_t FB[6][4] = {{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}};
  int id[6]; float dp[6]; for (int f = 0; f < 6; f++) { id[f] = f; dp[f] = (zz[FB[f][0]] + zz[FB[f][1]] + zz[FB[f][2]] + zz[FB[f][3]]) * .25f; }
  for (int i = 1; i < 6; i++) { int a = id[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { id[j + 1] = id[j]; dp[j + 1] = dp[j]; j--; } id[j + 1] = a; dp[j + 1] = v; }
  int r = B->r, g = B->g, b = B->b; float kk = (B->cond >= 3) ? 1.f : 1.9f;
  if (B->fr == 1 && B->cond == 3 && B->a1 == 1 && B->b1 < .7f) { r = ENG[ce].r; g = ENG[ce].g; b = ENG[ce].b; }
  r = (int)(r * kk); g = (int)(g * kk); b = (int)(b * kk); if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
  for (int k = 0; k < 6; k++) { const uint8_t *q = FB[id[k]]; int count = 0; float wx[8], wy[8], wz[8], wd[8], xs[8], ys[8];
    for (int e = 0; e < 4; e++) { int a = q[e], b0 = q[(e + 1) & 3], ina = zz[a] >= .401f, inb = zz[b0] >= .401f;
      if (ina && inb) { wx[count] = x[b0]; wy[count] = y[b0]; wz[count] = z[b0]; wd[count++] = zz[b0]; }
      else if (ina != inb) { float t = (.401f - zz[a]) / (zz[b0] - zz[a]); wx[count] = x[a] + (x[b0] - x[a]) * t; wy[count] = y[a] + (y[b0] - y[a]) * t; wz[count] = z[a] + (z[b0] - z[a]) * t; wd[count++] = .401f;
        if (!ina) { wx[count] = x[b0]; wy[count] = y[b0]; wz[count] = z[b0]; wd[count++] = zz[b0]; } } }
    if (count < 3) continue;
    for (int v = 0; v < count; v++) { float dx = wx[v] - camx, dz = wz[v] - camz; xs[v] = CXc + (dx * camhz - dz * camhx) / wd[v] * FOC; ys[v] = HOR - (wy[v] - camy) / wd[v] * FOC; }
    float sh = shade(x, y, z, q[0], q[1], q[3]); sh = .6f + .8f * (sh - .5f); polyn(xs, ys, count, C((int)(r * sh), (int)(g * sh), (int)(b * sh)));
    uint16_t ec = C((int)(r * sh * .35f), (int)(g * sh * .35f), (int)(b * sh * .35f)); for (int i = 0; i < count; i++) line((int)xs[i], (int)ys[i], (int)xs[(i + 1) % count], (int)ys[(i + 1) % count], ec); }
}
static int localproj(float x, float y, float z, float *sx, float *sy) {
  float wx, wy, wz; fpt(0, x, y, z, &wx, &wy, &wz); return proj(wx, wy, wz, sx, sy);
}
static void line3(float x0, float y0, float z0, float x1, float y1, float z1, uint16_t col) {
  float sx0, sy0, sx1, sy1; if (localproj(x0, y0, z0, &sx0, &sy0) && localproj(x1, y1, z1, &sx1, &sy1)) line((int)sx0, (int)sy0, (int)sx1, (int)sy1, col);
}
static void ring3(float x, float y, float z, float r, uint16_t col) {
  float sx0, sy0; int was = localproj(x + r, y, z, &sx0, &sy0);
  for (int i = 1; i <= 16; i++) { float th = i * .3926991f, px1 = x + r * fsin(th + 1.5708f), py1 = y + r * fsin(th), sx1, sy1; int valid = localproj(px1, py1, z, &sx1, &sy1);
    if (was && valid) line((int)sx0, (int)sy0, (int)sx1, (int)sy1, col);
    if (valid) { sx0 = sx1; sy0 = sy1; } was = valid;
  }
}
static void dial3(float x, float y, float z, float r, float value, uint16_t needle) {
  float sx, sy, wx, wy, wz; fpt(0, x, y, z, &wx, &wy, &wz); if (!proj(wx, wy, wz, &sx, &sy)) return;
  float d = (wx - camx) * camhx + (wz - camz) * camhz; int rr = (int)(r * FOC / d * .72f); if (rr < 2) rr = 2; if (rr > 12) rr = 12;
  disc((int)sx, (int)sy, rr, C(14, 20, 25)); ring3(x, y, z, r, C(145, 155, 160));
  for (int i = 0; i <= 5; i++) { float th = 3.75f + i * .47f, c = fsin(th + 1.5708f), sn = fsin(th); line3(x + r * .68f * c, y + r * .68f * sn, z, x + r * c, y + r * sn, z, C(180, 190, 190)); }
  if (value < 0) value = 0;
  if (value > 1) value = 1;
  float th = 3.75f + value * 2.35f;
  line3(x, y, z - .003f, x + r * .68f * fsin(th + 1.5708f), y + r * .68f * fsin(th), z - .003f, needle);
}
static void draw_cockpit(void) {
  float hx, hz, speed = 0; heading(&hx, &hz); for (int i = 0; i < 20; i++) speed += (n[i].vx * hx + n[i].vz * hz) / 20.f; if (speed < 0) speed = -speed;
  uint16_t trim = C(34, 40, 45), metal = C(115, 125, 130);
  line3(-.78f, 1.02f, .82f, .78f, 1.02f, .82f, trim);
  line3(-.78f, 1.02f, .82f, -.78f, 1.12f, .78f, metal); line3(.78f, 1.02f, .82f, .78f, 1.12f, .78f, metal);
  for (int v = 0; v < 3; v++) line3(.10f, 1.04f + v * .025f, .84f, .30f, 1.04f + v * .025f, .84f, C(85, 95, 100));
  ring3(-.43f, 1.13f, .58f, .075f, C(22, 25, 28));
  line3(-.43f, 1.13f, .58f, -.43f, 1.20f, .58f, metal);
  line3(-.43f, 1.13f, .58f, -.49f, 1.09f, .58f, metal); line3(-.43f, 1.13f, .58f, -.37f, 1.09f, .58f, metal);
  dial3(-.22f, 1.17f, .82f, .052f, engineRpm / 6500.f, C(245, 80, 55));
  dial3(-.06f, 1.17f, .82f, .052f, speed / (engV + .01f), C(240, 205, 85));
  line3(.40f, 1.09f, .84f, .72f, 1.09f, .84f, metal); line3(.72f, 1.09f, .84f, .72f, 1.25f, .84f, metal);
  line3(.72f, 1.25f, .84f, .40f, 1.25f, .84f, metal); line3(.40f, 1.25f, .84f, .40f, 1.09f, .84f, metal);
  line3(.44f, 1.13f, .835f, .67f, 1.13f, .835f, C(60, 170, 150)); line3(.44f, 1.17f, .835f, .62f, 1.17f, .835f, C(60, 170, 150));
}
static void car(void) {
  float cxm = 0, czm = 0; np = NC; curZ = -1; blendA = 0;
  if (!psgInit) { psgInit = 1; float cx = 0, cy = 0, cz = 0; for (int i = 0; i < 22; i++) { cx += P[i][0] / 22; cy += P[i][1] / 22; cz += P[i][2] / 22; }
    for (int k = 0; k < NPAT; k++) { const float *a = P[PAT[k].n[0]], *b = P[PAT[k].n[1]], *d = P[PAT[k].n[3]], *e = P[PAT[k].n[2]];
      float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], vx = d[0] - a[0], vy = d[1] - a[1], vz = d[2] - a[2], nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
      float mx = (a[0] + b[0] + d[0] + e[0]) / 4 - cx, my = (a[1] + b[1] + d[1] + e[1]) / 4 - cy, mz = (a[2] + b[2] + d[2] + e[2]) / 4 - cz; psg[k] = (nx * mx + ny * my + nz * mz) >= 0 ? 1 : -1; } }
  for (int i = 0; i < NC; i++) { PX[i] = n[i].x; PY[i] = n[i].y; PZ[i] = n[i].z; } for (int i = 0; i < 22; i++) { cxm += n[i].x / 22; czm += n[i].z / 22; }
  // chassis frame: least-squares fit of the structure nodes against their rest coordinates
  { float wx = 0, wy = 0, wz = 0, mx = 0, my = 0, mz = 0; int cnt = 0;
    for (int i = 0; i < 22; i++) { if (i >= 14 && i < 18) continue; wx += PX[i]; wy += PY[i]; wz += PZ[i]; mx += P[i][0]; my += P[i][1]; mz += P[i][2]; cnt++; }
    wx /= cnt; wy /= cnt; wz /= cnt; bxm = mx / cnt; bym = my / cnt; bzm = mz / cnt; float sxx = 0, syy = 0, szz = 0, dx[3] = {0, 0, 0}, dy[3] = {0, 0, 0}, dz[3] = {0, 0, 0};
    for (int i = 0; i < 22; i++) { if (i >= 14 && i < 18) continue; float a = P[i][0] - bxm, b = P[i][1] - bym, c = P[i][2] - bzm, ex = PX[i] - wx, ey = PY[i] - wy, ez = PZ[i] - wz;
      sxx += a * a; syy += b * b; szz += c * c; dx[0] += a * ex; dx[1] += a * ey; dx[2] += a * ez; dy[0] += b * ex; dy[1] += b * ey; dy[2] += b * ez; dz[0] += c * ex; dz[1] += c * ey; dz[2] += c * ez; }
    for (int k = 0; k < 3; k++) { CFX[k] = dx[k] / sxx; CFY[k] = dy[k] / syy; CFZ[k] = dz[k] / szz; } CFO[0] = wx; CFO[1] = wy; CFO[2] = wz; }
  int ri[64]; for (int i = 0; i < 60; i++) ri[i] = i;
  float oh = oHood, ot = oTrunk, od = oDoor, rhx, rhz; heading(&rhx, &rhz);
  if (oh > .02f && pAtt[0] > 0) { ri[22] = rot(22, 25, 24, oh * 1.05f, 0, cxm, czm, rhx, rhz); ri[23] = rot(23, 25, 24, oh * 1.05f, 0, cxm, czm, rhx, rhz); }
  if (ot > .02f && pAtt[3] > 0) { ri[34] = rot(34, 37, 36, ot * 1.2f, 0, cxm, czm, rhx, rhz); ri[35] = rot(35, 37, 36, ot * 1.2f, 0, cxm, czm, rhx, rhz); }
  if (od > .02f && pAtt[4] > 0) { ri[38] = rot(38, 39, 40, od * 1.1f, 1, cxm, czm, rhx, rhz); ri[41] = rot(41, 39, 40, od * 1.1f, 1, cxm, czm, rhx, rhz); }
  if (od > .02f && pAtt[5] > 0) { ri[42] = rot(42, 43, 44, od * 1.1f, 1, cxm, czm, rhx, rhz); ri[45] = rot(45, 43, 44, od * 1.1f, 1, cxm, czm, rhx, rhz); }
  static Pat PL[NPAT]; for (int k = 0; k < NPAT; k++) { PL[k] = PAT[k]; for (int q = 0; q < 4; q++) PL[k].n[q] = ri[PAT[k].n[q]]; }
  if (structure) {
    float dummy = 0; (void)dummy;
    for (int i = 0; i < nbc; i++) if (bm[i].f != 2 && bm[i].o == 0 && bm[i].f != 3) { float d = (bm[i].l0 - bm[i].lr) / bm[i].lr; d = (d < 0 ? -d : d) * 12; if (d > 1) d = 1; float x0, y0, x1, y1;
      if (proj(PX[bm[i].a], PY[bm[i].a], PZ[bm[i].a], &x0, &y0) && proj(PX[bm[i].b], PY[bm[i].b], PZ[bm[i].b], &x1, &y1)) line((int)x0, (int)y0, (int)x1, (int)y1, C(255, 255 - (int)(d * 230), 255 - (int)(d * 255))); }
    float hx_, hz_; heading(&hx_, &hz_); for (int w = 14; w < 18; w++) { float ax = hz_, az = -hx_; wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(WHL[cw].cr, WHL[cw].cg, WHL[cw].cb)); }
    return;
  }
  float hx_, hz_, ccx = 0, ccz = 0; heading(&hx_, &hz_); for (int i = 0; i < 22; i++) { ccx += n[i].x / 22; ccz += n[i].z / 22; }
  float camSide = (camx - ccx) * hz_ - (camz - ccz) * hx_;
  // facing of every patch (uses the open/closed positions)
  uint8_t fc[NPAT]; float pd[NPAT];
  for (int k = 0; k < NPAT; k++) { const Pat *p = &PL[k]; float c0[3] = {PX[p->n[0]], PY[p->n[0]], PZ[p->n[0]]};
    float ax = PX[p->n[1]] - c0[0], ay = PY[p->n[1]] - c0[1], az = PZ[p->n[1]] - c0[2], bx = PX[p->n[3]] - c0[0], by = PY[p->n[3]] - c0[1], bz = PZ[p->n[3]] - c0[2];
    float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx, mx = (PX[p->n[0]] + PX[p->n[1]] + PX[p->n[2]] + PX[p->n[3]]) / 4, my = (PY[p->n[0]] + PY[p->n[1]] + PY[p->n[2]] + PY[p->n[3]]) / 4, mz = (PZ[p->n[0]] + PZ[p->n[1]] + PZ[p->n[2]] + PZ[p->n[3]]) / 4;
    fc[k] = (nx * (camx - mx) + ny * (camy - my) + nz * (camz - mz)) * psg[k] > 0; pd[k] = (mx - camx) * camhx + (mz - camz) * camhz; }
  // far-side glass first
  for (int k = 0; k < NPAT; k++) if (!fc[k] && PL[k].cls == 0 && (PL[k].kind == K_WIN || PL[k].kind == K_DOOR)) drawpat(k, &PL[k], 3, 0, cxm);
  // interior pass: inner walls, solids (seats, dashboard, trunk, engine bay, engine) and far-side wheels, far to near
  int it[80]; float dp[80]; int ni = 0; int bayOpen = oh > .05f || pAtt[0] <= 0, trunkOpen = ot > .05f || pAtt[3] <= 0;
  for (int k = 0; k < NPAT; k++) if (PL[k].cls == 1 && !fc[k]) { it[ni] = k; dp[ni++] = pd[k]; }
  for (int b = 0; b < NBX; b++) { int cd = BXS[b].cond; if ((cd == 1 && !trunkOpen) || (cd == 2 && !bayOpen) || (cd == 3 && !(bayOpen || pAtt[7] <= 0)) || (cd == 4 && (ce != 3 || !(bayOpen || pAtt[7] <= 0)))) continue;
    float x, y, z; fpt(BXS[b].fr, (BXS[b].a0 + BXS[b].a1) / 2, (BXS[b].b0 + BXS[b].b1) / 2, (BXS[b].c0 + BXS[b].c1) / 2, &x, &y, &z); it[ni] = 100 + b; dp[ni++] = (x - camx) * camhx + (z - camz) * camhz; }
  for (int w = 14; w < 18; w++) { int sw = (w & 1) ? 1 : -1; if (sw * camSide > 0) continue; it[ni] = 200 + w; dp[ni++] = (n[w].x - camx) * camhx + (n[w].z - camz) * camhz; }
  for (int i = 1; i < ni; i++) { int a = it[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { it[j + 1] = it[j]; dp[j + 1] = dp[j]; j--; } it[j + 1] = a; dp[j + 1] = v; }
  const Whl *W = &WHL[cw];
  for (int q = 0; q < ni; q++) { int a = it[q];
    if (a >= 200) { int w = a - 200; float ax = hz_, az = -hx_; if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); } wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); }
    else if (a >= 100) drawbx(a - 100); else drawpat(a, &PL[a], 2, 0, cxm); }
  if (camInterior) draw_cockpit();
  // opaque outer patches facing the camera (parts also show their inside when turned away)
  int ok[NPAT], no = 0; float od2[NPAT];
  for (int k = 0; k < NPAT; k++) if (PL[k].cls == 0 && (fc[k] || PL[k].two)) { ok[no] = k; od2[no++] = pd[k]; }
  for (int i = 1; i < no; i++) { int a = ok[i]; float v = od2[i]; int j = i - 1; while (j >= 0 && od2[j] < v) { ok[j + 1] = ok[j]; od2[j + 1] = od2[j]; j--; } ok[j + 1] = a; od2[j + 1] = v; }
  for (int q = 0; q < no; q++) drawpat(ok[q], &PL[ok[q]], 2, fc[ok[q]], cxm);
  for (int w = 14; w < 18; w++) { int sw = (w & 1) ? 1 : -1; if (sw * camSide <= 0) continue; float ax = hz_, az = -hx_; if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); } wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); }
  for (int q = 0; q < no; q++) { int k = ok[q]; if (fc[k] && (PL[k].kind == K_WIN || PL[k].kind == K_DOOR)) drawpat(k, &PL[k], 3, 1, cxm); }
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
static void draw_prism3(const float *x, const float *y, const float *z, int cr, int cg, int cb) {
  static const uint8_t F[6][4] = {{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}};
  float dep[6]; int order[6];
  for (int f = 0; f < 6; f++) { order[f] = f; dep[f] = 0; for (int q = 0; q < 4; q++) { int v = F[f][q]; dep[f] += (x[v] - camx) * camhx + (z[v] - camz) * camhz; } dep[f] *= .25f; }
  for (int i = 1; i < 6; i++) { int id = order[i], j = i - 1; float key = dep[i]; while (j >= 0 && dep[j] < key) { order[j + 1] = order[j]; dep[j + 1] = dep[j]; j--; } order[j + 1] = id; dep[j + 1] = key; }
  for (int f = 0; f < 6; f++) { const uint8_t *q = F[order[f]]; float wx[8], wy[8], wz[8], wd[8], sx[8], sy[8]; int count = 0;
    for (int e = 0; e < 4; e++) { int a = q[e], b = q[(e + 1) & 3]; float da = (x[a] - camx) * camhx + (z[a] - camz) * camhz, db = (x[b] - camx) * camhx + (z[b] - camz) * camhz; int ina = da >= .401f, inb = db >= .401f;
      if (ina && inb) { wx[count] = x[b]; wy[count] = y[b]; wz[count] = z[b]; wd[count++] = db; }
      else if (ina != inb) { float t = (.401f - da) / (db - da); wx[count] = x[a] + (x[b] - x[a]) * t; wy[count] = y[a] + (y[b] - y[a]) * t; wz[count] = z[a] + (z[b] - z[a]) * t; wd[count++] = .401f;
        if (!ina) { wx[count] = x[b]; wy[count] = y[b]; wz[count] = z[b]; wd[count++] = db; } } }
    if (count < 3) continue;
    for (int v = 0; v < count; v++) { float dx = wx[v] - camx, dz = wz[v] - camz; sx[v] = CXc + (dx * camhz - dz * camhx) / wd[v] * FOC; sy[v] = HOR - (wy[v] - camy) / wd[v] * FOC; }
    float light[6] = {.72f, 1.08f, .84f, .92f, .68f, .78f}, sh = light[order[f]];
    polyn(sx, sy, count, C((int)(cr * sh), (int)(cg * sh), (int)(cb * sh)));
  }
}
static void draw_city_block(int index) {
  CityBlock *B = &cityBlocks[index]; float x[8], y[8], z[8];
  for (int i = 0; i < 8; i++) { x[i] = B->x + ((i & 1) ? 8.5f : -8.5f); y[i] = (i & 2) ? B->h : 0; z[i] = B->z + ((i & 4) ? 11.f : -11.f); }
  int tone = (index * 17 + (int)(B->x + B->z + 1000)) & 1; draw_prism3(x, y, z, tone ? 118 : 145, tone ? 128 : 150, tone ? 135 : 158);
  uint16_t glass = C(42, 74, 88), edge = C(75, 83, 88);
  for (int floor = 1; floor * 3.f < B->h; floor++) {
    float yy = floor * 3.f;
    for (int col = 0; col < 4; col++) {
      float a = B->x - 7.f + col * 4.f, b = a + 2.25f, c = B->z - 8.f + col * 4.f, d = c + 2.25f, y1 = yy + 1.45f;
      float wx[4] = {a, b, b, a}, wy[4] = {yy, yy, y1, y1}, sx[4], sy[4];
      float wz[4] = {B->z + 11.02f, B->z + 11.02f, B->z + 11.02f, B->z + 11.02f}; int ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(wx[q], wy[q], wz[q], &sx[q], &sy[q])) ok = 0;
      if (ok) polyn(sx, sy, 4, glass);
      wx[0] = b; wx[1] = a; wx[2] = a; wx[3] = b; wz[0] = wz[1] = wz[2] = wz[3] = B->z - 11.02f; ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(wx[q], wy[q], wz[q], &sx[q], &sy[q])) ok = 0;
      if (ok) polyn(sx, sy, 4, glass);
      float sideZ[4] = {c, d, d, c}, sideX[4] = {B->x + 8.52f, B->x + 8.52f, B->x + 8.52f, B->x + 8.52f}; ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(sideX[q], wy[q], sideZ[q], &sx[q], &sy[q])) ok = 0;
      if (ok) polyn(sx, sy, 4, glass);
      sideX[0] = sideX[1] = sideX[2] = sideX[3] = B->x - 8.52f; sideZ[0] = d; sideZ[1] = c; sideZ[2] = c; sideZ[3] = d; ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(sideX[q], wy[q], sideZ[q], &sx[q], &sy[q])) ok = 0;
      if (ok) polyn(sx, sy, 4, glass);
    }
    float ax0, ay0, ax1, ay1; if (proj(B->x - 8.49f, yy, B->z, &ax0, &ay0) && proj(B->x - 8.49f, yy + .04f, B->z, &ax1, &ay1)) line((int)ax0, (int)ay0, (int)ax1, (int)ay1, edge);
  }
}
static void city_line(float x0, float y0, float z0, float x1, float y1, float z1, uint16_t color) {
  float sx0, sy0, sx1, sy1;
  if (proj(x0, y0, z0, &sx0, &sy0) && proj(x1, y1, z1, &sx1, &sy1)) line((int)sx0, (int)sy0, (int)sx1, (int)sy1, color);
}
static void city_lamp(float x, float y, float z, float radius, uint16_t color) {
  float sx, sy; if (!proj(x, y, z, &sx, &sy)) return;
  float depth = (x - camx) * camhx + (z - camz) * camhz;
  int r = (int)(radius * FOC / depth); if (r < 1) r = 1; if (r > 5) r = 5;
  disc((int)sx, (int)sy, r, color);
}
static void city_box(float x, float z, float hx, float hz, float y0, float y1, int r, int g, int b) {
  float px[8], py[8], pz[8];
  for (int i = 0; i < 8; i++) { px[i] = x + ((i & 1) ? hx : -hx); py[i] = (i & 2) ? y1 : y0; pz[i] = z + ((i & 4) ? hz : -hz); }
  draw_prism3(px, py, pz, r, g, b);
}
static void city_manhole(float x, float z) {
  float sx[12], sy[12];
  for (int i = 0; i < 12; i++) { float a = i * .523599f, wx = x + .58f * fsin(a + 1.5708f), wz = z + .58f * fsin(a); if (!proj(wx, .025f, wz, &sx[i], &sy[i])) return; }
  polyn(sx, sy, 12, C(53, 57, 58));
  for (int i = 0; i < 12; i++) line((int)sx[i], (int)sy[i], (int)sx[(i + 1) % 12], (int)sy[(i + 1) % 12], C(115, 119, 116));
  for (int i = 0; i < 3; i++) { float a = i * 1.0472f, ax = x + .34f * fsin(a + 1.5708f), az = z + .34f * fsin(a), bx = x - .34f * fsin(a + 1.5708f), bz = z - .34f * fsin(a); city_line(ax, .03f, az, bx, .03f, bz, C(82, 87, 86)); }
}
static void draw_city_details(int index) {
  CityBlock *B = &cityBlocks[index]; float x = B->x + 14.f, z = B->z + 18.f;
  float depth = (x - camx) * camhx + (z - camz) * camhz, side = (x - camx) * camhz - (z - camz) * camhx;
  if (depth < 2.f || depth > zmax - 2.f || side < -depth * 1.25f || side > depth * 1.25f) return;
  uint16_t pole = C(58, 63, 65);
  city_line(x + 4.5f, 0, z + 4.5f, x + 4.5f, 4.9f, z + 4.5f, pole);
  city_line(x + 4.5f, 4.7f, z + 4.5f, x + 1.2f, 4.7f, z + 4.5f, pole);
  city_box(x + 1.2f, z + 4.5f, .28f, .18f, 3.45f, 4.82f, 30, 34, 36);
  int signal = ((int)(signalClock / 4.f) + index) % 3;
  city_lamp(x + 1.2f, 4.48f, z + 4.28f, .13f, signal == 0 ? C(235, 55, 45) : C(70, 35, 35));
  city_lamp(x + 1.2f, 4.12f, z + 4.28f, .13f, signal == 1 ? C(245, 205, 45) : C(70, 58, 30));
  city_lamp(x + 1.2f, 3.76f, z + 4.28f, .13f, signal == 2 ? C(65, 225, 80) : C(30, 65, 38));
  city_line(x - 4.7f, 0, z - 4.6f, x - 4.7f, 5.7f, z - 4.6f, pole);
  city_line(x - 4.7f, 5.65f, z - 4.6f, x - 1.9f, 5.65f, z - 4.6f, pole);
  city_lamp(x - 1.9f, 5.58f, z - 4.6f, .24f, C(255, 232, 170));
  city_line(x - 4.3f, 0, z + 4.8f, x - 4.3f, 2.55f, z + 4.8f, pole);
  city_box(x - 4.3f, z + 4.8f, .58f, .10f, 2.25f, 2.78f, 42, 116, 70);
  int signColor = index % 3 == 0 ? C(205, 65, 48) : (index % 3 == 1 ? C(50, 135, 82) : C(42, 105, 151));
  city_box(x - 4.3f, z + 4.8f, .46f, .10f, 1.55f, 2.12f, (signColor >> 11) * 255 / 31, ((signColor >> 5) & 63) * 255 / 63, (signColor & 31) * 255 / 31);
  city_line(x - 4.62f, 2.50f, z + 4.91f, x - 4.03f, 2.50f, z + 4.91f, C(220, 225, 210));
  city_line(x - 4.55f, 1.68f, z + 4.91f, x - 4.05f, 1.68f, z + 4.91f, C(220, 225, 210));
  city_manhole(x + 2.4f, z + 2.4f);
  if (index == 0) {
    city_box(x - 7.f, z - 5.f, 1.1f, .5f, .05f, .85f, 65, 118, 170);
    city_box(x - 3.8f, z - 5.f, 1.1f, .5f, .05f, .85f, 65, 118, 170);
    city_box(x + 1.f, z - 6.f, 4.f, 2.f, .05f, 3.8f, 182, 165, 117);
    city_box(x + 1.f, z - 3.92f, 2.2f, .08f, 2.2f, 3.25f, 175, 55, 42);
  } else if (index == 1) {
    city_box(x, z - 11.1f, 5.f, .18f, .15f, 2.8f, 96, 103, 105);
    for (int bay = 0; bay < 3; bay++) {
      float bx = x - 3.2f + bay * 3.2f;
      city_line(bx, .2f, z - 11.25f, bx, 2.55f, z - 11.25f, C(62, 69, 71));
    }
    city_box(x, z + 3.f, 6.f, 4.f, .02f, .05f, 58, 60, 61);
    for (int bay = 0; bay < 4; bay++) { float bx = x - 4.5f + bay * 3.f; city_line(bx, .06f, z, bx, .06f, z + 6.f, C(220, 215, 185)); }
  } else if (index == 2) {
    for (int cone = 0; cone < 4; cone++) { float cx = x - 5.f + cone * 3.f, cz = z - 5.f; city_box(cx, cz, .32f, .32f, .05f, .6f, 230, 110, 38); city_box(cx, cz, .24f, .24f, .6f, .67f, 235, 220, 190); }
    city_box(x + 5.f, z - 4.f, .65f, .65f, .05f, 1.35f, 192, 145, 70);
  } else if (index == 3) {
    for (int space = 0; space < 5; space++) { float bx = x - 7.f + space * 3.4f; city_line(bx, .04f, z - 7.f, bx, .04f, z - 1.f, C(228, 224, 200)); }
    city_box(x + 5.f, z + 4.f, 1.3f, 1.3f, .04f, 1.25f, 55, 105, 65);
  }
}
static void draw_city_wall(int index) {
  float x[8], y[8], z[8];
  for (int i = 0; i < 8; i++) {
    if (index < 2) { x[i] = index == 0 ? -57.f : CITY_X_LIMIT; z[i] = (i & 4) ? CITY_Z_LIMIT : -CITY_Z_LIMIT; }
    else { x[i] = (i & 1) ? CITY_X_LIMIT : -CITY_X_LIMIT; z[i] = index == 2 ? -73.f : CITY_Z_LIMIT; }
    if (index < 2) x[i] += (i & 1) ? 1.f : 0.f;
    else z[i] += (i & 4) ? 1.f : 0.f;
    y[i] = (i & 2) ? 3.2f : 0.f;
  }
  draw_prism3(x, y, z, 92, 101, 102);
}
static void traffic_prism(const TrafficCar *car, float halfWidth, float halfLength, float y0, float y1, int cr, int cg, int cb) {
  float x[8], y[8], z[8], rx = car->hz, rz = -car->hx;
  for (int i = 0; i < 8; i++) { float side = (i & 1) ? halfWidth : -halfWidth, along = (i & 4) ? halfLength : -halfLength;
    x[i] = car->x + side * rx + along * car->hx; z[i] = car->z + side * rz + along * car->hz; y[i] = (i & 2) ? y1 : y0; }
  draw_prism3(x, y, z, cr, cg, cb);
}
static void make_city(void) {
  int k = 0;
  for (int iz = -2; iz < 2; iz++) for (int ix = -2; ix < 2; ix++) {
    int ax = ix, az = iz; city_center(ax, az, &cityBlocks[k].x, &cityBlocks[k].z);
    cityBlocks[k].h = city_height(ax, az); k++;
  }
}
static void draw_traffic(int index) {
  const TrafficCar *car = &traffic[index];
  static const uint8_t CR[8] = {200, 40, 215, 185, 65, 230, 110, 175}, CG[8] = {48, 135, 170, 90, 165, 180, 90, 140}, CB[8] = {40, 55, 45, 40, 70, 65, 180, 60};
  int color = index & 7;
  traffic_prism(car, .82f, 1.8f, .18f, .82f, CR[color], CG[color], CB[color]);
  traffic_prism(car, .65f, .86f, .84f, 1.27f, 44, 82, 102);
  traffic_prism(car, .66f, .34f, .82f, .89f, 25, 28, 32);
}
static void draw_police(void) {
  traffic_prism(&policeCar, .86f, 1.9f, .18f, .84f, 235, 235, 228);
  traffic_prism(&policeCar, .64f, .9f, .86f, 1.28f, 42, 72, 92);
  TrafficCar light = policeCar; float sideX = policeCar.hz * .28f, sideZ = -policeCar.hx * .28f;
  light.x += sideX; light.z += sideZ; traffic_prism(&light, .22f, .18f, 1.28f, 1.43f, ((int)(signalClock * 3.f) & 1) ? 35 : 230, 50, ((int)(signalClock * 3.f) & 1) ? 230 : 35);
  light.x -= sideX * 2.f; light.z -= sideZ * 2.f; traffic_prism(&light, .22f, .18f, 1.28f, 1.43f, ((int)(signalClock * 3.f) & 1) ? 230 : 35, 50, ((int)(signalClock * 3.f) & 1) ? 35 : 230);
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
  static const float GU[NG] = {0, 0, 0, 0, -.95f, .95f, 0, .45f, -.45f, .45f}, GV[NG] = {1.5f, 2.4f, -2.4f, -1.65f, 0, 0, .85f, 1.4f, -.15f, -.15f};
  for (int g = 0; g < NG; g++) {
    int b0 = PB[g], cnt = PN[g];
    if (pAtt[g] > 0) { if (cnt == 4 && g != 7) for (int i = 0; i < 4; i++) line((int)TX(b0 + i), (int)TY(b0 + i), (int)TX(b0 + ((i + 1) & 3)), (int)TY(b0 + ((i + 1) & 3)), C(230, 230, 235)); }
    else { int x = (int)(cx0 + GU[g] * S * gsx), y = (int)(cy0 - GV[g] * S * gsz), r = full ? 5 : 2; line(x - r, y - r, x + r, y + r, C(255, 40, 40)); line(x - r, y + r, x + r, y - r, C(255, 40, 40)); }
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
  struct { int ty, idx; float d; } L[96]; int m = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  L[m].ty = 0; L[m].idx = 0; L[m++].d = depth_of(cx, cz);
  if (ntr >= 0) { L[m].ty = 1; L[m].idx = 0; L[m++].d = depth_of(n[ntr + 4].x, n[ntr + 4].z); }
  for (int o = 0; o < nobj && m < 12; o++) { int b = nob0 + o * 8; L[m].ty = 2; L[m].idx = o; L[m++].d = depth_of(n[b].x, n[b].z); }
  if (mapId == 3) { make_city(); for (int b = 0; b < 16; b++) { L[m].ty = 3; L[m].idx = b; L[m++].d = depth_of(cityBlocks[b].x, cityBlocks[b].z); }
    if (trafficEnabled) for (int a = 0; a < trafficCount; a++) {
      float dx = traffic[a].x - camx, dz = traffic[a].z - camz, d = dx * camhx + dz * camhz, side = dx * camhz - dz * camhx;
      if (d < 1.f || d >= zmax - 2.f || side < -d * 1.45f - 3.f || side > d * 1.45f + 3.f) continue;
      L[m].ty = 4; L[m].idx = a; L[m++].d = d;
    }
    if (pursuitEnabled && m < 95) { L[m].ty = 6; L[m].idx = 0; L[m++].d = depth_of(policeCar.x, policeCar.z); }
    for (int w = 0; w < 4 && m < 96; w++) { float x = w < 2 ? (w == 0 ? -56.5f : 56.5f) : 0.f, z = w < 2 ? 0.f : (w == 2 ? -72.5f : 72.5f); L[m].ty = 5; L[m].idx = w; L[m++].d = depth_of(x, z); } }
  for (int i = 1; i < m; i++) { __typeof__(L[0]) v = L[i]; int j = i - 1; while (j >= 0 && L[j].d < v.d) { L[j + 1] = L[j]; j--; } L[j + 1] = v; }
  for (int k = 0; k < m; k++) {
    if (L[k].ty == 0) car();
    else if (L[k].ty == 1) { trailer_wheels(0); draw_box(ntr, 150, 150, 160); trailer_wheels(1); }
    else if (L[k].ty == 2 && L[k].d > 1 && L[k].d < zmax - 2) { const Obj *O = &OBJ[okind[L[k].idx]]; draw_box(nob0 + L[k].idx * 8, O->r, O->g, O->b); }
    if (L[k].ty == 3 && L[k].d > 1 && L[k].d < zmax - 2) { draw_city_block(L[k].idx); draw_city_details(L[k].idx); }
    else if (L[k].ty == 4 && L[k].d > 1 && L[k].d < zmax - 2) draw_traffic(L[k].idx);
    else if (L[k].ty == 5 && L[k].d > 1 && L[k].d < zmax - 2) draw_city_wall(L[k].idx);
    else if (L[k].ty == 6 && L[k].d > 1 && L[k].d < zmax - 2) draw_police();
  }
  if (weatherMode == 1 || weatherMode == 3) {
    uint16_t streak = weatherMode == 1 ? C(155, 190, 210) : C(205, 218, 220);
    for (int i = 0; i < 14; i++) { int x = (i * 37 + (int)(signalClock * 45.f)) % (lw - 3) + 2, y = (i * 29 + (int)(signalClock * 71.f)) % (lh - 5); line(x, y, x - 2, y + 5, streak); }
  }
  if (showTop && xd0 == 0) { int bw = lw / 4, bh = lh * 40 / 112; for (int y = lh - bh; y < lh; y++) for (int x = 0; x < bw; x++) fb[y * lw + x] = C(18, 20, 30); draw_top(bw / 2, lh - bh / 2, 6.5f * lw / 160, 0); }
}
// ---- settings, controls
enum { A_ACC, A_BRK, A_LEFT, A_RIGHT, A_RESET, A_HOOD, A_DOORS, A_TRUNK, A_ALL, A_BEAMS, A_TOP, A_DMG, A_CL, A_CR, A_CU, A_CD, A_ZI, A_ZO, A_CRESET, A_QUICK, A_TURBO, A_CINT, A_SKIP, NA };
static const char *AN[NA] = {"Accelerer", "Freiner", "Gauche", "Droite", "Remettre/Rejouer", "Capot", "Portes", "Coffre", "Tout ouvrir", "Poutres", "Vue dessus", "Degats", "Camera gauche", "Camera droite", "Camera haut", "Camera bas", "Zoom +", "Zoom -", "Camera reset", "Menu rapide", "Turbo (tenir)", "Vue habitacle", "Tutoriel: etape suivante"};
static const int BDEF[NA] = {eadk_key_up, eadk_key_down, eadk_key_left, eadk_key_right, eadk_key_ok, eadk_key_var, eadk_key_xnt, eadk_key_exp, eadk_key_shift, eadk_key_toolbox, eadk_key_ln, eadk_key_log, eadk_key_four, eadk_key_six, eadk_key_eight, eadk_key_two, eadk_key_seven, eadk_key_nine, eadk_key_five, eadk_key_exe, eadk_key_backspace, eadk_key_zero, eadk_key_dot};
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
static int tutOn, tutStep, tutFlag, tutOk, evDmg, evRepair, mapBak;
static float tutH0x, tutH0z;
static void startpos(void) {
  car_init(mapId == 0 ? TA : 0, 0, 0, 1, .4f, 1); ct = 0; stopT = 0; vpk = 0; impV = 0; score = 0;
  traffic_reset();
  if (mapId == 2) for (int i = 0; i < (ntr >= 0 ? ntr + 11 : NC); i++) n[i].vz = CRASHV[crashI] / 3.6f;
}
static char *dec1(char *p, float value) { p = num(p, (int)value); p = cat(p, "."); return num(p, (int)(value * 10.f) % 10); }
static const char *kname(int action) { return KN[bind[action]] ? KN[bind[action]] : "?"; }
static float absf(float value) { return value < 0 ? -value : value; }
static void tut_adv(float hx, float hz) {
  if (tutStep >= 13) return;
  tutStep++; tutFlag = 0; tutOk = 40; tutH0x = hx; tutH0z = hz;
  if (tutStep == 8) tHood = tDoor = tTrunk = 1.f;
  if (tutStep == 10) evDmg = 0;
  if (tutStep == 11) evRepair = 0;
}
static void tut_text(char *text) {
  char *p = text; p = num(p, tutStep + 1 > 13 ? 13 : tutStep + 1); p = cat(p, "/13 "); if (tutOk > 0) p = cat(p, "Bien! ");
  switch (tutStep) {
    case 0: p = cat(p, "Accelere avec "); p = cat(p, kname(A_ACC)); cat(p, " (25 km/h)"); break;
    case 1: p = cat(p, "Tourne avec "); p = cat(p, kname(A_LEFT)); p = cat(p, " et "); cat(p, kname(A_RIGHT)); break;
    case 2: p = cat(p, "Roule vite puis freine ("); p = cat(p, kname(A_BRK)); cat(p, ")"); break;
    case 3: p = cat(p, "Camera: tourne avec "); p = cat(p, kname(A_CL)); p = cat(p, " ou "); cat(p, kname(A_CR)); break;
    case 4: p = cat(p, "Camera: monte avec "); cat(p, kname(A_CU)); break;
    case 5: p = cat(p, "Ouvre le capot ("); p = cat(p, kname(A_HOOD)); cat(p, ")"); break;
    case 6: p = cat(p, "Ouvre les portes ("); p = cat(p, kname(A_DOORS)); cat(p, ")"); break;
    case 7: p = cat(p, "Ouvre le coffre ("); p = cat(p, kname(A_TRUNK)); cat(p, ")"); break;
    case 8: p = cat(p, "Referme tout avec "); cat(p, kname(A_ALL)); break;
    case 9: cat(p, "Fonce dans un immeuble pour abimer la voiture"); break;
    case 10: p = cat(p, "Regarde les degats avec "); cat(p, kname(A_DMG)); break;
    case 11: p = cat(p, "Repare: "); p = cat(p, kname(A_QUICK)); cat(p, " puis Reparer"); break;
    case 12: p = cat(p, "Turbo: "); p = cat(p, kname(A_QUICK)); cat(p, " puis Turbo"); break;
    default: cat(p, "Bravo! Tutoriel fini. Pause pour quitter");
  }
}
static const char *MAPN[4] = {"Circuit", "Route", "Crash-test", "Ville"}, *SOLN[3] = {"Robuste", "Normale", "Fragile"};
static const char *MN[9] = {"Jouer", "Garage", "Carte", "Solidite", "Test", "Reglages", "Commandes", "Tutoriel", "Quitter"};
static void draw_menu(int sel) {
  clear(); txt("NumBeam 3D", 90, 6, 1, C(230, 60, 50), C(15, 18, 28)); txt("simulateur de collision", 82, 32, 0, C(150, 160, 190), C(15, 18, 28));
  for (int i = 0; i < 9; i++) { char s[40], *p = s; p = cat(p, MN[i]);
    if (i == 2) { p = cat(p, ": "); cat(p, MAPN[mapId]); } else if (i == 3) { p = cat(p, ": "); cat(p, SOLN[solid]); } else if (i == 4) { p = cat(p, ": "); p = num(p, CRASHV[crashI]); cat(p, " km/h"); }
    row(s, 80, 46 + i * 19, 160, 17, i == sel); }
  txt("Haut/Bas, Gauche/Droite, OK", 66, 226, 0, C(110, 120, 150), C(15, 18, 28));
}
static void draw_garage(int sel, int gv) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 160, 240}, C(15, 18, 28));
  const char *lab[12] = {"Vue (OK)", "Vehicule", "Generer", "Roues", "Susp.", "Moteur", "Chassis", "Remorque", "Capot", "Portes", "Coffre", "JOUER"};
  char seed[16], *sp = seed; *sp++ = '#'; num(sp, (int)genSeed);
  const char *val[11] = {"", VEH[cv].nm, seed, WHL[cw].nm, SUS[cs].nm, ENG[ce].nm, CHA[cc].nm, trl ? "oui" : "non", tHood > .5f ? "ouvert" : "ferme", tDoor > .5f ? "ouvertes" : "fermees", tTrunk > .5f ? "ouvert" : "ferme"};
  for (int i = 0; i < 12; i++) { char s[32], *p = s; p = cat(p, lab[i]); if (i > 0 && i < 11) { p = cat(p, ": "); cat(p, val[i]); } row(s, 4, 3 + i * 16, 152, 15, i == sel); }
  if (gv) { txt("Fleches: tourner/hauteur", 4, 198, 0, C(255, 210, 80), C(15, 18, 28)); txt("+ / - : zoom", 4, 211, 0, C(255, 210, 80), C(15, 18, 28)); txt("OK ou Retour: fin", 4, 224, 0, C(255, 210, 80), C(15, 18, 28)); return; }
  char s[32], *p = s; p = cat(p, "Puiss "); p = num(p, (int)(ENG[ce].a * VEH[cv].pw * 10)); p = cat(p, " Vmax "); num(p, (int)(ENG[ce].v * 3.6f));
  txt(s, 6, 198, 0, C(180, 190, 220), C(15, 18, 28));
  p = s; p = cat(p, "Grip "); p = num(p, (int)(WHL[cw].gr * VEH[cv].gr * 100)); p = cat(p, " Solid. "); num(p, (int)(CHA[cc].k / 140 * (solid == 0 ? 1.6f : (solid == 2 ? .65f : 1.f))));
  txt(s, 6, 211, 0, C(180, 190, 220), C(15, 18, 28));
  p = s; p = cat(p, "Dim "); p = dec1(p, 4.6f * VEH[cv].sz); p = cat(p, "x"); p = dec1(p, 1.9f * VEH[cv].sx); p = cat(p, "x"); dec1(p, 1.45f * VEH[cv].sy);
  txt(s, 6, 224, 0, C(180, 190, 220), C(15, 18, 28));
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
#define QUICK_COUNT 21
static const char *QL[QUICK_COUNT] = {"Reparer tout", "Turbo", "Boost !", "Voiture", "Moteur", "Remorque", "Route", "Gravite", "Temps", "Retour au depart", "Trafic IA", "Nombre IA", "Comportement IA", "Vitesse IA", "Plan de ville", "Meteo", "Puissance", "Adherence", "Suspension", "Freinage", "Poursuite police"};
static void draw_quick(int sel) {
  static const char *RN[3] = {"Seche", "Mouillee", "Glace"}, *GN[3] = {"Normale", "Lune", "Forte"};
  static const char *AI_MODE[3] = {"Circuit", "Suit joueur", "Patrouille"}, *AI_SPEED[5] = {"Tres lent", "Lent", "Normal", "Rapide", "Tres rapide"};
  static const char *CITY_PLAN[3] = {"Regulier", "Decale", "Canalise"}, *WEATHER[4] = {"Clair", "Pluie", "Brouillard", "Glace"};
  eadk_display_push_rect_uniform((eadk_rect_t){40, 0, 240, 240}, C(15, 18, 28)); txt("OPTIONS RAPIDES", 108, 3, 0, C(255, 210, 80), C(15, 18, 28));
  int top = sel - 5; if (top < 0) top = 0; if (top > QUICK_COUNT - 12) top = QUICK_COUNT - 12;
  for (int line = 0; line < 12; line++) { int i = top + line; char s[40], *p = s; p = cat(p, QL[i]);
    if (i == 1) cat(p, turboT ? ": oui" : ": non"); else if (i == 3) { p = cat(p, ": "); cat(p, VEH[cv].nm); } else if (i == 4) { p = cat(p, ": "); cat(p, ENG[ce].nm); }
    else if (i == 5) cat(p, trl ? ": oui" : ": non"); else if (i == 6) { p = cat(p, ": "); cat(p, RN[ri]); } else if (i == 7) { p = cat(p, ": "); cat(p, GN[gi]); } else if (i == 8) cat(p, tscale < .5f ? ": ralenti" : ": normal");
    else if (i == 10) cat(p, trafficEnabled ? ": oui" : ": non"); else if (i == 11) { p = cat(p, ": "); p = num(p, trafficCount); cat(p, "/64"); }
    else if (i == 12) { p = cat(p, ": "); cat(p, AI_MODE[trafficBehavior]); } else if (i == 13) { p = cat(p, ": "); cat(p, AI_SPEED[trafficSpeed]); }
    else if (i == 14) { p = cat(p, ": "); cat(p, CITY_PLAN[cityPlan]); } else if (i == 15) { p = cat(p, ": "); cat(p, WEATHER[weatherMode]); }
    else if (i >= 16 && i <= 19) { float v = i == 16 ? tunePower : (i == 17 ? tuneGrip : (i == 18 ? tuneSusp : tuneBrake)); p = cat(p, ": "); p = num(p, (int)(v * 100.f)); cat(p, "%"); }
    else if (i == 20) cat(p, pursuitEnabled ? ": active" : ": inactive");
    row(s, 50, 18 + line * 18, 220, 17, i == sel); }
}
static void draw_tutorial(void) {
  char text[72]; tut_text(text); eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14));
  txt(text, 4, 225, 0, tutOk > 0 ? C(120, 255, 140) : C(255, 210, 80), C(10, 10, 14));
}
static void draw_dmg(void) {
  for (int i = 0; i < LW * LH; i++) fb[i] = C(15, 18, 28);
  curZ = -1; draw_top(lw * 45 / 160, lh / 2, 20.f * lw / 160, 1); present(0);
  int zp[5]; zones(zp); const char *zn[5] = {"Avant", "Arriere", "Gauche", "Droite", "Toit"};
  txt("DEGATS", 220, 6, 1, C(230, 60, 50), C(15, 18, 28));
  for (int i = 0; i < 5; i++) { char s[24], *p = s; p = cat(p, zn[i]); p = cat(p, ": "); p = num(p, zp[i]); cat(p, "%"); txt(s, 190, 36 + i * 17, 0, zp[i] > 60 ? C(255, 90, 80) : (zp[i] > 25 ? C(255, 210, 80) : C(120, 220, 120)), C(15, 18, 28)); }
  { char ln[28]; ln[0] = 0; char *p = ln; int y = 124, any = 0;
    for (int g = 0; g < NG; g++) if (pAtt[g] <= 0) { any = 1; if ((p - ln) + (int)sizeof(PNAME[0]) > 0 && (p - ln) > 11) { txt(ln, 190, y, 0, C(255, 120, 100), C(15, 18, 28)); y += 14; p = ln; *p = 0; } p = cat(p, PNAME[g]); p = cat(p, " "); }
    if (any) txt(ln, 190, y, 0, C(255, 120, 100), C(15, 18, 28)); else txt("Aucune piece perdue", 190, y, 0, C(120, 220, 120), C(15, 18, 28)); }
  char s[24], *p = s;
  int wl = 0; for (int w = 0; w < 4; w++) wl += wAtt[w] > 0; p = s; p = cat(p, "Roues: "); p = num(p, wl); cat(p, "/4"); txt(s, 190, 192, 0, 0xFFFF, C(15, 18, 28));
  p = s; p = cat(p, "Total: "); p = num(p, (int)dmg); cat(p, "%"); txt(s, 190, 208, 0, C(255, 210, 80), C(15, 18, 28));
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
  float acc = 0, stw = 0, pz = 0, gorb = .6f, gelev = 3.6f, gzoom = 7.5f, shx = 0, shz = 1, yawO = 0, hO = 0, dO = 0; uint64_t last = eadk_timing_millis(), lapT = last;
  for (int i = 0; i < NA; i++) bind[i] = BDEF[i];
  gen_car(genSeed);
#ifdef TESTQ
  qual = TESTQ;
#endif
  setq();
#ifdef TESTST
  st = TESTST; tHood = oHood = TESTO; tDoor = oDoor = TESTO; tTrunk = oTrunk = TESTO; cv = TESTV; ce = 3; cw = 1; trl = TESTT; mapId = TESTM; crashI = 3; gview = TESTGV;
  if (st == S_GARAGE) garage_car(); else startpos();
#ifdef TESTGO
  gorb = TESTGO; gelev = TESTGE; gzoom = TESTGZ;
#endif
#endif
  for (;;) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    int U = pressed(k, eadk_key_up), D = pressed(k, eadk_key_down), L = pressed(k, eadk_key_left), R = pressed(k, eadk_key_right), O = pressed(k, eadk_key_ok), B = pressed(k, eadk_key_back);
    if (st == S_MENU) {
      if (redraw) { draw_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 8) % 9; redraw = 1; } if (D) { sel = (sel + 1) % 9; redraw = 1; }
      if ((L || R || O) && sel >= 2 && sel <= 4) { int d = L ? -1 : 1; redraw = 1;
        if (sel == 2) mapId = (mapId + 4 + d) % 4; else if (sel == 3) solid = (solid + 3 + d) % 3; else crashI = (crashI + 5 + d) % 5; }
      else if (O) { if (sel == 0) { tutOn = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 1) { garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 5) { retS = S_MENU; sel = 0; st = S_SET; redraw = 1; }
        else if (sel == 6) { retS = S_MENU; sel = 0; cap = 0; st = S_CTRL; redraw = 1; }
        else if (sel == 7) { mapBak = mapId; mapId = 3; tutOn = 1; tutStep = tutFlag = tutOk = evDmg = evRepair = 0; yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 8) return 0; }
      eadk_timing_msleep(30);
    } else if (st == S_GARAGE) {
      if (redraw) { draw_garage(gsel, gview); redraw = 0; }
      if (gview) {                                       // free orbit with the arrow keys
        if (down(k, eadk_key_left)) gorb -= .05f; if (down(k, eadk_key_right)) gorb += .05f;
        if (down(k, eadk_key_plus)) gzoom -= .1f; if (down(k, eadk_key_minus)) gzoom += .1f; if (gzoom < 3.5f) gzoom = 3.5f; if (gzoom > 14.f) gzoom = 14.f;
        if (down(k, eadk_key_up)) gelev += .1f; if (down(k, eadk_key_down)) gelev -= .1f; if (gelev > 9.f) gelev = 9.f; if (gelev < .3f) gelev = .3f;
        if (O || B) { gview = 0; redraw = 1; }
      } else {
        int ch = 0;
        if (U) { gsel = (gsel + 11) % 12; redraw = 1; } if (D) { gsel = (gsel + 1) % 12; redraw = 1; }
        if (O && gsel == 0) { gview = 1; redraw = 1; }
        else if (O && gsel == 2) { genSeed = (eadk_timing_millis() % 9999) + 1; gen_car(genSeed); cv = 4; garage_car(); redraw = 1; }
        else if (L || R || (O && gsel >= 8 && gsel <= 10)) { int d = R ? 1 : -1; redraw = 1; ch = 1;
          if (gsel == 1) { cv = (cv + NV + d) % NV; if (cv == 4) gen_car(genSeed); }
          else if (gsel == 2) { int seed = (int)genSeed + d; if (seed < 1) seed = 9999; if (seed > 9999) seed = 1; gen_car((unsigned)seed); cv = 4; }
          else if (gsel == 3) cw = (cw + 4 + d) % 4; else if (gsel == 4) cs = (cs + 4 + d) % 4; else if (gsel == 5) ce = (ce + 4 + d) % 4; else if (gsel == 6) cc = (cc + 3 + d) % 3;
          else if (gsel == 7) trl ^= 1; else if (gsel == 8) tHood = tHood > .5f ? 0 : 1; else if (gsel == 9) tDoor = tDoor > .5f ? 0 : 1; else if (gsel == 10) tTrunk = tTrunk > .5f ? 0 : 1; else ch = 0; }
        if (ch && gsel >= 1 && gsel <= 7) garage_car();
        if (O && gsel == 11) { tutOn = 0; startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        if (B) { st = S_MENU; redraw = 1; }
      }
      if (st == S_GARAGE) {
        oHood += (tHood - oHood) * .15f; oDoor += (tDoor - oDoor) * .15f; oTrunk += (tTrunk - oTrunk) * .15f;
        float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); float dist = gzoom + (trl ? 3.5f : 0.f);
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
      if (bpress(k, A_CINT)) { camInterior ^= 1; if (camInterior) { yawO = 0; hO = 0; dO = 0; } }
      if (bpress(k, A_BEAMS)) structure ^= 1;
      if (bpress(k, A_TOP)) showTop ^= 1;
      if (bpress(k, A_DMG)) { if (tutOn) evDmg = 1; st = S_DMG; redraw = 1; pk = k; continue; }
      if (tutOn && bpress(k, A_SKIP)) tut_adv(hx, hz);
      if (bpress(k, A_QUICK)) { st = S_QUICK; sel = 0; redraw = 1; pk = k; continue; }
      if (bpress(k, A_HOOD)) tHood = tHood > .5f ? 0 : 1; if (bpress(k, A_DOORS)) tDoor = tDoor > .5f ? 0 : 1; if (bpress(k, A_TRUNK)) tTrunk = tTrunk > .5f ? 0 : 1;
      if (bpress(k, A_ALL)) { float v = (tHood > .5f || tDoor > .5f || tTrunk > .5f) ? 0 : 1; tHood = tDoor = tTrunk = v; }
      if (tutOn && tutStep < 13) {
        float kmh = sp * 3.6f; int ok = 0;
        switch (tutStep) {
          case 0: ok = kmh > 25.f; break;
          case 1: ok = absf(hx * tutH0z - hz * tutH0x) > .5f; break;
          case 2: if (kmh > 25.f) tutFlag = 1; ok = tutFlag && kmh < 3.f; break;
          case 3: ok = yawO > .6f || yawO < -.6f; break;
          case 4: ok = hO > 2.5f; break;
          case 5: ok = tHood > .5f; break;
          case 6: ok = tDoor > .5f; break;
          case 7: ok = tTrunk > .5f; break;
          case 8: ok = tHood < .3f && tDoor < .3f && tTrunk < .3f; break;
          case 9: ok = dmg > 25.f; break;
          case 10: ok = evDmg; break;
          case 11: ok = evRepair && dmg < 1.f; break;
          case 12: ok = turbo; break;
        }
        if (ok) tut_adv(hx, hz);
      }
      if (tutOk > 0) tutOk--;
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
      traffic_update(dtf * .001f);
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
      float dist = camInterior ? 3.4f : (ntr >= 0 ? 10.f : 8.5f) + dO; if (dist < 3.f) dist = 3.f;
      if (camInterior) { camx = cx - .28f * camhz + .12f * camhx; camz = cz + .28f * camhx + .12f * camhz; camy = cy + .55f * VEH[cv].sy; }
      else { camx = cx - camhx * dist; camz = cz - camhz * dist;
        float gy = gh(camx, camz) + 1.4f, ty = cy + 2.6f + hO; if (ty < gy) ty = gy; camy += (ty - camy) * .15f; }
      hor = (int)(lh * (camInterior ? .42f : .72f) - (camy - cy) * foc / dist);
      scene(0); present(0);
      if (++fr % 6 == 0) hud((int)(sp * 3.6f), vf < -.5f ? -1 : gearNow, lap, (int)(now - lapT), best);
      if (tutOn) draw_tutorial();
    } else if (st == S_DMG) {
      if (redraw) { draw_dmg(); redraw = 0; }
      if (U || D || L || R || O || B) { if (O && mapId == 2 && ct == 2) { startpos(); } st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
      eadk_timing_msleep(30);
    } else if (st == S_PAUSE) {
      if (redraw) { draw_pause(sel); redraw = 0; }
      if (U) { sel = (sel + 7) % 8; redraw = 1; } if (D) { sel = (sel + 1) % 8; redraw = 1; }
      if (B) { sel = 0; O = 1; }
      if (O) { if (sel == 0) { st = S_GAME; last = eadk_timing_millis(); acc = 0; }
        else if (sel == 1) { if (tutOn) { tutStep = tutFlag = tutOk = 0; evDmg = evRepair = 0; } startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; }
        else if (sel == 2) { st = S_QUICK; sel = 0; redraw = 1; } else if (sel == 3) { st = S_DMG; redraw = 1; }
        else if (sel == 4) { if (tutOn) { tutOn = 0; mapId = mapBak; } garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 5) { retS = S_PAUSE; sel = 0; st = S_SET; redraw = 1; }
        else if (sel == 6) { retS = S_PAUSE; sel = 0; cap = 0; st = S_CTRL; redraw = 1; } else { if (tutOn) { tutOn = 0; mapId = mapBak; } st = S_MENU; sel = 0; redraw = 1; } }
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
      if (U) { sel = (sel + QUICK_COUNT - 1) % QUICK_COUNT; redraw = 1; } if (D) { sel = (sel + 1) % QUICK_COUNT; redraw = 1; }
      if (L || R || O) { int d = L ? -1 : 1, close = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); redraw = 1;
        static const float RF[3] = {1.f, .6f, .25f}, GV[3] = {14.f, 5.f, 25.f};
        if (sel == 0 && O) { repair(cx, cz, hx, hz); if (tutOn) evRepair = 1; close = 1; }
        else if (sel == 1) turboT ^= 1;
        else if (sel == 2 && O) { for (int i = 0; i < (ntr >= 0 ? ntr + 11 : NC); i++) { n[i].vx += hx * 10.f; n[i].vz += hz * 10.f; } close = 1; }
        else if (sel == 3) { cv = (cv + NV + d) % NV; if (cv == 4) gen_car(genSeed); if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
        else if (sel == 4) { ce = (ce + 4 + d) % 4; engA = ENG[ce].a * VEH[cv].pw * tunePower; engV = ENG[ce].v; }
        else if (sel == 5) { trl ^= 1; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
        else if (sel == 6) { ri = (ri + 3 + d) % 3; gripF = RF[ri]; } else if (sel == 7) { gi = (gi + 3 + d) % 3; G = GV[gi]; }
        else if (sel == 8) tscale = tscale > .5f ? .4f : 1.f;
        else if (sel == 9 && O) { startpos(); lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); close = 1; }
        else if (sel == 10) { trafficEnabled ^= 1; traffic_reset(); }
        else if (sel == 11) { trafficCount += d; if (trafficCount < 0) trafficCount = 64; if (trafficCount > 64) trafficCount = 0; traffic_reset(); }
        else if (sel == 12) { trafficBehavior = (trafficBehavior + 3 + d) % 3; traffic_reset(); }
        else if (sel == 13) { trafficSpeed = (trafficSpeed + 5 + d) % 5; traffic_reset(); }
        else if (sel == 14) { cityPlan = (cityPlan + 3 + d) % 3; }
        else if (sel == 15) { weatherMode = (weatherMode + 4 + d) % 4; }
        else if (sel >= 16 && sel <= 19) {
          float *tune = sel == 16 ? &tunePower : (sel == 17 ? &tuneGrip : (sel == 18 ? &tuneSusp : &tuneBrake));
          float old = *tune; *tune += d * .1f; if (*tune < .5f) *tune = .5f; if (*tune > 1.5f) *tune = 1.5f;
          if (sel == 16) engA = ENG[ce].a * VEH[cv].pw * tunePower;
          if (sel == 17) { latG = WHL[cw].gr * VEH[cv].gr * tuneGrip; if (latG > .45f) latG = .45f; }
          if (sel == 18 && old > 0.f) for (int b = 0; b < nb; b++) if (bm[b].f == 1) { bm[b].k *= *tune / old; bm[b].c *= *tune / old; }
        } else if (sel == 20) { pursuitEnabled ^= 1; traffic_reset(); }
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

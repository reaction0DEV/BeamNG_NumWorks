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
#define ACC C(255, 102, 0)      // BeamNG-style orange accent
#define BGC C(18, 19, 22)       // dark UI background
#define PNL C(26, 27, 32)       // panel row
#define PSEL C(58, 36, 20)      // selected row
enum { S_MENU, S_GARAGE, S_GAME, S_PAUSE, S_DMG, S_SET, S_CTRL, S_KEYS, S_QUICK, S_DELIV, S_DRES };
static uint16_t fb[MAXW * MAXH], buf[320 * 8]; static uint8_t zb[MAXW * MAXH], xmap[320];
static int structure, CXc = 80, curZ = -1, showTop = 1, crashI = 2, camInterior;
typedef struct { float x, z, h; int kind; } CityBlock;
static CityBlock cityBlocks[64];
static const int CRASHV[5] = {30, 50, 80, 110, 140};
static float camhx = 0, camhz = 1, camx, camy = 5, camz, oHood, oDoor, oTrunk, tHood, tDoor, tTrunk;
static uint64_t pk; static float wspin;
// delivery challenge state (livraisons)
static int delOn, delLvl = 1, delN, delIdx, delEnd, delMsgKind, delMsgScore, delScore, legOk[8], legScore[8], delBest[3], delFinal, delStars, delPen;
static float delT, delLimit, delTx, delTz, delTotal, delMsgT, delDeadT, legTime[8];
static int recLap; static float grindT;   // recLap: best circuit lap (persistent records)

static int blendA, clipOn, clX0, clY0, clX1, clY1;   // clip rectangle (used by the mini damage map)
static inline void px(int x, int y, uint16_t c) {
  if (clipOn && (x < clX0 || x >= clX1 || y < clY0 || y >= clY1)) return;
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
      if (mapId == 4) { if (gfeat > .5f) { int band = ((int)(Z * .4f + 1000)) & 1; r = band ? 235 : 70; g = band ? 125 : 72; b = band ? 30 : 80; }
        else { int gx = (int)((X + 2000.f) * .5f), gz = (int)((Z + 2000.f) * .5f); r = g = b = ((gx + gz) & 1) ? 255 : 228; } }
      else if (mapId == 3) {
        if (X < -CITY_X_LIMIT || X > CITY_X_LIMIT || Z < -CITY_Z_LIMIT || Z > CITY_Z_LIMIT) { r = 48 + ck * 5; g = 105 + ck * 7; b = 51; }
        else {
          int rx, rz0; city_nearest(X, Z, &rx, &rz0);
          float cxg, czg; city_center(rx, rz0, &cxg, &czg); float ax = X - cxg, az = Z - czg; if (ax < 0) ax = -ax; if (az < 0) az = -az;
          float roadX = 14.f - ax, roadZ = 18.f - az;
          int park = ax < 8.5f && az < 11.f && city_kind(rx, rz0) == 3, building = ax < 8.5f && az < 11.f && !park;
          int onRoadX = !building && roadX < 3.8f, onRoadZ = !building && roadZ < 3.8f;
          int onSidewalk = !building && !onRoadX && !onRoadZ && (roadX < 5.5f || roadZ < 7.f);
          if (park) { r = 62 + ck * 6; g = 128 + ck * 6; b = 66; if (ax < 1.1f || az < 1.1f) { r = 160; g = 148; b = 118; } }
          else if (building || onSidewalk) { r = 118; g = 116; b = 105; }
          else if (onRoadX || onRoadZ) { r = 48; g = 53; b = 57; if ((roadX < .12f || roadZ < .12f) && (((int)(X + Z) / 4) & 1)) { r = 220; g = 190; b = 95; } }
          else { r = 73 + ck * 5; g = 103 + ck * 5; b = 74; if (ad < 18.f && st) { r = 52; g = 80; b = 58; } }
          int cross = ((onRoadX && roadZ < 7.f && (((int)(Z * 1.8f) & 3) == 0)) || (onRoadZ && roadX < 7.f && (((int)(X * 1.8f) & 3) == 0)));
          if (cross) { r = 205; g = 200; b = 178; }
          if (cityPlan == 2 && (onRoadX || onRoadZ) && (((int)(X * 3.f + Z * 2.f) & 15) == 0)) { r = 215; g = 195; b = 115; }
          if (weatherMode == 1 && (onRoadX || onRoadZ)) { r = (r * 3) / 4; g = (g * 4) / 5; b = (b * 9) / 10; }
          if (weatherMode == 3 && (onRoadX || onRoadZ)) { r = (r * 3) / 4; g = (g * 4) / 5; b = (b * 5) / 6; }
          if (gbumpF > .5f && (onRoadX || onRoadZ)) { int sb = ((int)((onRoadX && !onRoadZ ? Z : X) * 1.5f + 1000.f)) & 1; r = sb ? 235 : 40; g = sb ? 200 : 40; b = sb ? 40 : 44; }   // speed bump stripes
        }
      }
      else if (gfeat > .25f) { if (mapId == 1) { r = 175; g = 170; b = 150; } else { r = st ? 230 : 200; g = st ? 230 : 45; b = g; } }
      else if (mapId == 0 && ad < 6.f) { r = 70; g = 70; b = 76; if (Z > -1.5f && Z < 1.5f && X > TA - 6 && X < TA + 6) r = g = b = (((int)(X * 1.5f + 1000) + (int)(Z * 1.5f + 1000)) & 1) ? 240 : 25; else if (ad > 5.5f) r = g = b = 200; }
      else if (mapId == 0 && ad < 7.4f) { r = st ? 220 : 235; g = st ? 40 : 235; b = g; }
      else if (mapId == 1 && ad < 6.f) { r = 72; g = 72; b = 78; if (ad < .15f && (((int)(Z / 3.f)) & 1)) { r = 230; g = 210; b = 70; } if (ad > 5.6f) r = g = b = 200; if (gbumpF > .5f && ad < 5.4f) { int sb = ((int)(Z * 1.5f + 1000.f)) & 1; r = sb ? 235 : 40; g = sb ? 200 : 40; b = sb ? 40 : 44; } }
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
// cond: 0 always, 1 trunk visible, 2 engine bay visible, 3 engine visible, 4 turbo only,
//       5 bay visible + radiator leaking, 6 trunk visible + tank leaking, 7 underbody (camera low)
static const Bx BXS[] = {
  {0,0,-.82f,.82f,.7f,.98f,.5f,.8f,52,50,58},{0,0,-.62f,-.3f,.9f,.95f,.4f,.52f,30,30,34},{0,0,-.48f,-.44f,.78f,.9f,.45f,.6f,30,30,34},{0,0,-.1f,.1f,.45f,.72f,-.3f,.55f,55,55,62},
  {0,0,-.8f,.8f,.45f,.62f,-1.f,-.45f,70,68,76},{0,0,-.8f,.8f,.62f,1.1f,-1.08f,-.95f,70,68,76},{0,0,-.85f,.85f,.38f,.44f,-1.1f,.8f,48,46,52},
  {0,1,-.82f,.82f,.5f,.56f,-1.98f,-1.3f,60,58,60},{0,1,-.88f,-.82f,.56f,.95f,-1.98f,-1.3f,70,70,74},{0,1,.82f,.88f,.56f,.95f,-1.98f,-1.3f,70,70,74},{0,1,-.82f,.82f,.56f,.98f,-1.34f,-1.28f,66,64,70},
  {0,1,-.8f,-.12f,.56f,.68f,-1.88f,-1.42f,30,30,34},{0,1,-.78f,-.42f,.68f,.8f,-1.8f,-1.56f,140,64,40},{0,1,-.82f,.82f,.56f,.95f,-1.99f,-1.93f,80,80,86},
  {0,2,-.85f,.85f,.36f,.4f,.8f,2.05f,50,50,55},{0,2,-.9f,-.85f,.4f,.88f,.8f,2.05f,64,64,70},{0,2,.85f,.9f,.4f,.88f,.8f,2.05f,64,64,70},{0,2,-.85f,.85f,.4f,.95f,.76f,.82f,58,58,64},
  {0,2,.45f,.8f,.4f,.62f,1.55f,1.8f,25,30,60},{0,2,-.8f,-.6f,.4f,.55f,1.3f,1.5f,50,90,200},
  {1,3,0,1,0,.62f,0,1,0,0,0},{1,3,.08f,.92f,.62f,.82f,.1f,.9f,170,170,180},{1,3,.3f,.7f,.82f,1.f,.2f,.75f,95,95,100},{1,3,.15f,.4f,.15f,.4f,1.f,1.1f,200,200,205},
  {1,3,.6f,.9f,.1f,.4f,1.f,1.08f,40,40,44},{1,3,1.f,1.1f,.1f,.5f,.1f,.9f,150,80,40},{1,4,-.4f,0,.15f,.5f,.2f,.6f,230,90,40},
  {1,3,.1f,.43f,.2f,.58f,.15f,.88f,125,130,138},{1,3,.57f,.9f,.2f,.58f,.15f,.88f,125,130,138},
  {1,3,.34f,.66f,.83f,.94f,.18f,.78f,78,84,94},{1,3,.38f,.62f,.08f,.18f,.72f,.98f,45,48,54},
  {1,3,.05f,.14f,.14f,.28f,.22f,.82f,205,135,55},{1,3,.86f,.95f,.14f,.28f,.22f,.82f,205,135,55},
  {2,0,0,1,0,.28f,0,1,45,45,55},{3,0,0,1,0,.28f,0,1,45,45,55},{2,0,0,1,.28f,1.35f,-.35f,.1f,50,50,60},{3,0,0,1,.28f,1.35f,-.35f,.1f,50,50,60},{2,0,.25f,.75f,1.35f,1.7f,-.3f,.05f,45,45,55},{3,0,.25f,.75f,1.35f,1.7f,-.3f,.05f,45,45,55}};
#define NBX ((int)(sizeof(BXS) / sizeof(BXS[0])))
// vehicle systems: radiator + fan + hoses (bay), fuel tank (trunk), gearbox, driveshaft / differential / axles / exhaust (underbody)
static const Bx SYSB[] = {
  {0,2,-.68f,.68f,.46f,.86f,1.93f,2.0f,100,106,116},{0,2,-.68f,.68f,.52f,.54f,2.0f,2.012f,22,24,28},{0,2,-.68f,.68f,.64f,.66f,2.0f,2.012f,22,24,28},{0,2,-.68f,.68f,.76f,.78f,2.0f,2.012f,22,24,28},
  {0,2,-.7f,.7f,.86f,.92f,1.92f,2.01f,38,40,46},{0,2,.5f,.62f,.92f,.99f,1.94f,2.0f,150,36,36},{0,2,-.7f,.7f,.4f,.46f,1.92f,2.01f,38,40,46},{0,2,.52f,.62f,.78f,.88f,1.55f,1.95f,16,16,18},{0,2,-.62f,-.52f,.42f,.52f,1.6f,1.95f,16,16,18},
  {0,2,-.3f,.3f,.62f,.68f,1.88f,1.915f,40,40,46},{0,2,-.03f,.03f,.42f,.88f,1.88f,1.915f,40,40,46},{0,2,-.07f,.07f,.6f,.7f,1.86f,1.92f,90,90,96},
  {0,2,.62f,.66f,.4f,.44f,.82f,1.95f,60,45,25},{0,2,.58f,.7f,.4f,.54f,1.2f,1.4f,80,80,86},{1,3,.2f,.8f,.12f,.62f,-.28f,0.f,75,78,86},
  {0,5,-.5f,.5f,.396f,.404f,1.4f,2.0f,40,210,90},
  {0,1,0.f,.78f,.56f,.86f,-1.92f,-1.42f,52,56,64},{0,1,-.01f,.79f,.56f,.88f,-1.74f,-1.7f,28,28,32},{0,1,-.01f,.79f,.56f,.88f,-1.6f,-1.56f,28,28,32},{0,1,.66f,.72f,.86f,.97f,-1.66f,-1.6f,26,26,30},
  {0,6,-.1f,.82f,.56f,.568f,-1.42f,-1.3f,210,160,30},
  {0,7,-.045f,.045f,.3f,.37f,-1.3f,.85f,64,64,70},{0,7,-.08f,.08f,.28f,.4f,.78f,.9f,95,95,100},{0,7,-.08f,.08f,.28f,.4f,-1.42f,-1.3f,95,95,100},{0,7,-.24f,.24f,.26f,.52f,-1.46f,-1.16f,88,90,98},
  {0,7,-1.f,1.f,.32f,.36f,-1.33f,-1.27f,56,56,62},{0,7,-1.f,1.f,.32f,.37f,1.27f,1.33f,56,56,62},{0,7,.38f,.46f,.33f,.39f,-2.f,.7f,70,60,55},{0,7,.3f,.54f,.3f,.48f,-1.9f,-1.45f,80,76,72},
  {0,8,-.58f,-.28f,.4f,.72f,-2.7f,-2.28f,72,76,88},{0,8,.28f,.58f,.4f,.72f,-2.7f,-2.28f,72,76,88},       // tuyeres des propulseurs (voiture-fusee)
  {0,8,-.5f,-.36f,.46f,.66f,-2.72f,-2.69f,255,150,40},{0,8,.36f,.5f,.46f,.66f,-2.72f,-2.69f,255,150,40}};
#define NSB ((int)(sizeof(SYSB) / sizeof(SYSB[0])))
static const Bx *bx_at(int i) { return i < NBX ? &BXS[i] : &SYSB[i - NBX]; }
static int bx_show(const Bx *B, int bayOpen, int trunkOpen) {
  if (camInterior) return B->cond == 0;   // first-person: cabin only, no engine bay / trunk / underbody
  switch (B->cond) {
    case 1: return trunkOpen; case 2: return bayOpen; case 3: return bayOpen || pAtt[7] <= 0; case 4: return ce == 3 && (bayOpen || pAtt[7] <= 0);
    case 5: return bayOpen && radHp < .6f; case 6: return trunkOpen && tankHp < .55f; case 7: return !camInterior && camy - n[4].y < 1.9f; case 8: return VEH[cv].th > 0.f;
    default: return 1;
  }
}
static float gx[64], gy[64], gz3[64], g3x[64], g3y[64], g3z[64];
static const uint8_t HULLS = 0;
// draw one patch; pass 0 = glass of far-side patches, 2 = opaque cells, 3 = glass cells of near patches
#ifndef DBG_HIDE
#define DBG_HIDE 0
#endif
static void drawpat(int pi, const Pat *p, int pass, int facing, float cxm) {
  if (((DBG_HIDE & 1) && PAT[pi].kind == K_HOOD) || ((DBG_HIDE & 2) && PAT[pi].kind == K_TRUNK) || ((DBG_HIDE & 4) && PAT[pi].kind == K_DOOR) || ((DBG_HIDE & 8) && (PAT[pi].kind == K_ROOF || PAT[pi].kind == K_WIN))) return;
  if (camInterior && p->kind == K_WIN && p->grp == 6) return;   // first-person: windshield removed for a clear view
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
    if (glass) { blendA = 150; polyn(xs, ys, count, col); blendA = 0; }
    else if (p->kind == K_HOOD && oHood > .12f && pAtt[0] > 0) { blendA = 100; polyn(xs, ys, count, col); blendA = 0; }   // open hood: see-through
    else polyn(xs, ys, count, col);
  }
  if (pass != 2 && p->kind == K_WIN && p->grp == 6 && (pAtt[6] < pAtt0[6] || dmg > 40)) {   // cracked windshield
    int lx[8] = {0, nu, 0, nu, nu / 2, nu / 2, 0, nu}, ly[8] = {0, 0, nv, nv, 0, nv, nv / 2, nv / 2}; int ck = (nv / 2) * (nu + 1) + nu / 2;
    for (int q = 0; q < 8; q++) { int k = ly[q] * (nu + 1) + lx[q]; if (visible[ck] && visible[k]) line((int)gx[ck], (int)gy[ck], (int)gx[k], (int)gy[k], C(235, 240, 245)); }
  }
}
static void drawbx(int bi) {
  const Bx *B = bx_at(bi); float x[8], y[8], z[8], zz[8]; int any = 0;
  for (int i = 0; i < 8; i++) { fpt(B->fr, (i & 1) ? B->a1 : B->a0, ((i >> 1) & 1) ? B->b1 : B->b0, ((i >> 2) & 1) ? B->c1 : B->c0, &x[i], &y[i], &z[i]);
    float dx = x[i] - camx, dz = z[i] - camz; zz[i] = dx * camhx + dz * camhz; if (zz[i] >= .401f) any = 1; }
  if (!any) return;
  static const uint8_t FB[6][4] = {{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}};
  int id[6]; float dp[6]; for (int f = 0; f < 6; f++) { id[f] = f; dp[f] = (zz[FB[f][0]] + zz[FB[f][1]] + zz[FB[f][2]] + zz[FB[f][3]]) * .25f; }
  for (int i = 1; i < 6; i++) { int a = id[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { id[j + 1] = id[j]; dp[j + 1] = dp[j]; j--; } id[j + 1] = a; dp[j + 1] = v; }
  int r = B->r, g = B->g, b = B->b; float kk = (B->cond >= 3) ? 1.f : 1.9f;
  if (B->fr == 1 && B->cond == 3 && B->a1 == 1 && B->b1 < .7f) { r = ENG[ce].r; g = ENG[ce].g; b = ENG[ce].b; }
  if (exploded || burning) { float ch = exploded ? .35f : 1.f - (fireT * .1f > .65f ? .65f : fireT * .1f); kk *= ch; }          // burnt black
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
// ---- foreground interface: drawn in screen space AFTER the whole 3D scene, so nothing in the bodywork can cover it
static void frect(int x, int y, int w, int h, uint16_t c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c); }
static const uint16_t GLYPH[14] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF, 0x6BAD, 0x5FED, 0x7BED, 0x7B6D};   // 3x5 font: 0-9 and R
static void glyph(int x, int y, int g, int sc, uint16_t c) {
  for (int r = 0; r < 5; r++) for (int q = 0; q < 3; q++) if ((GLYPH[g] >> (14 - r * 3 - q)) & 1) frect(x + q * sc, y + r * sc, sc, sc, c);
}
static void ring2(int cx, int cy, int r, int thick, uint16_t c) {
  int ox = cx + r, oy = cy;
  for (int i = 1; i <= 24; i++) { float th = i * .2617994f; int x = cx + (int)(r * fsin(th + 1.5708f)), y = cy - (int)(r * fsin(th));
    for (int t = 0; t < thick; t++) { line(ox, oy + t, x, y + t, c); } ox = x; oy = y; }
}
static void gauge(int cx, int cy, int r, float value, uint16_t needle, int gear) {
  disc(cx, cy, r, C(10, 14, 18)); ring2(cx, cy, r, 1, C(150, 160, 165));
  if (value < 0) value = 0; if (value > 1) value = 1;
  for (int i = 0; i <= 6; i++) { float th = 3.927f - i * .7854f, c = fsin(th + 1.5708f), s = fsin(th);
    line(cx + (int)(r * .72f * c), cy - (int)(r * .72f * s), cx + (int)(r * .95f * c), cy - (int)(r * .95f * s), i >= 5 ? C(230, 70, 55) : C(185, 195, 195)); }
  if (gear >= 0) glyph(cx - 1, cy + 1, gear > 13 ? 0 : gear, 1, C(120, 130, 135));
  float th = 3.927f - value * 4.712f; line(cx, cy, cx + (int)(r * .86f * fsin(th + 1.5708f)), cy - (int)(r * .86f * fsin(th)), needle);
  disc(cx, cy, 1, C(220, 220, 225));
}
static void draw_overlay(float kmh, int gear) {
  curZ = -1; blendA = 0;
  if (flashT > 0.f) { blendA = (int)(flashT * 215.f); frect(0, 0, lw, lh, C(255, 240, 200)); blendA = 0; }   // explosion flash
  int sp = (int)(kmh + .5f), sc = lw >= 150 ? 2 : 1, g = gear < 0 ? 10 : (gear == 0 ? 13 : gear), blink = ((int)(fxClock * 6.f)) & 1;
  float rpm = engineRpm / 7000.f;
  if (camInterior) {                                          // dashboard, steering wheel and gauges, always on top
    int bandY = lh - lh / 5, R = lw * 15 / 100, wx = lw * 40 / 100, wy = lh - R / 3, gr = lh * 8 / 100 + 1;
    frect(0, bandY, lw, lh - bandY, C(26, 30, 36)); frect(0, bandY, lw, 1, C(95, 105, 112));
    gauge(lw * 62 / 100, lh - gr - 2, gr, rpm, C(245, 80, 55), -1);
    gauge(lw * 80 / 100, lh - gr - 2, gr, kmh / (engV * 3.6f * (turbo ? 1.5f : 1.f) + 1.f), C(240, 205, 85), g);
    glyph(lw * 24 / 100, bandY + 4, manualGear ? 11 : 12, 1, manualGear ? ACC : C(120, 130, 135));
    float a = -steer * 2.6f;                                   // the wheel turns with the front wheels
    ring2(wx, wy, R, 3, C(58, 62, 70)); ring2(wx, wy, R + 1, 1, C(150, 158, 165)); ring2(wx, wy, R - 2, 1, C(20, 22, 26));
    for (int k = 0; k < 3; k++) { float th = a + 1.5708f * (k == 0 ? 2 : (k == 1 ? 0 : -1)); int x = wx + (int)((R - 2) * fsin(th + 1.5708f)), y = wy - (int)((R - 2) * fsin(th));
      line(wx, wy, x, y, C(58, 62, 70)); line(wx, wy + 1, x, y + 1, C(58, 62, 70)); }
    disc(wx, wy, R / 5 + 1, C(95, 102, 110));   // hub
    int bh = lh - bandY - 5, bx1 = lw * 5 / 100, bx2 = lw * 10 / 100;                       // fuel and engine temperature
    frect(bx1, bandY + 3, 3, bh, C(34, 38, 44)); int ff = (int)(bh * fuel); frect(bx1, bandY + 3 + bh - ff, 3, ff, fuel < .2f ? C(235, 70, 55) : C(240, 205, 85));
    frect(bx2, bandY + 3, 3, bh, C(34, 38, 44)); float tt = engTemp / 1.6f; if (tt > 1.f) tt = 1.f; int tf = (int)(bh * tt); frect(bx2, bandY + 3 + bh - tf, 3, tf, engTemp > 1.f ? C(235, 70, 55) : (engTemp > .75f ? C(240, 190, 70) : C(90, 180, 235)));
    if (burning && blink) frect(lw * 15 / 100, bandY + 3, 5, 5, C(255, 60, 40));             // fire lamp
    else if (tankHp < .6f && fuel > .01f) frect(lw * 15 / 100, bandY + 3, 5, 5, C(255, 170, 40));   // fuel leak lamp
    if (abFront && abT < 3.f) {                               // driver airbag: inflates in a flash, then slowly deflates
      float gr = abT < .12f ? abT / .12f : (abT < 1.2f ? 1.f : 1.f - (abT - 1.2f) / 1.8f); if (gr < 0.f) gr = 0.f;
      int ar = (int)(R * 1.6f * gr); blendA = 235; disc(wx, wy - R / 2, ar, C(226, 229, 234)); blendA = 0; ring2(wx, wy - R / 2, ar, 1, C(150, 156, 164));
    }
    return;
  }
  // chase view: shift lights, big speed, gear ladder (M/A R 1..5), fuel + temperature
  int big = sc == 2 ? 3 : 2, chw = 6, chh = 7, SW = 8 * (chw + 1) - 1, W = SW + 6, H = 8 + 5 * big + 2 + chh + 2 + 5 + 3, x0 = lw - W - 3, y0 = lh - H - 3;
  int man = manualGear, ry = y0 + 8, sy = ry + 5 * big + 2, fy = sy + chh + 2;
  uint16_t ac = man ? ACC : C(240, 205, 85), dark = C(10, 12, 18);
  blendA = 215; frect(x0, y0, W, H, dark); blendA = 0; frect(x0, y0, W, 1, man ? ACC : C(70, 78, 90));
  { int lit = (int)(rpm * 12.f + .5f); if (lit > 12) lit = 12; int over = man && rpm > .9f && blink;
    for (int i = 0; i < 12; i++) frect(x0 + 3 + i * 4, y0 + 3, 3, 3, i < lit ? (over || i >= 10 ? C(235, 70, 55) : (i >= 7 ? C(240, 205, 85) : C(80, 210, 120))) : C(40, 44, 52)); }
  int d[3] = {sp / 100 % 10, sp / 10 % 10, sp % 10}, started = 0;
  for (int i = 0; i < 3; i++) if (d[i] || started || i == 2) { started = 1; glyph(x0 + 3 + i * 4 * big, ry, d[i], big, 0xFFFF); }
  glyph(x0 + 3 + SW - 3 * big, ry, g, big, g == 10 ? C(255, 130, 90) : ac);
  if (man && !burning) { int ax = x0 + 3 + 11 * big + 1;   // shift hints (manual only): green = shift up, orange = shift down
    if (gear > 0 && gear < 5 && rpm > .84f && (blink || rpm < .93f)) for (int r = 0; r < 3; r++) frect(ax + 2 - r, ry + r, 2 * r + 1, 1, C(80, 230, 120));
    if (gear > 1 && rpm < .3f && kmh > 5.f) for (int r = 0; r < 3; r++) frect(ax + r, ry + 5 * big - 3 + r, 5 - 2 * r, 1, C(255, 170, 60)); }
  for (int i = 0; i < 8; i++) {                                        // chips: M/A, R, N, 1..5
    int cx = x0 + 3 + i * (chw + 1), on = i == 0 ? man : (i == 1 ? g == 10 : (i == 2 ? g == 13 : g == i - 2)), gl = i == 0 ? (man ? 11 : 12) : (i == 1 ? 10 : (i == 2 ? 13 : i - 2));
    uint16_t bgc = i == 0 ? (man ? ACC : C(44, 48, 56)) : (on ? (i == 1 ? C(255, 130, 90) : ac) : C(34, 38, 44));
    uint16_t fgc = i == 0 ? (man ? dark : C(150, 156, 166)) : (on ? dark : C(110, 116, 128));
    frect(cx, sy, chw, chh, bgc); glyph(cx + 1, sy + 1, gl, 1, fgc); }
  frect(x0 + 3, fy, SW, 2, C(40, 44, 52)); frect(x0 + 3, fy, (int)(SW * fuel), 2, fuel < .2f ? C(235, 70, 55) : C(240, 205, 85));
  float tt = engTemp / 1.6f; if (tt > 1.f) tt = 1.f; frect(x0 + 3, fy + 3, SW, 2, C(40, 44, 52)); frect(x0 + 3, fy + 3, (int)(SW * tt), 2, engTemp > 1.f ? C(235, 70, 55) : (engTemp > .75f ? C(240, 190, 70) : C(90, 180, 235)));
  if (burning && blink) frect(x0, y0, W, 1, C(255, 70, 40));
  { int tx0 = x0 - 10, ty0 = y0 + H - 12;                                              // tyre pressure: top view, front row first
    for (int w = 0; w < 4; w++) { float pr = tirePres[w]; uint16_t tc = wAtt[w] <= 0 ? C(70, 30, 30) : (pr > .8f ? C(80, 210, 120) : (pr > .4f ? C(240, 205, 85) : C(235, 70, 55)));
      if (tpLeak[w] && blink && wAtt[w] > 0) tc = C(255, 255, 255);
      frect(tx0 + (w & 1) * 5, ty0 + (1 - (w >> 1)) * 6, 4, 5, tc); } }
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
  if (camInterior) oh = ot = od = 0.f;
  if (oh > .02f && pAtt[0] > 0) { ri[22] = rot(22, 25, 24, oh * 1.3f, 0, cxm, czm, rhx, rhz); ri[23] = rot(23, 25, 24, oh * 1.3f, 0, cxm, czm, rhx, rhz); }
  if (ot > .02f && pAtt[3] > 0) { ri[34] = rot(34, 37, 36, ot * 1.2f, 0, cxm, czm, rhx, rhz); ri[35] = rot(35, 37, 36, ot * 1.2f, 0, cxm, czm, rhx, rhz); }
  if (od > .02f && pAtt[4] > 0) { ri[38] = rot(38, 39, 40, od * 1.1f, 1, cxm, czm, rhx, rhz); ri[41] = rot(41, 39, 40, od * 1.1f, 1, cxm, czm, rhx, rhz); }
  if (od > .02f && pAtt[5] > 0) { ri[42] = rot(42, 43, 44, od * 1.1f, 1, cxm, czm, rhx, rhz); ri[45] = rot(45, 43, 44, od * 1.1f, 1, cxm, czm, rhx, rhz); }
  static Pat PL[NPAT]; for (int k = 0; k < NPAT; k++) { PL[k] = PAT[k]; for (int q = 0; q < 4; q++) PL[k].n[q] = ri[PAT[k].n[q]]; }
  if (structure) {
    float dummy = 0; (void)dummy;
    for (int i = 0; i < nbc; i++) if (bm[i].f != 2 && bm[i].o == 0 && bm[i].f != 3) { float d = (bm[i].l0 - bm[i].lr) / bm[i].lr; d = (d < 0 ? -d : d) * 12; if (d > 1) d = 1; float x0, y0, x1, y1;
      if (proj(PX[bm[i].a], PY[bm[i].a], PZ[bm[i].a], &x0, &y0) && proj(PX[bm[i].b], PY[bm[i].b], PZ[bm[i].b], &x1, &y1)) line((int)x0, (int)y0, (int)x1, (int)y1, C(255, 255 - (int)(d * 230), 255 - (int)(d * 255))); }
    float hx_, hz_; heading(&hx_, &hz_); for (int w = 14; w < 18; w++) { float ax = hz_, az = -hx_; wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(WHL[cw].cr, WHL[cw].cg, WHL[cw].cb)); }
    for (int b = NBX; b < NBX + NSB; b++) { if (bx_at(b)->cond == 8 && VEH[cv].th <= 0.f) continue; drawbx(b); }                   // x-ray: radiator, tank, gearbox, driveshaft...
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
  int it[128]; float dp[128]; int ni = 0; int bayOpen = oh > .05f || pAtt[0] <= 0, trunkOpen = ot > .05f || pAtt[3] <= 0;
  for (int k = 0; k < NPAT; k++) if (PL[k].cls == 1 && !fc[k]) { it[ni] = k; dp[ni++] = pd[k]; }
  for (int b = 0; b < NBX + NSB; b++) { const Bx *Q = bx_at(b); if (!bx_show(Q, bayOpen, trunkOpen)) continue;
    float x, y, z; fpt(Q->fr, (Q->a0 + Q->a1) / 2, (Q->b0 + Q->b1) / 2, (Q->c0 + Q->c1) / 2, &x, &y, &z); it[ni] = 100 + b; dp[ni++] = (x - camx) * camhx + (z - camz) * camhz; }
  for (int w = 14; w < 18; w++) { int sw = (w & 1) ? 1 : -1; if (camInterior || sw * camSide > 0) continue; it[ni] = 200 + w; dp[ni++] = (n[w].x - camx) * camhx + (n[w].z - camz) * camhz; }
  for (int i = 1; i < ni; i++) { int a = it[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { it[j + 1] = it[j]; dp[j + 1] = dp[j]; j--; } it[j + 1] = a; dp[j + 1] = v; }
  const Whl *W = &WHL[cw];
  for (int q = 0; q < ni; q++) { int a = it[q];
    if (a >= 200) { int w = a - 200; float ax = hz_, az = -hx_; if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); } wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); }
    else if (a >= 100) drawbx(a - 100); else drawpat(a, &PL[a], 2, 0, cxm); }
  // opaque outer patches facing the camera (parts also show their inside when turned away)
  int ok[NPAT], no = 0; float od2[NPAT];
  for (int k = 0; k < NPAT; k++) if (PL[k].cls == 0 && (fc[k] || PL[k].two)) { ok[no] = k; od2[no++] = pd[k]; }
  for (int i = 1; i < no; i++) { int a = ok[i]; float v = od2[i]; int j = i - 1; while (j >= 0 && od2[j] < v) { ok[j + 1] = ok[j]; od2[j + 1] = od2[j]; j--; } ok[j + 1] = a; od2[j + 1] = v; }
  for (int q = 0; q < no; q++) drawpat(ok[q], &PL[ok[q]], 2, fc[ok[q]], cxm);
  for (int w = 14; w < 18; w++) { int sw = (w & 1) ? 1 : -1; if (camInterior || sw * camSide <= 0) continue; float ax = hz_, az = -hx_; if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); } wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); }
  for (int q = 0; q < no; q++) { int k = ok[q]; if (fc[k] && (PL[k].kind == K_WIN || PL[k].kind == K_DOOR)) drawpat(k, &PL[k], 3, 1, cxm); }
}
// ---- fire, smoke, coolant steam and the explosion fireball (world-space sprites, painted in the car's depth slot)
static void puff(float x, float y, float z, float r, uint16_t c) {
  float sx, sy; if (!proj(x, y, z, &sx, &sy)) return;
  float d = (x - camx) * camhx + (z - camz) * camhz; if (d < .5f) return;
  int rr = (int)(r * FOC / d + .5f); if (rr < 1) rr = 1; if (rr > 28) rr = 28; disc((int)sx, (int)sy, rr, c);
}
static void fire_src(float wx, float wy, float wz, float k, int seed) {      // k = intensity 0..1
  for (int i = 0; i < 7; i++) {                                              // smoke first, flames on top
    float ph = fxClock * .45f + i * .143f + seed * .31f; ph -= (int)ph; float fl = 1.f - ph * .5f; int g = 74 - (int)(ph * 36);
    puff(wx + fsin(i * 12.9f + seed) * .5f * fl, wy + .9f + ph * 2.4f * (.5f + k * .5f), wz + fsin(i * 7.7f + 2.f + seed) * .5f * fl, (.22f + ph * .55f) * (.5f + k * .5f), C(g, g, g + 4));
  }
  for (int i = 0; i < 12; i++) {
    float ph = fxClock * 2.2f + i * .0837f + seed * .27f; ph -= (int)ph; float fl = 1.f - ph * .6f, r = (.34f - ph * .26f) * (.45f + k * .75f);
    uint16_t c = ph < .25f ? C(255, 235, 120) : (ph < .55f ? C(255, 160, 45) : C(225, 70, 30));
    puff(wx + fsin(i * 12.9f + seed) * .4f * fl, wy + ph * (.9f + k * .9f), wz + fsin(i * 7.7f + 2.f + seed) * .4f * fl, r, c);
  }
}
static void draw_jets(void) {                      // flammes des propulseurs : deux jets derriere les tuyeres
  float x, y, z;
  for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 9; i++) {
    float ph = i / 8.f, fl = 1.f + .3f * fsin(fxClock * 45.f + i * 1.7f + s * 2.f), len = ph * 3.4f * fl;
    uint16_t c = ph < .2f ? C(255, 250, 220) : (ph < .5f ? C(255, 190, 70) : (ph < .8f ? C(255, 110, 35) : C(210, 60, 30)));
    fpt(0, s * .43f + fsin(fxClock * 31.f + i * 2.3f + s) * .04f * ph, .56f, -2.75f - len, &x, &y, &z);
    puff(x, y, z, (.2f - ph * .13f) * fl, c);
  }
}
static void draw_fx(void) {
  int jet = VEH[cv].th > 0.f && turbo && fuel > .005f && !exploded;
  if (!jet && !burning && steamI <= 0.f && boomT > 2.f) return;
  curZ = -1; blendA = 0; float x, y, z;
  if (jet) draw_jets();
  if (steamI > 0.f && !burning) { fpt(0, 0.f, .85f, 1.7f, &x, &y, &z);                    // coolant steam from the radiator
    for (int i = 0; i < 8; i++) { float ph = fxClock * .8f + i * .125f; ph -= (int)ph; int g = 235 - (int)(ph * 70);
      puff(x + fsin(i * 5.1f) * .35f, y + ph * 1.5f, z + fsin(i * 3.7f) * .35f, (.1f + ph * .3f) * (.5f + steamI * .5f), C(g, g, g + 6 > 255 ? 255 : g + 6)); } }
  if (burning) {
    fpt(0, .4f, .8f, -1.65f, &x, &y, &z); fire_src(x, y, z, fireI, 0);                  // the tank burns first
    if (fireT > 2.5f || exploded) { float k2 = exploded ? fireI : fireI * ((fireT - 2.5f) * .3f > 1.f ? 1.f : (fireT - 2.5f) * .3f);
      fpt(0, 0.f, .9f, 1.45f, &x, &y, &z); fire_src(x, y, z, k2, 1); }                  // then the engine bay
  }
  if (boomT < 1.6f) { fpt(0, .4f, .8f, -1.65f, &x, &y, &z);                               // expanding fireball
    for (int i = 0; i < 10; i++) { float a = i * 2.399f, rr = boomT * (3.5f + (i % 3) * 1.6f), r = (.7f + boomT * 1.6f) * (1.f - boomT * .45f);
      uint16_t c = boomT < .35f ? C(255, 245, 190) : (boomT < .8f ? ((i & 1) ? C(255, 205, 80) : C(255, 140, 40)) : (boomT < 1.2f ? ((i & 1) ? C(235, 110, 40) : C(190, 60, 30)) : C(70, 66, 64)));
      puff(x + fsin(a) * rr * .6f, y + boomT * (2.f + (i % 4)) * .9f, z + fsin(a + 1.5708f) * rr * .6f, r, c); } }
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
  int bix = index % 8 - 4, biz = index / 8 - 4; unsigned hh = city_hash(bix, biz); int tone = hh & 1, kd = B->kind;
  if (kd == 2) draw_prism3(x, y, z, 150 + (hh >> 4 & 31), 105 + (hh >> 9 & 31), 90 + (hh >> 14 & 15));
  else if (kd == 1) draw_prism3(x, y, z, tone ? 78 : 62, tone ? 108 : 92, tone ? 130 : 118);
  else draw_prism3(x, y, z, tone ? 118 : 145, tone ? 128 : 150, tone ? 135 : 158);
  uint16_t glass = kd == 1 ? C(70, 120, 150) : C(42, 74, 88), edge = C(75, 83, 88);
  float dd = (B->x - camx) * camhx + (B->z - camz) * camhz; if (dd > 48.f) return;   // no window detail far away
  for (int floor = 1; floor * 3.f < B->h; floor++) {
    float yy = floor * 3.f;
    for (int col = 0; col < 4; col++) {
      float a = B->x - 7.f + col * 4.f, b = a + 2.25f, c = B->z - 8.f + col * 4.f, d = c + 2.25f, y1 = yy + 1.45f;
      float wx[4] = {a, b, b, a}, wy[4] = {yy, yy, y1, y1}, sx[4], sy[4];
      float wz[4] = {B->z + 11.02f, B->z + 11.02f, B->z + 11.02f, B->z + 11.02f}; int ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(wx[q], wy[q], wz[q], &sx[q], &sy[q])) ok = 0;
      if (ok && camz > B->z + 11.f) polyn(sx, sy, 4, glass);
      wx[0] = b; wx[1] = a; wx[2] = a; wx[3] = b; wz[0] = wz[1] = wz[2] = wz[3] = B->z - 11.02f; ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(wx[q], wy[q], wz[q], &sx[q], &sy[q])) ok = 0;
      if (ok && camz < B->z - 11.f) polyn(sx, sy, 4, glass);
      float sideZ[4] = {c, d, d, c}, sideX[4] = {B->x + 8.52f, B->x + 8.52f, B->x + 8.52f, B->x + 8.52f}; ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(sideX[q], wy[q], sideZ[q], &sx[q], &sy[q])) ok = 0;
      if (ok && camx > B->x + 8.5f) polyn(sx, sy, 4, glass);
      sideX[0] = sideX[1] = sideX[2] = sideX[3] = B->x - 8.52f; sideZ[0] = d; sideZ[1] = c; sideZ[2] = c; sideZ[3] = d; ok = 1;
      for (int q = 0; q < 4; q++) if (!proj(sideX[q], wy[q], sideZ[q], &sx[q], &sy[q])) ok = 0;
      if (ok && camx < B->x - 8.5f) polyn(sx, sy, 4, glass);
    }
    float ax0, ay0, ax1, ay1; if (camx < B->x - 8.5f && proj(B->x - 8.49f, yy, B->z, &ax0, &ay0) && proj(B->x - 8.49f, yy + .04f, B->z, &ax1, &ay1)) line((int)ax0, (int)ay0, (int)ax1, (int)ay1, edge);
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
static void draw_city_park(int index) {
  CityBlock *B = &cityBlocks[index]; int ix = index % 8 - 4, iz = index / 8 - 4; unsigned hh = city_hash(ix, iz);
  float dd = (B->x - camx) * camhx + (B->z - camz) * camhz; if (dd > 60.f) return;
  for (int t = 0; t < 7; t++) {
    float u = ((hh >> (t * 4)) & 15) / 15.f, v = (city_hash(ix + t * 3, iz - t * 5) & 255) / 255.f, tx = B->x + (u * 2.f - 1.f) * 6.5f, tz = B->z + (v * 2.f - 1.f) * 9.5f;
    float hs = 1.f + ((hh >> (t + 3)) & 3) * .25f;
    city_box(tx, tz, .18f, .18f, 0.f, 1.5f * hs, 95, 70, 45); city_box(tx, tz, 1.1f * hs, 1.1f * hs, 1.5f * hs, 3.4f * hs, 40 + t * 3, 108 + (t & 1) * 14, 50);
  }
  city_box(B->x, B->z + 2.f, .5f, .3f, .05f, .5f, 120, 90, 60);   // bench
}
static void city_manhole(float x, float z) {
  float sx[12], sy[12];
  for (int i = 0; i < 12; i++) { float a = i * .523599f, wx = x + .58f * fsin(a + 1.5708f), wz = z + .58f * fsin(a); if (!proj(wx, .025f, wz, &sx[i], &sy[i])) return; }
  polyn(sx, sy, 12, C(53, 57, 58));
  for (int i = 0; i < 12; i++) line((int)sx[i], (int)sy[i], (int)sx[(i + 1) % 12], (int)sy[(i + 1) % 12], C(115, 119, 116));
  for (int i = 0; i < 3; i++) { float a = i * 1.0472f, ax = x + .34f * fsin(a + 1.5708f), az = z + .34f * fsin(a), bx = x - .34f * fsin(a + 1.5708f), bz = z - .34f * fsin(a); city_line(ax, .03f, az, bx, .03f, bz, C(82, 87, 86)); }
}
static int city_occluded(float x, float y, float z) {
  float dx = x - camx, dz = z - camz, depth = dx * camhx + dz * camhz;
  if (depth <= .5f) return 0;
  for (int i = 0; i < 64; i++) {
    CityBlock *B = &cityBlocks[i];
    float bx = B->x - camx, bz = B->z - camz, blockDepth = bx * camhx + bz * camhz;
    if (blockDepth <= .5f || blockDepth >= depth - 1.f) continue;
    float t = blockDepth / depth, rayX = camx + dx * t, rayZ = camz + dz * t, rayY = camy + (y - camy) * t;
    if (rayX > B->x - 8.5f && rayX < B->x + 8.5f && rayZ > B->z - 11.f && rayZ < B->z + 11.f && rayY > 0.f && rayY < B->h) return 1;
  }
  return 0;
}
static void city_inter(int index, float *ox, float *oz) {
  int ix = index % 8 - 4, iz = index / 8 - 4; float x0, z0, x1, z1, x2, z2, x3, z3;
  city_center(ix, iz, &x0, &z0); city_center(ix + 1, iz, &x1, &z1); city_center(ix, iz + 1, &x2, &z2); city_center(ix + 1, iz + 1, &x3, &z3);
  *ox = (x0 + x1 + x2 + x3) * .25f; *oz = (z0 + z1 + z2 + z3) * .25f;
}
static void draw_city_details(int index) {
  int ix = index % 8 - 4, iz = index / 8 - 4;
  float x0, z0, x1, z1, x2, z2, x3, z3;
  city_center(ix, iz, &x0, &z0); city_center(ix + 1, iz, &x1, &z1);
  city_center(ix, iz + 1, &x2, &z2); city_center(ix + 1, iz + 1, &x3, &z3);
  float x = (x0 + x1 + x2 + x3) * .25f, z = (z0 + z1 + z2 + z3) * .25f;
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
  float signX = x - 4.3f, signZ = z + 4.8f;
  if (!city_occluded(signX, 2.f, signZ)) {
    city_line(signX, 0, signZ, signX, 2.55f, signZ, pole);
    city_box(signX, signZ, .58f, .10f, 2.25f, 2.78f, 42, 116, 70);
    int signColor = index % 3 == 0 ? C(205, 65, 48) : (index % 3 == 1 ? C(50, 135, 82) : C(42, 105, 151));
    city_box(signX, signZ, .46f, .10f, 1.55f, 2.12f, (signColor >> 11) * 255 / 31, ((signColor >> 5) & 63) * 255 / 63, (signColor & 31) * 255 / 31);
    city_line(x - 4.62f, 2.50f, z + 4.91f, x - 4.03f, 2.50f, z + 4.91f, C(220, 225, 210));
    city_line(x - 4.55f, 1.68f, z + 4.91f, x - 4.05f, 1.68f, z + 4.91f, C(220, 225, 210));
  }
  city_manhole(x + 2.4f, z + 2.4f);
  int sp = index % 16;
  if (sp == 0) {
    city_box(x - 7.f, z - 5.f, 1.1f, .5f, .05f, .85f, 65, 118, 170);
    city_box(x - 3.8f, z - 5.f, 1.1f, .5f, .05f, .85f, 65, 118, 170);
    city_box(x + 1.f, z - 6.f, 4.f, 2.f, .05f, 3.8f, 182, 165, 117);
    city_box(x + 1.f, z - 3.92f, 2.2f, .08f, 2.2f, 3.25f, 175, 55, 42);
  } else if (sp == 1) {
    city_box(x, z - 11.1f, 5.f, .18f, .15f, 2.8f, 96, 103, 105);
    for (int bay = 0; bay < 3; bay++) {
      float bx = x - 3.2f + bay * 3.2f;
      city_line(bx, .2f, z - 11.25f, bx, 2.55f, z - 11.25f, C(62, 69, 71));
    }
    city_box(x, z + 3.f, 6.f, 4.f, .02f, .05f, 58, 60, 61);
    for (int bay = 0; bay < 4; bay++) { float bx = x - 4.5f + bay * 3.f; city_line(bx, .06f, z, bx, .06f, z + 6.f, C(220, 215, 185)); }
  } else if (sp == 2) {
    for (int cone = 0; cone < 4; cone++) { float cx = x - 5.f + cone * 3.f, cz = z - 5.f; city_box(cx, cz, .32f, .32f, .05f, .6f, 230, 110, 38); city_box(cx, cz, .24f, .24f, .6f, .67f, 235, 220, 190); }
    city_box(x + 5.f, z - 4.f, .65f, .65f, .05f, 1.35f, 192, 145, 70);
  } else if (sp == 3) {
    for (int space = 0; space < 5; space++) { float bx = x - 7.f + space * 3.4f; city_line(bx, .04f, z - 7.f, bx, .04f, z - 1.f, C(228, 224, 200)); }
    city_box(x + 5.f, z + 4.f, 1.3f, 1.3f, .04f, 1.25f, 55, 105, 65);
  }
}
static void draw_city_wall(int index) {
  float x[8], y[8], z[8];
  for (int i = 0; i < 8; i++) {
    if (index < 2) { x[i] = index == 0 ? -(CITY_X_LIMIT + 1.f) : CITY_X_LIMIT; z[i] = (i & 4) ? CITY_Z_LIMIT : -CITY_Z_LIMIT; }
    else { x[i] = (i & 1) ? CITY_X_LIMIT : -CITY_X_LIMIT; z[i] = index == 2 ? -(CITY_Z_LIMIT + 1.f) : CITY_Z_LIMIT; }
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
  for (int iz = -4; iz < 4; iz++) for (int ix = -4; ix < 4; ix++) {
    int ax = ix, az = iz; city_center(ax, az, &cityBlocks[k].x, &cityBlocks[k].z);
    cityBlocks[k].h = city_height(ax, az); cityBlocks[k].kind = city_kind(ax, az); k++;
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
static uint16_t comp_col(float h) { return h > .7f ? C(80, 220, 100) : (h > .35f ? C(240, 200, 60) : C(255, 60, 50)); }
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
  if (full) {                                                                          // seat belts and airbags
    int bs = beltOn ? ((abFront || abSideL || abSideR) ? 2 : 1) : 0; uint16_t bc = bs == 0 ? C(255, 60, 50) : (bs == 2 ? C(255, 200, 60) : C(80, 220, 100)), wh = C(235, 238, 245);
    for (int sd = 0; sd < 2; sd++) { int sx = (int)(cx0 + (sd ? .45f : -.45f) * S * gsx), sy = (int)(cy0 + .15f * S * gsz);
      line(sx - 4, sy - 6, sx + 4, sy + 5, bc); line(sx - 3, sy - 6, sx + 5, sy + 5, bc);
      if (abFront) { int ay = (int)(cy0 - .6f * S * gsz); disc(sx, ay, 6, wh); ring2(sx, ay, 6, 1, C(150, 156, 164)); } }
    for (int sd = 0; sd < 2; sd++) if (sd ? abSideR : abSideL) { int x = (int)(cx0 + (sd ? 1.f : -1.f) * .9f * S * gsx); for (int q = 0; q < 2; q++) line(x + q, (int)(cy0 - .8f * S * gsz), x + q, (int)(cy0 + .6f * S * gsz), wh); }
  }
  {                                                                                     // radiator (front), fuel tank (rear), driveshaft (centre line)
    int hwR = (int)(.6f * S * gsx), yR = (int)(cy0 - 1.78f * S * gsz), yT = (int)(cy0 + 1.7f * S * gsz), y0s = (int)(cy0 - .9f * S * gsz), y1s = (int)(cy0 + 1.3f * S * gsz), xc = (int)cx0;
    for (int q = 0; q <= (full ? 1 : 0); q++) { line(xc - hwR, yR + q, xc + hwR, yR + q, comp_col(radHp)); line(xc - hwR, yT + q, xc + hwR, yT + q, comp_col(tankHp)); line(xc + q, y0s, xc + q, y1s, comp_col(shaftHp)); }
    int r = full ? 4 : 2, ym = (y0s + y1s) / 2;
    if (radHp < .3f) { line(xc - r, yR - r, xc + r, yR + r, C(255, 40, 40)); line(xc - r, yR + r, xc + r, yR - r, C(255, 40, 40)); }
    if (tankHp < .3f) { line(xc - r, yT - r, xc + r, yT + r, C(255, 40, 40)); line(xc - r, yT + r, xc + r, yT - r, C(255, 40, 40)); }
    if (shaftHp < .3f) { line(xc - r, ym - r, xc + r, ym + r, C(255, 40, 40)); line(xc - r, ym + r, xc + r, ym - r, C(255, 40, 40)); }
    if (burning && (((int)(fxClock * 6.f)) & 1)) disc(xc, yT - (full ? 6 : 3), full ? 4 : 2, C(255, 150, 40));
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
  for (int by = 0; by < 224; by += 8) {
    for (int r = 0; r < 8; r++) { const uint16_t *src = &fb[((by + r) * lh / 224) * lw]; uint16_t *d = &buf[r * w]; for (int x = 0; x < w; x++) d[x] = src[xmap[xd0 + x]]; }
    eadk_display_push_rect((eadk_rect_t){xd0, by, w, 8}, buf);
  }
}
static void draw_bot(int slot) {          // a physical bot is drawn by the very same car() and draw_fx() as the player's car
  Bot *b = &bots[slot]; float sw = wspin, sh = oHood, sd = oDoor, st = oTrunk; int ss = structure, sci = camInterior;
  camInterior = 0; bot_enter(b); wspin = b->spin; oHood = oDoor = oTrunk = 0.f; structure = 0;
  car(); draw_fx();
  bot_leave(b); wspin = sw; oHood = sh; oDoor = sd; oTrunk = st; structure = ss; camInterior = sci;
}
static float depth_of(float x, float z) { return (x - camx) * camhx + (z - camz) * camhz; }
// delivery destination: translucent ring on the ground + a tall blinking pillar (visible from far away)
static void draw_marker(void) {
  curZ = -1; blendA = 0;
  float d = depth_of(delTx, delTz), hw = .3f + d * .012f, sx[12], sy[12]; int bl = ((int)(fxClock * 4.f)) & 1, ok = 1;
  for (int i = 0; i < 12 && ok; i++) { float a = i * .5236f; ok = proj(delTx + 5.f * fsin(a + 1.5708f), .06f, delTz + 5.f * fsin(a), &sx[i], &sy[i]); }
  if (ok) { blendA = 120; polyn(sx, sy, 12, C(255, 180, 40)); blendA = 0; }
  city_box(delTx, delTz, hw, hw, 0.f, 18.f + d * .2f, 255, bl ? 190 : 120, 30);
}
// ---- particles: sparks, smoke, dust clouds and debris chunks (world space, small ring-buffer pool)
typedef struct { float x, y, z, vx, vy, vz, life, max, size; uint8_t type, cr, cg, cb; } Part;   // type: 0 smoke, 1 spark, 2 dust, 3 debris
#define PART_MAX 160
static Part parts[PART_MAX]; static int partHead, fxSkip;
static void part_clear(void) { for (int i = 0; i < PART_MAX; i++) parts[i].life = 0.f; partHead = 0; }
static void part_add(float x, float y, float z, float vx, float vy, float vz, float life, float size, int type, int cr, int cg, int cb) {
  if (!optPart) return;
  Part *p = &parts[partHead]; partHead = (partHead + 1) % PART_MAX;
  p->x = x; p->y = y; p->z = z; p->vx = vx; p->vy = vy; p->vz = vz; p->life = p->max = life; p->size = size; p->type = (uint8_t)type; p->cr = (uint8_t)cr; p->cg = (uint8_t)cg; p->cb = (uint8_t)cb;
}
static void dust_col(int *r, int *g, int *b) {
  if (mapId == 3) { *r = 128; *g = 126; *b = 120; } else if (mapId == 4) { *r = 210; *g = 210; *b = 212; } else if (mapId == 2) { *r = 125; *g = 125; *b = 130; } else { *r = 130; *g = 108; *b = 76; }
}
static void part_update(float dt) {
  if (dt <= 0.f) return; if (dt > .1f) dt = .1f;
  for (int i = 0; i < PART_MAX; i++) {
    Part *p = &parts[i]; if (p->life <= 0.f) continue;
    p->life -= dt; if (p->life <= 0.f) { p->life = 0.f; continue; }
    p->x += p->vx * dt; p->y += p->vy * dt; p->z += p->vz * dt;
    if (p->type == 1 || p->type == 3) {
      p->vy -= (p->type == 1 ? 12.f : 14.f) * dt; if (p->type == 1) { p->vx *= .985f; p->vz *= .985f; }
      float gy = gh(p->x, p->z) + .04f; if (p->y < gy) { p->y = gy; p->vy = -p->vy * .35f; p->vx *= .7f; p->vz *= .7f; }
    } else {
      float k = 1.f - 1.6f * dt; if (k < 0.f) k = 0.f; p->vx *= k; p->vz *= k; p->vy += (p->type == 0 ? .7f : (p->type == 4 ? 1.2f : .1f)) * dt; p->size += dt * (p->type == 0 ? .9f : (p->type == 4 ? 2.f : .7f));
    }
  }
}
static void draw_particles(void) {
  for (int i = 0; i < PART_MAX; i++) {
    Part *p = &parts[i]; if (p->life <= 0.f) continue;
    float sx, sy, d = depth_of(p->x, p->z); if (d < .6f || d > zmax - 2.f || !proj(p->x, p->y, p->z, &sx, &sy)) continue;
    float f = p->life / p->max; int zq = (int)(d * zsc) - 4; curZ = zq < 0 ? 0 : (zq > 254 ? 254 : zq);
    if (p->type == 1) {                                                         // spark: streak, white-yellow -> orange -> red
      float bx, by; uint16_t c = f > .6f ? C(255, 245, 170) : (f > .3f ? C(255, 170, 50) : C(220, 70, 30));
      if (proj(p->x - p->vx * .04f, p->y - p->vy * .04f, p->z - p->vz * .04f, &bx, &by)) line((int)bx, (int)by, (int)sx, (int)sy, c); else px((int)sx, (int)sy, c);
    } else if (p->type == 4) {                                                  // fireball puff: white-yellow -> orange -> red -> smoke
      int rr = (int)(p->size * FOC / d + .5f); if (rr < 1) rr = 1; if (rr > 22) rr = 22;
      uint16_t c = f > .7f ? C(255, 240, 170) : (f > .4f ? C(255, 160, 50) : (f > .2f ? C(210, 70, 30) : C(70, 66, 64))); blendA = 70 + (int)(f * 130.f); disc((int)sx, (int)sy, rr, c); blendA = 0;
    } else if (p->type == 3) {                                                  // debris chunk
      int rr = (int)(p->size * FOC / d + .5f); if (rr < 1) rr = 1; if (rr > 3) rr = 3; disc((int)sx, (int)sy, rr, C(p->cr, p->cg, p->cb));
    } else {                                                                    // smoke / dust: translucent disc that grows and fades
      int rr = (int)(p->size * FOC / d + .5f); if (rr < 1) rr = 1; if (rr > 16) rr = 16;
      float sh = p->type == 0 ? 1.f : .8f + .2f * f; blendA = 25 + (int)(f * (p->type == 0 ? 120.f : 95.f));
      disc((int)sx, (int)sy, rr, C((int)(p->cr * sh), (int)(p->cg * sh), (int)(p->cb * sh))); blendA = 0;
    }
  }
  curZ = -1; blendA = 0;
}
static float ipvx, ipvy, ipvz, ipcx, ipcz;
static void impact_fx(float dt) {                 // once per frame: a sudden loss of speed means a collision (or a hard landing)
  float vx = 0, vy = 0, vz = 0, cx = 0, cy = 0, cz = 0;
  for (int i = 0; i < 20; i++) { vx += n[i].vx * .05f; vy += n[i].vy * .05f; vz += n[i].vz * .05f; cx += n[i].x * .05f; cy += n[i].y * .05f; cz += n[i].z * .05f; }
  float dvx = ipvx - vx, dvy = ipvy - vy, dvz = ipvz - vz, mx = cx - ipcx, mz = cz - ipcz; int skip = fxSkip > 0; if (fxSkip > 0) fxSkip--;
  ipvx = vx; ipvy = vy; ipvz = vz; ipcx = cx; ipcz = cz;
  if (skip || dt <= 0.f || mx * mx + mz * mz > 100.f) return;
  float hm = fsqrt(dvx * dvx + dvz * dvz); int gr, gg, gb; dust_col(&gr, &gg, &gb);
  if (hm > 2.f && hm < 60.f) {
    float k = hm > 12.f ? 12.f : hm, ux = dvx / hm, uz = dvz / hm, ox = cx + ux * 1.6f, oz = cz + uz * 1.6f, oy = cy;
    int dark = dmg > 60.f || burning, sm = 150 - (dmg > 50.f ? 50 : (int)dmg);
    int nsp = 2 + (int)(k * 2.f), nsm = 3 + (int)(k * .6f), ndb = 2 + (int)(k * .7f), ndu = 2 + (int)(k * .4f);
    for (int i = 0; i < nsp; i++) part_add(ox, oy, oz, -ux * k * .35f + (fx_rand() - .5f) * 6.f, 1.f + fx_rand() * 4.f, -uz * k * .35f + (fx_rand() - .5f) * 6.f, .35f + fx_rand() * .4f, .05f, 1, 255, 220, 120);
    for (int i = 0; i < nsm; i++) { int g = dark ? 55 + (int)(fx_rand() * 25.f) : sm - (int)(fx_rand() * 30.f); part_add(ox, oy, oz, -ux * .5f + (fx_rand() - .5f) * 1.6f, .6f + fx_rand() * 1.4f, -uz * .5f + (fx_rand() - .5f) * 1.6f, 1.2f + fx_rand() * 1.2f, .35f + fx_rand() * .3f, 0, g, g, g + 4); }
    for (int i = 0; i < ndb; i++) { int body = fx_rand() < .5f; part_add(ox, oy, oz, -ux * k * .25f + (fx_rand() - .5f) * 5.f, 2.f + fx_rand() * 3.f, -uz * k * .25f + (fx_rand() - .5f) * 5.f, 1.f + fx_rand() * .8f, .07f,
      3, body ? VEH[cv].r : 40, body ? VEH[cv].g : 40, body ? VEH[cv].b : 44); }
    for (int i = 0; i < ndu; i++) part_add(ox, cy - .4f, oz, (fx_rand() - .5f) * 2.f, .3f + fx_rand() * .6f, (fx_rand() - .5f) * 2.f, 1.f + fx_rand() * 1.f, .3f + fx_rand() * .3f, 2, gr, gg, gb);
  }
  if (dvy < -3.f && dvy > -40.f) {                // hard landing: dust ring at every wheel
    float k = -dvy > 12.f ? 12.f : -dvy;
    for (int w = 0; w < 4; w++) { Node *p = &n[14 + w]; if (wAtt[w] <= 0) continue;
      for (int q = 0; q < 2 + (int)(k * .3f); q++) part_add(p->x, p->y - p->r + .1f, p->z, (fx_rand() - .5f) * 4.f, .3f + fx_rand() * .8f, (fx_rand() - .5f) * 4.f, .9f + fx_rand() * .9f, .25f + fx_rand() * .2f, 2, gr, gg, gb); }
  }
}
static void blast_fx(void) {                      // barrel explosions queued by the physics
  for (int b = 0; b < blastN; b++) {
    float x = blastP[b][0], y = blastP[b][1], z = blastP[b][2];
    for (int i = 0; i < 16; i++) { float a = fx_rand() * 6.2832f, sp = 1.5f + fx_rand() * 5.f; part_add(x, y + .3f, z, fsin(a) * sp, 1.5f + fx_rand() * 4.f, fsin(a + 1.5708f) * sp, .5f + fx_rand() * .6f, .45f + fx_rand() * .4f, 4, 255, 220, 120); }
    for (int i = 0; i < 14; i++) { float a = fx_rand() * 6.2832f, sp = 4.f + fx_rand() * 8.f; part_add(x, y + .3f, z, fsin(a) * sp, 2.f + fx_rand() * 8.f, fsin(a + 1.5708f) * sp, .5f + fx_rand() * .6f, .05f, 1, 255, 220, 120); }
    for (int i = 0; i < 8; i++) { int g = 45 + (int)(fx_rand() * 25.f); part_add(x + (fx_rand() - .5f), y + .5f, z + (fx_rand() - .5f), (fx_rand() - .5f) * 2.f, 1.f + fx_rand() * 2.f, (fx_rand() - .5f) * 2.f, 1.6f + fx_rand() * 1.2f, .6f + fx_rand() * .4f, 0, g, g, g + 4); }
    for (int i = 0; i < 6; i++) { float a = fx_rand() * 6.2832f, sp = 2.f + fx_rand() * 5.f; int red = fx_rand() < .6f; part_add(x, y + .3f, z, fsin(a) * sp, 4.f + fx_rand() * 4.f, fsin(a + 1.5708f) * sp, 1.2f + fx_rand() * .8f, .08f, 3, red ? 205 : 40, red ? 40 : 40, red ? 35 : 44); }
  }
  blastN = 0;
}
static void airbag_fx(void) {                     // white powder cloud when an airbag fires
  if (!abBang) return; abBang = 0;
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  for (int i = 0; i < 7; i++) part_add(cx + hx * .5f + (fx_rand() - .5f) * .8f, cy + .5f + fx_rand() * .4f, cz + hz * .5f + (fx_rand() - .5f) * .8f,
    (fx_rand() - .5f) * 2.5f, .6f + fx_rand() * .8f, (fx_rand() - .5f) * 2.5f, .7f + fx_rand() * .5f, .3f + fx_rand() * .2f, 0, 235, 236, 240);
}
// ---- tyre marks: braking, drifting and burn-outs leave dark strips on the ground
typedef struct { float x0, y0, z0, x1, y1, z1; } Skid;
#define SK_MAX 256
static Skid sk[SK_MAX]; static int skHead, skCnt; static float skLX[4], skLY[4], skLZ[4]; static uint8_t skOn[4];
static void skid_clear(void) { skHead = skCnt = 0; for (int i = 0; i < 4; i++) skOn[i] = 0; }
static void skid_update(void) {
  if (!optSkid) { skid_clear(); return; }                                  // once per frame, player car only
  float hx, hz; heading(&hx, &hz);
  for (int w = 0; w < 4; w++) {
    Node *p = &n[14 + w]; int on = 0;
    if (p->gnd && wAtt[w] <= 0) { float s2 = fsqrt(p->vx * p->vx + p->vz * p->vz);       // bare rim on the ground: sparks
      if (s2 > 4.f && fx_rand() < .8f) for (int q = 0; q < 2; q++) part_add(p->x, p->y - p->r + .05f, p->z, (fx_rand() - .5f) * 3.f, 1.f + fx_rand() * 2.5f, (fx_rand() - .5f) * 3.f - p->vz * .1f, .25f + fx_rand() * .3f, .05f, 1, 255, 220, 120); }
    if (p->gnd && wAtt[w] > 0 && p->y < 12.f) {
      float spd = fsqrt(p->vx * p->vx + p->vz * p->vz), along = p->vx * hx + p->vz * hz, lat = p->vx * hz - p->vz * hx; if (lat < 0) lat = -lat;
      on = (brk > .4f && along > 4.f) || (lat > 2.6f && spd > 6.f) || (thr > .8f && spd < 8.f) || (hbrake > .3f && w < 2 && spd > 3.f);
    }
    if (!on) { skOn[w] = 0; continue; }
    float x = p->x, z = p->z, y = p->y - p->r + .05f;
    { float s2 = fsqrt(p->vx * p->vx + p->vz * p->vz); if (s2 > 4.f && fx_rand() < .55f)                       // tyre smoke
        part_add(x, y + .1f, z, p->vx * .15f + (fx_rand() - .5f), .5f + fx_rand() * .8f, p->vz * .15f + (fx_rand() - .5f), .8f + fx_rand() * .7f, .22f + fx_rand() * .15f, 0, 200, 200, 205); }
    if (skOn[w]) { float dx = x - skLX[w], dz = z - skLZ[w], d2 = dx * dx + dz * dz;
      if (d2 > 9.f) skOn[w] = 0;                                  // teleport / jump: start a new strip
      else if (d2 > .12f) { sk[skHead] = (Skid){skLX[w], skLY[w], skLZ[w], x, y, z}; skHead = (skHead + 1) % SK_MAX; if (skCnt < SK_MAX) skCnt++; skLX[w] = x; skLY[w] = y; skLZ[w] = z; } }
    if (!skOn[w]) { skOn[w] = 1; skLX[w] = x; skLY[w] = y; skLZ[w] = z; }
  }
}
static void draw_skids(void) {
  for (int q = 0; q < skCnt; q++) {
    Skid *s = &sk[(skHead - 1 - q + SK_MAX * 2) % SK_MAX]; float d = depth_of(s->x1, s->z1);
    if (d < .8f || d > zmax - 2.f) continue;
    float side = (s->x1 - camx) * camhz - (s->z1 - camz) * camhx; if (side < -d * 1.5f - 2.f || side > d * 1.5f + 2.f) continue;
    float dx = s->x1 - s->x0, dz = s->z1 - s->z0, l = fsqrt(dx * dx + dz * dz) + 1e-4f, ox = -dz / l * .14f, oz = dx / l * .14f, xs[4], ys[4];
    if (!proj(s->x0 + ox, s->y0, s->z0 + oz, &xs[0], &ys[0]) || !proj(s->x0 - ox, s->y0, s->z0 - oz, &xs[1], &ys[1]) ||
        !proj(s->x1 - ox, s->y1, s->z1 - oz, &xs[2], &ys[2]) || !proj(s->x1 + ox, s->y1, s->z1 + oz, &xs[3], &ys[3])) continue;
    int zq = (int)(d * zsc) - 6; curZ = zq < 0 ? 0 : (zq > 254 ? 254 : zq); blendA = 60 + (skCnt - q) * 110 / skCnt;
    polyn(xs, ys, 4, C(16, 16, 18));
  }
  curZ = -1; blendA = 0;
}
// ---- vertical loop (map 4): helical ring of quads, drawn in two halves (far / near) so the car can be sorted between them
#define NLSEG 24
static void draw_loop(int li, int part) {
  float lx = LOOPS[li][0], lz = LOOPS[li][1], R = LOOPS[li][2], dc = depth_of(lx, lz), dph = 6.2831853f / NLSEG, sd[NLSEG]; int ord[NLSEG], cnt = 0;
  for (int k = 0; k < NLSEG; k++) { float p = (k + .5f) * dph; sd[k] = depth_of(lx, lz + R * fsin(p)); if ((sd[k] >= dc) == (part == 0)) ord[cnt++] = k; }
  for (int i = 1; i < cnt; i++) { int a = ord[i]; float v = sd[a]; int j = i - 1; while (j >= 0 && sd[ord[j]] < v) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = a; }
  for (int q = 0; q < cnt; q++) {
    int k = ord[q]; float p0 = k * dph, p1 = (k + 1) * dph, y0 = R - R * fsin(p0 + 1.5708f), z0 = lz + R * fsin(p0), y1 = R - R * fsin(p1 + 1.5708f), z1 = lz + R * fsin(p1);
    float xc0 = lx - LOOP_L * .5f + LOOP_L * p0 / 6.2831853f, xc1 = lx - LOOP_L * .5f + LOOP_L * p1 / 6.2831853f;
    float X[4] = {xc0 - LOOP_HW, xc0 + LOOP_HW, xc1 + LOOP_HW, xc1 - LOOP_HW}, Y[4] = {y0, y0, y1, y1}, Z[4] = {z0, z0, z1, z1}, sx[4], sy[4]; int ok = 1;
    for (int i = 0; i < 4; i++) if (!proj(X[i], Y[i], Z[i], &sx[i], &sy[i])) ok = 0;
    if (!ok) continue;
    polyn(sx, sy, 4, (k & 1) ? C(235, 125, 30) : C(62, 66, 74));
    line((int)sx[0], (int)sy[0], (int)sx[3], (int)sy[3], C(235, 235, 235)); line((int)sx[1], (int)sy[1], (int)sx[2], (int)sy[2], C(235, 235, 235));
  }
}
static void scene(int xd0) {
  int xl = xd0 * lw / 320; terrain(xl); draw_skids();
  struct { int ty, idx; float d; } L[160]; int m = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  L[m].ty = 0; L[m].idx = 0; L[m++].d = depth_of(cx, cz);
  if (ntr >= 0) { L[m].ty = 1; L[m].idx = 0; L[m++].d = depth_of(n[ntr + 4].x, n[ntr + 4].z); }
  for (int o = 0; o < nobj && m < 26; o++) { int b = nob0 + o * 8; L[m].ty = 2; L[m].idx = o; L[m++].d = depth_of(n[b].x, n[b].z); }
  if (mapId == 3) { make_city(); for (int b = 0; b < 64 && m < 140; b++) {
      float bd = depth_of(cityBlocks[b].x, cityBlocks[b].z), bs = (cityBlocks[b].x - camx) * camhz - (cityBlocks[b].z - camz) * camhx;
      if (bd >= -14.f && bd <= zmax + 16.f && bs >= -bd * 1.7f - 18.f && bs <= bd * 1.7f + 18.f) { L[m].ty = 3; L[m].idx = b; L[m++].d = bd; }
      float ix_, iz_; city_inter(b, &ix_, &iz_); bd = depth_of(ix_, iz_); bs = (ix_ - camx) * camhz - (iz_ - camz) * camhx;
      if (bd >= -14.f && bd <= zmax + 16.f && bs >= -bd * 1.7f - 18.f && bs <= bd * 1.7f + 18.f) { L[m].ty = 8; L[m].idx = b; L[m++].d = bd; } }
    if (trafficEnabled) for (int a = 0; a < trafficCount; a++) {
      if (traffic[a].bot) { if (m < 96) { L[m].ty = 7; L[m].idx = traffic[a].bot - 1; L[m++].d = depth_of(traffic[a].x, traffic[a].z); } continue; }
      float dx = traffic[a].x - camx, dz = traffic[a].z - camz, d = dx * camhx + dz * camhz, side = dx * camhz - dz * camhx;
      if (d < 1.f || d >= zmax - 2.f || side < -d * 1.45f - 3.f || side > d * 1.45f + 3.f) continue;
      L[m].ty = 4; L[m].idx = a; L[m++].d = d;
    }
    if (pursuitEnabled && m < 95) { if (policeCar.bot) { L[m].ty = 7; L[m].idx = policeCar.bot - 1; } else { L[m].ty = 6; L[m].idx = 0; } L[m++].d = depth_of(policeCar.x, policeCar.z); }
    for (int w = 0; w < 4 && m < 96; w++) { float cxw = camx < -CITY_X_LIMIT ? -CITY_X_LIMIT : (camx > CITY_X_LIMIT ? CITY_X_LIMIT : camx), czw = camz < -CITY_Z_LIMIT ? -CITY_Z_LIMIT : (camz > CITY_Z_LIMIT ? CITY_Z_LIMIT : camz);   // nearest point of the (long) wall
      float x = w < 2 ? (w == 0 ? -CITY_X_LIMIT - .5f : CITY_X_LIMIT + .5f) : cxw, z = w < 2 ? czw : (w == 2 ? -CITY_Z_LIMIT - .5f : CITY_Z_LIMIT + .5f); L[m].ty = 5; L[m].idx = w; L[m++].d = depth_of(x, z); } }
  if (mapId == 4 && optRamp) for (int li = 0; li < NLOOP && m < 150; li++) {
    float lx = LOOPS[li][0], lz = LOOPS[li][1], R = LOOPS[li][2], dc = depth_of(lx, lz), bs = (lx - camx) * camhz - (lz - camz) * camhx;
    if (dc >= -14.f && dc <= zmax + 16.f && bs >= -dc * 1.7f - 20.f && bs <= dc * 1.7f + 20.f) { L[m].ty = 9; L[m].idx = li * 2; L[m++].d = dc + R; L[m].ty = 9; L[m].idx = li * 2 + 1; L[m++].d = dc - R; }
  }
  if (delOn && !delEnd && m < 150) { L[m].ty = 10; L[m].idx = 0; L[m++].d = depth_of(delTx, delTz); }
  for (int i = 1; i < m; i++) { __typeof__(L[0]) v = L[i]; int j = i - 1; while (j >= 0 && L[j].d < v.d) { L[j + 1] = L[j]; j--; } L[j + 1] = v; }
  for (int k = 0; k < m; k++) {
    if (L[k].ty == 0) { car(); draw_fx(); draw_particles(); }
    else if (L[k].ty == 1) { trailer_wheels(0); draw_box(ntr, 150, 150, 160); trailer_wheels(1); }
    else if (L[k].ty == 2 && L[k].d > 1 && L[k].d < zmax - 2 && !objDead[L[k].idx]) { const Obj *O = &OBJ[okind[L[k].idx]]; draw_box(nob0 + L[k].idx * 8, O->r, O->g, O->b); }
    if (L[k].ty == 3 && L[k].d > 1 && L[k].d < zmax - 2) { if (cityBlocks[L[k].idx].kind == 3) draw_city_park(L[k].idx); else draw_city_block(L[k].idx); }
    else if (L[k].ty == 8 && L[k].d > 1 && L[k].d < zmax - 2) draw_city_details(L[k].idx);
    
    else if (L[k].ty == 4 && L[k].d > 1 && L[k].d < zmax - 2) draw_traffic(L[k].idx);
    else if (L[k].ty == 5 && L[k].d > 1 && L[k].d < zmax - 2) draw_city_wall(L[k].idx);
    else if (L[k].ty == 6 && L[k].d > 1 && L[k].d < zmax - 2) draw_police();
    else if (L[k].ty == 7 && L[k].d > 1 && L[k].d < zmax - 2) draw_bot(L[k].idx);
    else if (L[k].ty == 9) draw_loop(L[k].idx >> 1, L[k].idx & 1);
    else if (L[k].ty == 10 && L[k].d > 1.f && L[k].d < 200.f) draw_marker();
  }
  if (weatherMode == 1 || weatherMode == 3) {
    uint16_t streak = weatherMode == 1 ? C(155, 190, 210) : C(205, 218, 220);
    for (int i = 0; i < 14; i++) { int x = (i * 37 + (int)(signalClock * 45.f)) % (lw - 3) + 2, y = (i * 29 + (int)(signalClock * 71.f)) % (lh - 5); line(x, y, x - 2, y + 5, streak); }
  }
  if (showTop && xd0 == 0 && !camInterior) { int bw = lw / 4, bh = lh * 40 / 112; for (int y = lh - bh; y < lh; y++) for (int x = 0; x < bw; x++) fb[y * lw + x] = C(18, 20, 30); clipOn = 1; clX0 = 0; clX1 = bw; clY0 = lh - bh; clY1 = lh; draw_top(bw / 2, lh - bh / 2, 6.5f * lw / 160, 0); clipOn = 0; }
}
// ---- settings, controls
enum { A_ACC, A_BRK, A_LEFT, A_RIGHT, A_RESET, A_HOOD, A_DOORS, A_TRUNK, A_ALL, A_BEAMS, A_TOP, A_DMG, A_CL, A_CR, A_CU, A_CD, A_ZI, A_ZO, A_CRESET, A_QUICK, A_TURBO, A_CINT, A_SKIP, A_QLAST, A_QUICK2, A_GUP, A_GDN, A_TRANS, A_CLUTCH, A_HBRAKE, NA };
static const char *AN[NA] = {"Accelerer", "Freiner", "Gauche", "Droite", "Remettre/Rejouer", "Capot", "Portes", "Coffre", "Tout ouvrir", "Poutres", "Vue dessus", "Degats", "Camera gauche", "Camera droite", "Camera haut", "Camera bas", "Zoom +", "Zoom -", "Camera reset", "Menu rapide", "Turbo (tenir)", "Vue habitacle", "Tutoriel: etape suivante", "Refaire option rapide", "Menu rapide (acces direct)", "Rapport superieur", "Rapport inferieur", "Boite auto/manuelle", "Embrayage (tenir)", "Frein a main (tenir)"};
static const int BDEF[NA] = {eadk_key_up, eadk_key_down, eadk_key_left, eadk_key_right, eadk_key_ok, eadk_key_var, eadk_key_xnt, eadk_key_exp, eadk_key_shift, eadk_key_toolbox, eadk_key_ln, eadk_key_log, eadk_key_four, eadk_key_six, eadk_key_eight, eadk_key_two, eadk_key_seven, eadk_key_nine, eadk_key_five, eadk_key_exe, eadk_key_backspace, eadk_key_zero, eadk_key_dot, eadk_key_plus, eadk_key_comma, eadk_key_three, eadk_key_one, eadk_key_minus, 13 /* alpha */, 23 /* power */};
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
  uint16_t bg = sel ? PSEL : PNL;
  eadk_display_push_rect_uniform((eadk_rect_t){x, y, w, h}, bg);
  if (sel) eadk_display_push_rect_uniform((eadk_rect_t){x, y, 3, h}, ACC);
  txt(s, x + 9, y + (h - 14) / 2, 0, sel ? 0xFFFF : C(185, 188, 198), bg);
}
static int pressed(uint64_t k, int key) { return ((k >> key) & 1) && !((pk >> key) & 1); }
static int down(uint64_t k, int key) { return (k >> key) & 1; }
static void clear(void) { eadk_display_push_rect_uniform(eadk_screen_rect, BGC); }
static void garage_car(void) { car_init(mapId == 0 ? TA : 0, 0, 0, 1, .02f, 0); }
static int ct, stopT, score; static float vpk, impV;
static int tutOn, tutStep, tutFlag, tutOk, evDmg, evRepair, mapBak;
static float tutH0x, tutH0z;
static void startpos(void) {
  car_init(mapId == 0 ? TA : 0, 0, 0, 1, .4f, 1); ct = 0; stopT = 0; vpk = 0; impV = 0; score = 0;
  traffic_reset(); skid_clear(); part_clear(); fxSkip = 3;
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
// ---- records (best lap, best delivery score per level).
// The EADK API has no standard storage call that I could confirm, so by default records live in RAM only (lost on exit).
// To make them persistent, compile with -DNB_STORAGE and provide:
//   int nb_store_read(void *dst, unsigned size);        // returns 1 when `size` bytes were read
//   void nb_store_write(const void *src, unsigned size);
#define REC_MAGIC 0x4E423344u
static uint32_t rec_sum(const uint32_t *d, int cnt) { uint32_t s = 0x4E42u; for (int i = 0; i < cnt; i++) s = s * 31u + d[i]; return s; }
static void rec_save(void) {
  uint32_t d[6] = {REC_MAGIC, (uint32_t)delBest[0], (uint32_t)delBest[1], (uint32_t)delBest[2], (uint32_t)recLap, 0}; d[5] = rec_sum(d, 5);
#ifdef NB_STORAGE
  nb_store_write(d, sizeof(d));
#else
  (void)d;
#endif
}
static void rec_load(void) {
#ifdef NB_STORAGE
  uint32_t d[6]; if (nb_store_read(d, sizeof(d)) && d[0] == REC_MAGIC && d[5] == rec_sum(d, 5)) { delBest[0] = (int)d[1]; delBest[1] = (int)d[2]; delBest[2] = (int)d[3]; recLap = (int)d[4]; }
#endif
}
// ---- livraisons: timed delivery challenge in the city (several parcels, one countdown per parcel, dedicated screens)
static const int DELN[3] = {3, 5, 7};
static const char *DLN[3] = {"Facile", "Normal", "Expert"};
static void deliv_pick(float px, float pz) {
  static const float DV[3] = {7.f, 9.5f, 12.f}, DM[3] = {20.f, 14.f, 10.f};
  float bx = px + 60.f, bz = pz, bd = -1.f;
  for (int t = 0; t < 12; t++) {
    int ix = (int)(fx_rand() * 8.f) - 4, iz = (int)(fx_rand() * 8.f) - 4; if (ix > 3) ix = 3; if (iz > 3) iz = 3;
    float cx, cz; city_center(ix, iz, &cx, &cz);
    float x = cx + (fx_rand() < .5f ? -14.f : 14.f), z = cz + (fx_rand() < .5f ? -9.f : 9.f);   // on a street, away from the speed bumps
    if (x < -CITY_X_LIMIT + 8.f) x = -CITY_X_LIMIT + 8.f; if (x > CITY_X_LIMIT - 8.f) x = CITY_X_LIMIT - 8.f;
    if (z < -CITY_Z_LIMIT + 8.f) z = -CITY_Z_LIMIT + 8.f; if (z > CITY_Z_LIMIT - 8.f) z = CITY_Z_LIMIT - 8.f;
    float d = absf(x - px) + absf(z - pz);
    if (d > bd) { bd = d; bx = x; bz = z; }
    if (d > 70.f && d < 170.f) { bd = d; bx = x; bz = z; break; }
  }
  delTx = bx; delTz = bz; delLimit = DM[delLvl] + bd / DV[delLvl]; delT = delLimit;
}
static void deliv_finish(int why) {                 // why: 1 = all parcels handled, 2 = car destroyed
  int sum = 0, ok = 0; for (int i = 0; i < delN; i++) { sum += legScore[i]; ok += legOk[i]; }
  delPen = (int)(dmg * 3.f); delFinal = sum - delPen; if (delFinal < 0) delFinal = 0;
  delStars = why == 2 ? 0 : (ok * 2 >= delN) + (ok == delN) + (ok == delN && dmg < 25.f);
  if (ok > 0 && delFinal > delBest[delLvl]) { delBest[delLvl] = delFinal; rec_save(); }
  delEnd = why;
}
static void deliv_begin(void) {
  fxSeed ^= (unsigned)eadk_timing_millis() * 2654435761u;
  startpos(); delN = DELN[delLvl]; delIdx = 0; delEnd = 0; delScore = 0; delTotal = 0.f; delMsgT = 0.f; delDeadT = 0.f; fuel = 1.f;
  for (int i = 0; i < 8; i++) { legOk[i] = 0; legScore[i] = 0; legTime[i] = 0.f; }
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); deliv_pick(cx, cz);
}
static void deliv_update(float dt) {
  if (delEnd) return;
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  delTotal += dt; delT -= dt; if (delMsgT > 0.f) delMsgT -= dt;
  float dx = cx - delTx, dz = cz - delTz; int done = 0;
  if (dx * dx + dz * dz < 36.f) {                                       // parcel delivered
    int sc = 100 + (int)(delT * 20.f); if (delT > delLimit * .5f) sc += 100;
    legOk[delIdx] = 1; legScore[delIdx] = sc; legTime[delIdx] = delLimit - delT; delScore += sc;
    delMsgKind = 1; delMsgScore = sc; delMsgT = 2.5f; fuel = 1.f; done = 1;
  } else if (delT <= 0.f) {                                             // too late: this parcel is lost
    legOk[delIdx] = 0; legScore[delIdx] = 0; legTime[delIdx] = delLimit; delMsgKind = 2; delMsgT = 2.5f; done = 1;
  }
  if (done) { delIdx++; if (delIdx >= delN) { deliv_finish(1); return; } deliv_pick(cx, cz); }
  if (driveEff < .05f) delDeadT += dt; else delDeadT = 0.f;
  if (exploded || (burning && fireT > 8.f) || delDeadT > 6.f) deliv_finish(2);
}
static void draw_deliv_hud(void) {
  curZ = -1; blendA = 0; if (delEnd) return;
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  float ex = delTx - cx, ez = delTz - cz, ef = ex * hx + ez * hz, er = ex * hz - ez * hx, a = fatan2(er, ef);
  int mx = lw / 2, ar = 9, ax = mx - 20, ay = 14, blink = ((int)(fxClock * 6.f)) & 1; uint16_t ac = C(255, 170, 30);
  blendA = 205; frect(mx - 33, 2, 66, 30, C(10, 12, 18)); blendA = 0; frect(mx - 33, 2, 66, 1, ACC);
  disc(ax, ay, ar, C(14, 18, 24)); ring2(ax, ay, ar, 1, C(150, 160, 165));
  int tx = ax + (int)(ar * .8f * fsin(a)), ty = ay - (int)(ar * .8f * fsin(a + 1.5708f)), bx = ax - (int)(ar * .45f * fsin(a)), by = ay + (int)(ar * .45f * fsin(a + 1.5708f));
  line(bx, by, tx, ty, ac); line(bx + 1, by, tx + 1, ty, ac);
  for (int s = -1; s <= 1; s += 2) { float th = a + 3.1416f + .5f * s; line(tx, ty, tx + (int)(5.f * fsin(th)), ty - (int)(5.f * fsin(th + 1.5708f)), ac); }
  int secs = (int)(delT + .99f); if (secs < 0) secs = 0; if (secs > 999) secs = 999;
  int dg[3] = {secs / 100 % 10, secs / 10 % 10, secs % 10}, started = 0; uint16_t tc = (delT < 8.f && blink) ? C(255, 70, 55) : 0xFFFF;
  for (int i = 0; i < 3; i++) if (dg[i] || started || i == 2) { glyph(mx - 6 + i * 8, 6, dg[i], 2, tc); started = 1; }
  float f = delT / delLimit; if (f < 0.f) f = 0.f; if (f > 1.f) f = 1.f;
  frect(mx - 29, 21, 58, 2, C(40, 44, 52)); frect(mx - 29, 21, (int)(58 * f), 2, f > .5f ? C(80, 210, 120) : (f > .25f ? C(240, 205, 85) : C(235, 70, 55)));
  int x0 = mx - delN * 3;
  for (int i = 0; i < delN; i++) frect(x0 + i * 6, 26, 4, 3, i < delIdx ? (legOk[i] ? C(80, 210, 120) : C(235, 70, 55)) : (i == delIdx ? (blink ? ACC : C(150, 80, 20)) : C(60, 64, 72)));
}
static void deliv_bar(int kmh) {
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  float ex = delTx - cx, ez = delTz - cz, dist = fsqrt(ex * ex + ez * ez);
  char s[96], *p = s; p = num(p, kmh); p = cat(p, "km/h L"); p = num(p, delIdx + 1 > delN ? delN : delIdx + 1); p = cat(p, "/"); p = num(p, delN);
  p = cat(p, " "); p = num(p, (int)dist); p = cat(p, "m Sc "); p = num(p, delScore); p = cat(p, " D"); p = num(p, (int)dmg); p = cat(p, "%");
  uint16_t col = 0xFFFF;
  if (delMsgT > 0.f) { if (delMsgKind == 1) { p = cat(p, " OK +"); num(p, delMsgScore); col = C(120, 255, 140); } else { cat(p, " RETARD"); col = C(255, 110, 90); } }
  eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14)); txt(s, 4, 225, 0, col, C(10, 10, 14));
}
static void draw_deliv_menu(int sel) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 152, 240}, BGC);
  eadk_display_push_rect_uniform((eadk_rect_t){152, 0, 2, 240}, ACC);
  eadk_display_push_rect_uniform((eadk_rect_t){154, 224, 166, 16}, BGC);
  txt("LIVRAISONS", 10, 6, 1, 0xFFFF, BGC); txt("defis chronometres", 6, 30, 0, C(130, 134, 146), BGC);
  eadk_display_push_rect_uniform((eadk_rect_t){6, 45, 140, 1}, C(60, 62, 70));
  char s[40], *p = s; p = cat(p, "Niveau: "); cat(p, DLN[delLvl]); row(s, 4, 50, 144, 17, sel == 0);
  row("Lancer", 4, 69, 144, 17, sel == 1); row("Retour", 4, 88, 144, 17, sel == 2);
  p = s; p = cat(p, "Colis: "); num(p, DELN[delLvl]); txt(s, 8, 118, 0, 0xFFFF, BGC);
  p = s; p = cat(p, "Record: "); num(p, delBest[delLvl]); txt(s, 8, 134, 0, ACC, BGC);
  txt("Suis la fleche et", 8, 158, 0, C(150, 154, 166), BGC); txt("rejoins le pilier", 8, 172, 0, C(150, 154, 166), BGC);
  txt("orange avant la fin", 8, 186, 0, C(150, 154, 166), BGC); txt("du chrono. Vite = bonus", 8, 200, 0, C(150, 154, 166), BGC);
  txt("Degats: malus", 8, 214, 0, C(150, 154, 166), BGC);
  txt("Livraisons - Ville", 162, 226, 0, ACC, BGC);
}
static void draw_deliv_res(void) {
  clear(); char s[48], *p;
  if (delEnd == 2) txt("VOITURE HORS SERVICE", 60, 4, 1, C(255, 90, 80), BGC); else txt("LIVRAISONS TERMINEES", 55, 4, 1, ACC, BGC);
  p = s; p = cat(p, "Niveau "); cat(p, DLN[delLvl]); txt(s, 118, 26, 0, C(150, 160, 190), BGC);
  for (int i = 0; i < delN; i++) { p = s; p = cat(p, "Livraison "); p = num(p, i + 1); p = cat(p, ": "); uint16_t col;
    if (legOk[i]) { p = dec1(p, legTime[i]); p = cat(p, "s  +"); num(p, legScore[i]); col = C(120, 230, 130); }
    else if (i < delIdx) { cat(p, "retard"); col = C(255, 110, 90); }
    else { cat(p, "--"); col = C(110, 116, 128); }
    txt(s, 60, 44 + i * 15, 0, col, BGC); }
  int y0 = 44 + delN * 15 + 6;
  p = s; p = cat(p, "Degats: "); p = num(p, (int)dmg); p = cat(p, "%  -"); num(p, delPen); txt(s, 60, y0, 0, 0xFFFF, BGC);
  p = s; p = cat(p, "Score: "); num(p, delFinal); txt(s, 60, y0 + 16, 1, C(255, 210, 80), BGC);
  p = s; p = cat(p, "Record: "); num(p, delBest[delLvl]); txt(s, 60, y0 + 38, 0, C(150, 160, 190), BGC);
  { char st[4]; for (int i = 0; i < 3; i++) st[i] = i < delStars ? '*' : '-'; st[3] = 0; txt(st, 240, y0 + 16, 1, C(255, 210, 80), BGC); }
  txt("OK: rejouer   Retour: menu", 60, 224, 0, C(150, 160, 190), BGC);
}
static const char *MAPN[5] = {"Circuit", "Route", "Crash-test", "Ville", "Vide"}, *SOLN[3] = {"Robuste", "Normale", "Fragile"};
static const char *MN[10] = {"Jouer", "Garage", "Carte", "Solidite", "Test", "Reglages", "Commandes", "Tutoriel", "Livraisons", "Quitter"};
static float menuT;
static void menu_init(void) { garage_car(); traffic_reset(); skid_clear(); part_clear(); menuT = 0.f; }
static void menu_bg(void) {                // slow cinematic orbit around the selected map, running live physics / traffic
  thr = 0; brk = 0; steer = 0; turbo = 0;
  for (int i = 0; i < 4; i++) world_step(.0035f);
  traffic_update(.05f); fx_update(.05f); bots_frame(.05f); menuT += .05f;
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  float ang = menuT * .14f + .6f, dist = 12.f + 4.f * fsin(menuT * .09f) + (ntr >= 0 ? 3.f : 0.f), el = 1.6f + 1.4f * (fsin(menuT * .06f) + 1.f);
  camhx = fsin(ang); camhz = fsin(ang + 1.5708f); camx = cx - camhx * dist; camz = cz - camhz * dist; camy = cy + el;
  float gy = gh(camx, camz) + 1.2f; if (camy < gy) camy = gy;
  int xd = 154, xl = xd * lw / 320; CXc = xl + (lw - xl) / 2; hor = (int)(lh * .62f - (camy - cy) * foc / dist);
  int sv = showTop, sci = camInterior, ss = structure; showTop = 0; camInterior = 0; structure = 0; oHood = oDoor = oTrunk = 0.f;
  scene(xd); showTop = sv; camInterior = sci; structure = ss; present(xd);
}
static void draw_menu(int sel) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 152, 240}, BGC);
  eadk_display_push_rect_uniform((eadk_rect_t){152, 0, 2, 240}, ACC);
  eadk_display_push_rect_uniform((eadk_rect_t){154, 224, 166, 16}, BGC);
  txt("NUMBEAM", 10, 6, 1, 0xFFFF, BGC); txt("3D", 114, 6, 1, ACC, BGC); txt("simulateur de collision", 6, 30, 0, C(130, 134, 146), BGC);
  eadk_display_push_rect_uniform((eadk_rect_t){6, 45, 140, 1}, C(60, 62, 70));
  for (int i = 0; i < 10; i++) { char s[40], *p = s; p = cat(p, MN[i]);
    if (i == 2) { p = cat(p, ": "); cat(p, MAPN[mapId]); } else if (i == 3) { p = cat(p, ": "); cat(p, SOLN[solid]); } else if (i == 4) { p = cat(p, ": "); p = num(p, CRASHV[crashI]); cat(p, " km/h"); }
    row(s, 4, 50 + i * 17, 144, 15, i == sel); }
  txt("Haut/Bas  Gauche/Droite  OK", 4, 226, 0, C(110, 114, 126), BGC);
  char t[40], *q = t; q = cat(q, "Carte: "); cat(q, MAPN[mapId]); txt(t, 162, 226, 0, ACC, BGC);
}
static void grow(const char *l, const char *v, int y, int sel) {
  uint16_t bg = sel ? PSEL : PNL; eadk_display_push_rect_uniform((eadk_rect_t){0, y, 160, 14}, bg);
  if (sel) eadk_display_push_rect_uniform((eadk_rect_t){0, y, 3, 14}, ACC);
  txt(l, 8, y, 0, sel ? 0xFFFF : C(150, 154, 166), bg); if (v && *v) txt(v, 72, y, 0, sel ? ACC : 0xFFFF, bg);
}
static void gbar(const char *l, float f, int y) {
  if (f < 0) f = 0; if (f > 1) f = 1;
  eadk_display_push_rect_uniform((eadk_rect_t){0, y, 160, 13}, BGC); txt(l, 6, y - 1, 0, C(150, 154, 166), BGC);
  eadk_display_push_rect_uniform((eadk_rect_t){50, y + 3, 104, 7}, C(40, 42, 50)); eadk_display_push_rect_uniform((eadk_rect_t){51, y + 4, (int)(102 * f), 5}, ACC);
}
static void draw_garage(int sel, int gv) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 160, 240}, BGC);
  eadk_display_push_rect_uniform((eadk_rect_t){0, 16, 160, 1}, ACC); txt("GARAGE", 8, 1, 0, 0xFFFF, BGC); txt(VEH[cv].nm, 80, 1, 0, ACC, BGC);
  const char *lab[12] = {"Vue 3D", "Vehicule", "Generer", "Roues", "Susp.", "Moteur", "Chassis", "Remorque", "Capot", "Portes", "Coffre", "JOUER"};
  char seed[16], *sp = seed; *sp++ = '#'; num(sp, (int)genSeed);
  const char *val[11] = {"OK: orbite", VEH[cv].nm, seed, WHL[cw].nm, SUS[cs].nm, ENG[ce].nm, CHA[cc].nm, trl ? "oui" : "non", tHood > .5f ? "ouvert" : "ferme", tDoor > .5f ? "ouvertes" : "fermees", tTrunk > .5f ? "ouvert" : "ferme"};
  for (int i = 0; i < 12; i++) grow(lab[i], i < 11 ? val[i] : "", 19 + i * 15, i == sel);
  if (gv) { eadk_display_push_rect_uniform((eadk_rect_t){0, 200, 160, 40}, BGC); txt("Fleches: tourner/hauteur", 4, 200, 0, ACC, BGC); txt("+ / - : zoom", 4, 212, 0, ACC, BGC); txt("OK ou Retour: fin", 4, 224, 0, ACC, BGC); return; }
  gbar("Puiss", ENG[ce].a * VEH[cv].pw / 23.f, 201);
  gbar("Grip", WHL[cw].gr * VEH[cv].gr / .45f, 214);
  gbar("Solid", CHA[cc].k / 140.f * SOLF[solid] / 230.f, 227);
}
static void draw_pause(int sel) {
  const char *it[8] = {"Reprendre", "Recommencer", "Options rapides", "Degats (dessus)", "Garage", "Reglages", "Commandes", "Menu principal"};
  eadk_display_push_rect_uniform((eadk_rect_t){70, 8, 180, 224}, BGC); txt("PAUSE", 140, 12, 0, 0xFFFF, BGC);
  for (int i = 0; i < 8; i++) row(it[i], 80, 30 + i * 25, 160, 21, i == sel);
}
static void opt_toggle(int sel) {                         // settings rows 5..9
  if (sel == 5) { optSkid ^= 1; skid_clear(); }
  else if (sel == 6) { optPart ^= 1; part_clear(); }
  else if (sel == 7) { optObj ^= 1; if (!optObj) objects_clear(); }            // turning objects back on takes effect at the next start
  else if (sel == 8) optRamp ^= 1;
  else if (sel == 9) optPunct ^= 1;
}
static void draw_set(int sel) {
  static const char *QN[3] = {"Basse", "Normale", "Haute"}, *FN[3] = {"Etroit", "Normal", "Large"}, *VN[3] = {"Courte", "Normale", "Longue"};
  static const char *lb[12] = {"Qualite", "Direction", "Camera", "Champ de vision", "Distance de vue", "Traces de pneus", "Particules et fumee", "Objets (barils...)", "Rampes/boucles/ralentis.", "Crevaisons", "Test des touches", "Retour"};
  clear(); txt("REGLAGES", 120, 4, 1, ACC, BGC);
  for (int i = 0; i < 12; i++) { char s[40], *p = s; p = cat(p, lb[i]);
    if (i == 0) { p = cat(p, ": "); cat(p, QN[qual]); } else if (i == 1) { p = cat(p, ": "); num(p, steerL); } else if (i == 2) { p = cat(p, ": "); num(p, camL); }
    else if (i == 3) { p = cat(p, ": "); cat(p, FN[fovI]); } else if (i == 4) { p = cat(p, ": "); cat(p, VN[vdI]); }
    else if (i >= 5 && i <= 9) { int v = i == 5 ? optSkid : (i == 6 ? optPart : (i == 7 ? optObj : (i == 8 ? optRamp : optPunct))); p = cat(p, ": "); cat(p, v ? "Oui" : "Non"); }
    row(s, 40, 24 + i * 16, 240, 14, i == sel); }
  txt("Gauche/Droite: changer   Objets: au prochain depart", 20, 222, 0, C(150, 160, 190), BGC);
}
static void draw_ctrl(int sel, int cap) {
  clear(); txt("COMMANDES", 115, 2, 1, ACC, BGC);
  int top = sel - 4; if (top < 0) top = 0; if (top > NA + 1 - 9) top = NA + 1 - 9;
  for (int i = 0; i < 9; i++) { int a = top + i; char s[48], *p = s; if (a > NA) break;
    if (a == NA) cat(s, "Restaurer les defauts"); else { p = cat(p, AN[a]); p = cat(p, ": "); cat(p, KN[bind[a]] ? KN[bind[a]] : "?"); if (cap && a == sel) cat(s, "  (appuie sur une touche)"); }
    row(s, 10, 24 + i * 21, 300, 19, a == sel); }
  txt(cap ? "Retour: annuler" : "OK: changer  Retour: quitter", 10, 220, 0, C(150, 160, 190), BGC);
}
static int turboT, gi, ri; static float tscale = 1.f;
#define QUICK_COUNT 31
// identifiants (anciens 0-21 conserves, nouveaux 22-25)
static const char *QL[QUICK_COUNT] = {"Reparer tout", "Turbo", "Boost !", "Voiture", "Moteur", "Remorque", "Route", "Gravite", "Temps", "Retour au depart", "Trafic IA", "Nombre IA", "Comportement IA", "Vitesse IA", "Plan de ville", "Meteo", "Puissance", "Adherence", "Suspension", "Freinage", "Poursuite police", "Test: percer>feu>boom", "Saut !", "Stop (frein d'urgence)", "Roues", "Carte", "Boite de vitesses", "Crevaison", "Embrayage", "Ceinture", "Airbags"};
// Page 1 : les actions les plus utiles. Page 2 ("Plus d'options") : reglages fins, IA, tests.
// 98 = retour a la page 1, 99 = ouvrir la page 2.
static const uint8_t QMAIN[] = {0, 22, 2, 23, 1, 26, 28, 27, 9, 3, 24, 15, 8, 7, 10, 20, 25, 99};
static const uint8_t QMORE[] = {4, 5, 6, 16, 17, 18, 19, 11, 12, 13, 14, 21, 29, 30, 98};
static int quickMore;                                     // 0 = page 1, 1 = page 2
static int qcount(void) { return quickMore ? (int)sizeof(QMORE) : (int)sizeof(QMAIN); }
static int qid(int i) { return quickMore ? QMORE[i] : QMAIN[i]; }
static int lastQid = 0, lastQd = 1;                      // derniere option utilisee (touche "Refaire option rapide")
static void draw_quick(int sel) {
  static const char *RN[3] = {"Seche", "Mouillee", "Glace"}, *GN[3] = {"Normale", "Lune", "Forte"};
  static const char *AI_MODE[3] = {"Circuit", "Suit joueur", "Patrouille"}, *AI_SPEED[5] = {"Tres lent", "Lent", "Normal", "Rapide", "Tres rapide"};
  static const char *CITY_PLAN[3] = {"Regulier", "Decale", "Canalise"}, *WEATHER[4] = {"Clair", "Pluie", "Brouillard", "Glace"};
  eadk_display_push_rect_uniform((eadk_rect_t){40, 0, 240, 240}, BGC); txt(quickMore ? "PLUS D'OPTIONS" : "OPTIONS RAPIDES", 108, 3, 0, C(255, 210, 80), BGC);
  int cnt = qcount(), top = sel - 5; if (top > cnt - 12) top = cnt - 12; if (top < 0) top = 0;
  for (int line = 0; line < 12 && top + line < cnt; line++) { int i = top + line, id = qid(i); char s[40], *p = s;
    if (id == 99) { cat(s, "Plus d'options  >"); row(s, 50, 18 + line * 18, 220, 17, i == sel); continue; }
    if (id == 98) { cat(s, "<  Retour"); row(s, 50, 18 + line * 18, 220, 17, i == sel); continue; }
    p = cat(p, QL[id]);
    if (id == 1) cat(p, turboT ? ": oui" : ": non"); else if (id == 10 && mapId != 3) cat(p, trafficEnabled ? ": oui (Ville)" : ": non (Ville)"); else if (id == 3) { p = cat(p, ": "); cat(p, VEH[cv].nm); } else if (id == 4) { p = cat(p, ": "); cat(p, ENG[ce].nm); }
    else if (id == 5) cat(p, trl ? ": oui" : ": non"); else if (id == 6) { p = cat(p, ": "); cat(p, RN[ri]); } else if (id == 7) { p = cat(p, ": "); cat(p, GN[gi]); } else if (id == 8) cat(p, tscale < .5f ? ": ralenti" : ": normal");
    else if (id == 10) cat(p, trafficEnabled ? ": oui" : ": non"); else if (id == 11) { p = cat(p, ": "); p = num(p, trafficCount); cat(p, "/64"); }
    else if (id == 12) { p = cat(p, ": "); cat(p, AI_MODE[trafficBehavior]); } else if (id == 13) { p = cat(p, ": "); cat(p, AI_SPEED[trafficSpeed]); }
    else if (id == 14) { p = cat(p, ": "); cat(p, CITY_PLAN[cityPlan]); } else if (id == 15) { p = cat(p, ": "); cat(p, WEATHER[weatherMode]); }
    else if (id >= 16 && id <= 19) { float v = id == 16 ? tunePower : (id == 17 ? tuneGrip : (id == 18 ? tuneSusp : tuneBrake)); p = cat(p, ": "); p = num(p, (int)(v * 100.f)); cat(p, "%"); }
    else if (id == 20) cat(p, pursuitEnabled ? ": active" : ": inactive");
    else if (id == 24) { p = cat(p, ": "); cat(p, WHL[cw].nm); } else if (id == 25) { p = cat(p, ": "); cat(p, MAPN[mapId]); } else if (id == 26) cat(p, manualGear ? ": Manuelle" : ": Auto");
    else if (id == 28) cat(p, clutchAssist ? ": Assistee" : ": Manuelle"); else if (id == 29) cat(p, beltOn ? ": bouclee" : ": detachee"); else if (id == 30) cat(p, airbagsOn ? ": actifs" : ": desactives");
    else if (id == 27) { int c = 0; for (int w = 0; w < 4; w++) c += tpLeak[w]; p = cat(p, ": "); p = num(p, c); cat(p, "/4 (OK)"); }
    row(s, 50, 18 + line * 18, 220, 17, i == sel); }
}
// applique une option rapide. ok = OK appuye (requis pour les actions "ponctuelles"). Retour : bit0 = fermer le menu, bit1 = remettre les tours a zero
static int quick_apply(int id, int d, int ok) {
  int close = 0, lapReset = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); fxSkip = 3;
  static const float RF[3] = {1.f, .6f, .25f}, GV[3] = {14.f, 5.f, 25.f};
  int cnt = ntr >= 0 ? ntr + 11 : NC;
  if (id == 0 && ok) { repair(cx, cz, hx, hz); if (tutOn) evRepair = 1; close = 1; }
  else if (id == 1) turboT ^= 1;
  else if (id == 2 && ok) { for (int i = 0; i < cnt; i++) { n[i].vx += hx * 10.f; n[i].vz += hz * 10.f; } close = 1; }
  else if (id == 22 && ok) { for (int i = 0; i < cnt; i++) n[i].vy += 7.f; close = 1; }
  else if (id == 23 && ok) { for (int i = 0; i < cnt; i++) n[i].vx = n[i].vy = n[i].vz = 0.f; close = 1; }
  else if (id == 3) { cv = (cv + NV + d) % NV; if (cv == 4) gen_car(genSeed); if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
  else if (id == 24) { cw = (cw + 4 + d) % 4; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
  else if (id == 4) { ce = (ce + 4 + d) % 4; engA = ENG[ce].a * VEH[cv].pw * tunePower; engV = ENG[ce].v; }
  else if (id == 5) { trl ^= 1; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
  else if (id == 6) { ri = (ri + 3 + d) % 3; gripF = RF[ri]; } else if (id == 7) { gi = (gi + 3 + d) % 3; G = GV[gi]; }
  else if (id == 8) tscale = tscale > .5f ? .4f : 1.f;
  else if (id == 9 && ok) { startpos(); lapReset = 1; close = 1; }
  else if (id == 25 && !tutOn && !delOn) { mapId = (mapId + 5 + d) % 5; startpos(); lapReset = 1; close = 1; }
  else if (id == 26) { manualGear ^= 1; }
  else if (id == 28) clutchAssist ^= 1; else if (id == 29) beltOn ^= 1; else if (id == 30) airbagsOn ^= 1;
  else if (id == 27 && ok && optPunct) { int w = 0; while (w < 4 && tpLeak[w]) w++; if (w < 4) tpLeak[w] = 1; else for (int q = 0; q < 4; q++) { tpLeak[q] = 0; tirePres[q] = 1.f; } }   // burst the next tyre; all flat -> re-inflate
  else if (id == 10) { trafficEnabled ^= 1; traffic_reset(); }
  else if (id == 11) { trafficCount += d; if (trafficCount < 0) trafficCount = 64; if (trafficCount > 64) trafficCount = 0; traffic_reset(); }
  else if (id == 12) { trafficBehavior = (trafficBehavior + 3 + d) % 3; traffic_reset(); }
  else if (id == 13) { trafficSpeed = (trafficSpeed + 5 + d) % 5; traffic_reset(); }
  else if (id == 14) { cityPlan = (cityPlan + 3 + d) % 3; }
  else if (id == 15) { weatherMode = (weatherMode + 4 + d) % 4; }
  else if (id >= 16 && id <= 19) {
    float *tune = id == 16 ? &tunePower : (id == 17 ? &tuneGrip : (id == 18 ? &tuneSusp : &tuneBrake));
    float old = *tune; *tune += d * .1f; if (*tune < .5f) *tune = .5f; if (*tune > 1.5f) *tune = 1.5f;
    if (id == 16) engA = ENG[ce].a * VEH[cv].pw * tunePower;
    if (id == 17) { latG = WHL[cw].gr * VEH[cv].gr * tuneGrip; if (latG > .45f) latG = .45f; }
    if (id == 18 && old > 0.f) for (int b = 0; b < nb; b++) if (bm[b].f == 1) { bm[b].k *= *tune / old; bm[b].c *= *tune / old; }
  } else if (id == 20) { pursuitEnabled ^= 1; traffic_reset(); }
  else if (id == 21 && ok) { static int demo; demo = (demo + 1) % 3; if (demo == 1) tankHp = .3f; else if (demo == 2) { burning = 1; fireT = 0.f; fireI = .1f; boomAt = 1e9f; } else explode(); close = 1; }
  return close | (lapReset << 1);
}
static void draw_tutorial(void) {
  char text[72]; tut_text(text); eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14));
  txt(text, 4, 225, 0, tutOk > 0 ? C(120, 255, 140) : C(255, 210, 80), C(10, 10, 14));
}
static void draw_dmg(void) {
  for (int i = 0; i < LW * LH; i++) fb[i] = BGC;
  curZ = -1; draw_top(lw * 45 / 160, lh / 2, 20.f * lw / 160, 1); present(0);
  int zp[5]; zones(zp); const char *zn[5] = {"Avant", "Arriere", "Gauche", "Droite", "Toit"};
  txt("DEGATS", 220, 6, 1, ACC, BGC);
  for (int i = 0; i < 5; i++) { char s[24], *p = s; p = cat(p, zn[i]); p = cat(p, ": "); p = num(p, zp[i]); cat(p, "%"); txt(s, 190, 36 + i * 17, 0, zp[i] > 60 ? C(255, 90, 80) : (zp[i] > 25 ? C(255, 210, 80) : C(120, 220, 120)), BGC); }
  { char ln[28]; ln[0] = 0; char *p = ln; int y = 124, any = 0;
    for (int g = 0; g < NG; g++) if (pAtt[g] <= 0) { any = 1; if ((p - ln) + (int)sizeof(PNAME[0]) > 0 && (p - ln) > 11) { txt(ln, 190, y, 0, C(255, 120, 100), BGC); y += 14; p = ln; *p = 0; } p = cat(p, PNAME[g]); p = cat(p, " "); }
    if (any) txt(ln, 190, y, 0, C(255, 120, 100), BGC); else txt("Aucune piece perdue", 190, y, 0, C(120, 220, 120), BGC); }
  { const char *nm[4] = {"Rad ", "Res ", "Arb ", "Ess "}; float v[4] = {radHp, tankHp, shaftHp, fuel};
    for (int i = 0; i < 4; i++) { char t[12], *q = t; q = cat(q, nm[i]); cat(q, ""); q = num(q, (int)(v[i] * 100.f + .5f)); *q = 0; txt(t, 2, 40 + i * 14, 0, comp_col(v[i]), BGC); }
    if (exploded) txt("BOOM!", 2, 100, 0, C(255, 120, 40), BGC); else if (burning) txt("FEU!", 2, 100, 0, C(255, 120, 40), BGC); }
  { int bs = beltOn ? ((abFront || abSideL || abSideR) ? 2 : 1) : 0, oc = (int)(occRisk * 100.f + .5f);                // belt, airbags, occupant injury risk
    txt(bs == 0 ? "Cein NON" : (bs == 2 ? "Cein TEN" : "Cein OK"), 2, 114, 0, bs == 0 ? C(255, 90, 80) : (bs == 2 ? C(255, 210, 80) : C(120, 220, 120)), BGC);
    txt(abFront && (abSideL || abSideR) ? "Airb AV+C" : (abFront ? "Airb AV" : (abSideL || abSideR ? "Airb LAT" : "Airb --")), 2, 128, 0, abFront || abSideL || abSideR ? C(255, 210, 80) : C(150, 160, 190), BGC);
    char t[12], *q = t; q = cat(q, "Occ "); q = num(q, oc); cat(q, "%"); txt(t, 2, 142, 0, oc > 50 ? C(255, 90, 80) : (oc > 15 ? C(255, 210, 80) : C(120, 220, 120)), BGC); }
  char s[24], *p = s;
  int wl = 0; for (int w = 0; w < 4; w++) wl += wAtt[w] > 0; p = s; p = cat(p, "Roues: "); p = num(p, wl); cat(p, "/4"); txt(s, 190, 192, 0, 0xFFFF, BGC);
  p = s; p = cat(p, "Total: "); p = num(p, (int)dmg); cat(p, "%"); txt(s, 190, 208, 0, C(255, 210, 80), BGC);
  if (mapId == 2 && ct == 2) { p = s; p = cat(p, "SCORE "); num(p, score); txt(s, 6, 226, 1, C(255, 210, 80), BGC); txt("OK : rejouer", 190, 226, 0, 0xFFFF, BGC); }
  else txt("Une touche pour revenir", 6, 226, 0, C(150, 160, 190), BGC);
}
static void hud(int kmh, int gear, int lap, int ms, int best) {
  char s[96], *p = s; p = num(p, kmh); p = cat(p, "km/h "); if (gear < 0) p = cat(p, "R"); else if (gear == 0) p = cat(p, "N"); else { p = cat(p, "G"); p = num(p, gear); if (manualGear) p = cat(p, "M"); }
  p = cat(p, " Deg "); p = num(p, (int)dmg); p = cat(p, "%"); if (turbo) p = cat(p, VEH[cv].th > 0.f ? " PROPULSEURS" : " TURBO");
  if (exploded) p = cat(p, " BOOM!"); else if (burning) p = cat(p, " FEU!"); else if (tankHp < .6f && fuel > .01f) p = cat(p, " FUITE");
  { int c = 0; for (int w = 0; w < 4; w++) c += tpLeak[w]; if (c) p = cat(p, " CREV"); }
  if (stallT > 0.f) p = cat(p, " CALE"); if (grindT > 0.f) p = cat(p, " CRAC"); if (hbrake > .3f) p = cat(p, " FREIN");
  if (engTemp > 1.f && !burning) p = cat(p, " TEMP!"); if (fuel < .12f && !burning) p = cat(p, " ESS");
  if (mapId == 0) { p = cat(p, " T"); p = num(p, lap); p = cat(p, " "); p = num(p, ms / 60000); p = cat(p, ":"); int sc = ms / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); p = cat(p, "."); p = num(p, ms / 100 % 10);
    if (best) { p = cat(p, " B"); p = num(p, best / 60000); p = cat(p, ":"); sc = best / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); } }
  else if (mapId == 2) { p = cat(p, " Test "); p = num(p, CRASHV[crashI]); if (ct == 2) { p = cat(p, " SCORE "); p = num(p, score); } else if (ct == 1) p = cat(p, " IMPACT!"); }
  eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14)); txt(s, 4, 225, 0, 0xFFFF, C(10, 10, 14));
}
int main(void) {
  int st = S_MENU, sel = 0, redraw = 1, gsel = 0, gview = 0, retS = S_MENU, cap = 0, fr = 0, lap = 1, best = 0, cp = 0;
  float acc = 0, stw = 0, pz = 0, gorb = .6f, gelev = 3.6f, gzoom = 7.5f, shx = 0, shz = 1, yawO = 0, hO = 0, dO = 0; uint64_t last = eadk_timing_millis(), lapT = last;
  for (int i = 0; i < NA; i++) bind[i] = BDEF[i];
  gen_car(genSeed); rec_load();
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
  static int lastSt = -1;
  for (;;) {
    if (st != lastSt) { if (st == S_MENU) { menu_init(); redraw = 1; } lastSt = st; }
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    int U = pressed(k, eadk_key_up), D = pressed(k, eadk_key_down), L = pressed(k, eadk_key_left), R = pressed(k, eadk_key_right), O = pressed(k, eadk_key_ok), B = pressed(k, eadk_key_back);
    if (st == S_MENU) {
      if (redraw) { draw_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 9) % 10; redraw = 1; } if (D) { sel = (sel + 1) % 10; redraw = 1; }
      if ((L || R || O) && sel >= 2 && sel <= 4) { int d = L ? -1 : 1; redraw = 1;
        if (sel == 2) mapId = (mapId + 5 + d) % 5; else if (sel == 3) solid = (solid + 3 + d) % 3; else crashI = (crashI + 5 + d) % 5; menu_init(); }
      else if (O) { if (sel == 0) { tutOn = 0; delOn = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 1) { garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 5) { retS = S_MENU; sel = 0; st = S_SET; redraw = 1; }
        else if (sel == 6) { retS = S_MENU; sel = 0; cap = 0; st = S_CTRL; redraw = 1; }
        else if (sel == 7) { mapBak = mapId; mapId = 3; tutOn = 1; tutStep = tutFlag = tutOk = evDmg = evRepair = 0; yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 8) { mapBak = mapId; mapId = 3; sel = 0; delOn = 0; st = S_DELIV; menu_init(); redraw = 1; }
        else if (sel == 9) return 0; }
      if (st == S_MENU) menu_bg();
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
        if (O && gsel == 11) { tutOn = 0; delOn = 0; startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
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
      if (bpress(k, A_RESET)) { fxSkip = 3; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); }
      if (bpress(k, A_CINT)) { camInterior ^= 1; if (camInterior) { yawO = 0; hO = 0; dO = 0; } }
      if (bpress(k, A_BEAMS)) structure ^= 1;
      if (bpress(k, A_TRANS)) { manualGear ^= 1; if (!manualGear && gearNow < 1) gearNow = 1; stallT = shiftT = 0.f; }
      if (manualGear) {                                      // 0 = neutral, 1..5; without the clutch (manual clutch mode) a shift grinds
        int ng = gearNow + (bpress(k, A_GUP) ? 1 : 0) - (bpress(k, A_GDN) ? 1 : 0); if (ng < 0) ng = 0; if (ng > 5) ng = 5;
        if (ng != gearNow) {
          if (clutchAssist) shiftT = .25f;
          else if (clutchKey < .5f && gearNow > 0 && ng > 0) { shiftT = .7f; grindT = 1.2f; engTemp += .03f; }
          gearNow = ng;
        }
      }
      if (bpress(k, A_TOP)) showTop ^= 1;
      if (bpress(k, A_DMG)) { if (tutOn) evDmg = 1; st = S_DMG; redraw = 1; pk = k; continue; }
      if (tutOn && bpress(k, A_SKIP)) tut_adv(hx, hz);
      if (bpress(k, A_QUICK) || bpress(k, A_QUICK2)) { st = S_QUICK; sel = 0; quickMore = 0; redraw = 1; pk = k; continue; }
      if (bpress(k, A_QLAST)) { int r = quick_apply(lastQid, lastQd, 1); if (r & 2) { lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); } }
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
      clutchKey = bdown(k, A_CLUTCH) ? 1.f : 0.f; hbrake += ((bdown(k, A_HBRAKE) ? 1.f : 0.f) - hbrake) * .6f; turbo = turboT || bdown(k, A_TURBO); thr = bdown(k, A_ACC) ? 1.f : 0; brk = 0;
      if (bdown(k, A_BRK)) { if (vf > 1.f) brk = 1; else thr = -.6f; }
      float tgt = (bdown(k, A_RIGHT) ? 1.f : 0) - (bdown(k, A_LEFT) ? 1.f : 0);
      stw += (tgt * (.30f + .05f * steerL) / (1.f + sp * .06f) - stw) * (.1f + .05f * steerL); steer = stw;
      uint64_t now = eadk_timing_millis(); float dtf = (float)(now - last) * tscale; wspin += vf * dtf * .001f / .35f; acc += dtf; last = now; if (acc > 60) acc = 60;
      while (acc >= 3.5f) { world_step(.0035f); acc -= 3.5f; }
      traffic_update(dtf * .001f); fx_update(dtf * .001f); bots_frame(dtf * .001f); skid_update(); tires_update(dtf * .001f); impact_fx(dtf * .001f); blast_fx(); airbag_fx(); part_update(dtf * .001f); if (grindT > 0.f) grindT -= dtf * .001f;
      if (delOn) { deliv_update(dtf * .001f); if (delEnd) { st = S_DRES; redraw = 1; pk = k; continue; } }
      if (mapId == 0) {
        if (cx < -TA * .8f && cz > -20 && cz < 20) cp = 1;
        if (cp && pz < 0 && cz >= 0 && cx > 0) { int t = (int)(now - lapT); if (!best || t < best) best = t; if (!recLap || t < recLap) { recLap = t; rec_save(); } lap++; lapT = now; cp = 0; }
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
      scene(0); draw_overlay(sp * 3.6f, vf < -.5f ? -1 : gearNow); if (delOn) draw_deliv_hud(); present(0);
      if (++fr % 6 == 0) { if (delOn) deliv_bar((int)(sp * 3.6f)); else hud((int)(sp * 3.6f), vf < -.5f ? -1 : gearNow, lap, (int)(now - lapT), recLap); }
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
        else if (sel == 1) { if (tutOn) { tutStep = tutFlag = tutOk = 0; evDmg = evRepair = 0; } if (delOn) deliv_begin(); else startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; }
        else if (sel == 2) { st = S_QUICK; sel = 0; quickMore = 0; redraw = 1; } else if (sel == 3) { st = S_DMG; redraw = 1; }
        else if (sel == 4) { if (tutOn || delOn) { tutOn = 0; delOn = 0; mapId = mapBak; } garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 5) { retS = S_PAUSE; sel = 0; st = S_SET; redraw = 1; }
        else if (sel == 6) { retS = S_PAUSE; sel = 0; cap = 0; st = S_CTRL; redraw = 1; } else { if (tutOn || delOn) { tutOn = 0; delOn = 0; mapId = mapBak; } st = S_MENU; sel = 0; redraw = 1; } }
      eadk_timing_msleep(30);
    } else if (st == S_SET) {
      if (redraw) { draw_set(sel); redraw = 0; }
      if (U) { sel = (sel + 11) % 12; redraw = 1; } if (D) { sel = (sel + 1) % 12; redraw = 1; }
      if (L || R) { int d = R ? 1 : -1; redraw = 1;
        if (sel == 0) { qual = (qual + 3 + d) % 3; setq(); } else if (sel == 1) { steerL += d; if (steerL < 1) steerL = 1; if (steerL > 5) steerL = 5; }
        else if (sel == 2) { camL += d; if (camL < 1) camL = 1; if (camL > 5) camL = 5; } else if (sel == 3) { fovI = (fovI + 3 + d) % 3; setq(); } else if (sel == 4) { vdI = (vdI + 3 + d) % 3; setq(); } else if (sel >= 5 && sel <= 9) opt_toggle(sel); }
      if (O && sel >= 5 && sel <= 9) { opt_toggle(sel); redraw = 1; }
      if (O && sel == 10) { st = S_KEYS; redraw = 1; }
      if ((O && sel == 11) || B) { if (retS == S_PAUSE) { st = S_PAUSE; sel = 4; } else { st = S_MENU; sel = 5; } redraw = 1; }
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
      int qn = qcount(), closeQ = 0;
      if (U) { sel = (sel + qn - 1) % qn; redraw = 1; } if (D) { sel = (sel + 1) % qn; redraw = 1; }
      if (L || R || O) { int d = L ? -1 : 1, id = qid(sel); redraw = 1;
        if (id == 99 || id == 98) { if (O || R || (id == 98 && L)) { quickMore = id == 99; sel = 0; } }   // changement de page
        else { lastQid = id; lastQd = d;
          int r = quick_apply(id, d, O);
          if (r & 2) { lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); }
          if (r & 1) closeQ = 1; }
      }
      if (B && quickMore) { quickMore = 0; sel = 0; redraw = 1; }                                       // Retour: page 2 -> page 1
      else if (B || bpress(k, A_QUICK) || bpress(k, A_QUICK2)) closeQ = 1;
      if (closeQ) { st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
      eadk_timing_msleep(30);
    } else if (st == S_DELIV) {
      if (redraw) { draw_deliv_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 2) % 3; redraw = 1; } if (D) { sel = (sel + 1) % 3; redraw = 1; }
      if ((L || R || O) && sel == 0) { delLvl = (delLvl + 3 + (L ? -1 : 1)) % 3; redraw = 1; }
      else if (O && sel == 1) { delOn = 1; tutOn = 0; deliv_begin(); yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
      else if (B || (O && sel == 2)) { mapId = mapBak; st = S_MENU; sel = 8; redraw = 1; }
      if (st == S_DELIV) menu_bg();
    } else if (st == S_DRES) {
      if (redraw) { draw_deliv_res(); redraw = 0; }
      if (O) { deliv_begin(); yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
      else if (B) { delOn = 0; mapId = mapBak; st = S_MENU; sel = 8; redraw = 1; }
      eadk_timing_msleep(30);
    } else {                                              // S_KEYS: key tester
      if (redraw) { clear(); txt("TEST DES TOUCHES", 90, 8, 1, ACC, BGC); txt("Appuie sur des touches. Retour: sortir", 30, 40, 0, C(150, 160, 190), BGC); redraw = 2; }
      if (B) { st = S_SET; sel = 10; redraw = 1; }
      else if (k != pk || redraw == 2) {
        eadk_display_push_rect_uniform((eadk_rect_t){0, 70, 320, 150}, BGC); int line = 0, col = 0;
        for (int key = 0; key < 53; key++) if (((k >> key) & 1) && KN[key]) { txt(KN[key], 20 + col * 100, 80 + line * 20, 0, C(255, 210, 80), BGC); if (++col == 3) { col = 0; line++; } }
        redraw = 3;
      }
      eadk_timing_msleep(30);
    }
    pk = k;
  }
}

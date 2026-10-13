#include <eadk.h>
#include "epsilon_app.h"     // np_app_begin/np_app_end: protege la calculatrice (Home) -> plus de reset
#include "epsilon_files.h"   // ef_read/ef_write: sauvegarde dans le systeme de fichiers d'Epsilon
#include "sim.h"
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumBeam3D";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#define MAXW 160   // RAM: the app must fit in ~220 KB (the 208x146 buffers needed 91 KB, 160x112 needs 54 KB)
#define MAXH 112
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
enum { S_MENU, S_GARAGE, S_GAME, S_PAUSE, S_DMG, S_PAGE, S_CTRL, S_KEYS, S_DELIV, S_DRES, S_STATS, S_ACH };
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
static int tsi = 4, tsPrev = 4; static float tscale = 1.f;   // temps: indice du niveau (0 = stop), niveau avant le stop, facteur
static const float TSV[6] = {0.f, .1f, .25f, .5f, 1.f, 2.f}; static const char *const TSN[6] = {"STOP", "x0.1", "x0.25", "x0.5", "x1", "x2"};
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
        else { int gx = (int)((X + 2000.f) * .5f), gz = (int)((Z + 2000.f) * .5f);
          if (cmCol) { int sh = ((gx + gz) & 1) ? 0 : 22; r = cmR > sh ? cmR - sh : 0; g = cmG > sh ? cmG - sh : 0; b = cmB > sh ? cmB - sh : 0; } else r = g = b = ((gx + gz) & 1) ? 255 : 228; } }
      else if (mapId == 3) {
        if (X < -CITY_X_LIMIT || X > CITY_X_LIMIT || Z < -CITY_Z_LIMIT || Z > CITY_Z_LIMIT) { r = 48 + ck * 5; g = 105 + ck * 7; b = 51; }
        else {
          int rx, rz0; city_nearest(X, Z, &rx, &rz0);
          float cxg, czg; city_center(rx, rz0, &cxg, &czg); float ax = X - cxg, az = Z - czg; if (ax < 0) ax = -ax; if (az < 0) az = -az;
          float roadX = 14.f - ax, roadZ = 18.f - az;
          int park = ax < 8.5f && az < 11.f && city_kind(rx, rz0) == 3, stn = ax < 8.5f && az < 11.f && city_kind(rx, rz0) == 4, building = ax < 8.5f && az < 11.f && !park && !stn;
          int onRoadX = !building && roadX < 3.8f, onRoadZ = !building && roadZ < 3.8f;
          int onSidewalk = !building && !onRoadX && !onRoadZ && (roadX < 5.5f || roadZ < 7.f);
          if (stn) { r = 104 + ck * 5; g = r; b = r + 4; if (ax > 7.7f || az > 10.2f) { r = 215; g = 195; b = 80; } else if (az < 4.4f && ax < 8.2f) { r = g = 74 + ck * 4; b = g + 3; } }   // station pad: yellow border, darker under the canopy
          else if (park) { r = 62 + ck * 6; g = 128 + ck * 6; b = 66; if (ax < 1.1f || az < 1.1f) { r = 160; g = 148; b = 118; } }
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
      else if (mapId == 5) {
        int vc = it_cz(Z), vm = ((vc % 12) + 12) % 12, vil = vm == 5 || vm == 6;
        if (ad < 5.4f) { r = g = (vil ? 106 : 82) + ck * 5; b = r + 3;                                      // asphalt (cobbles in the village)
          if (!vil && ad < .16f && (((int)(Z / 4.f + 1000.f)) & 1)) { r = 228; g = 214; b = 150; }          // dashed centre line
          else if (!vil && ad > 4.6f && ad < 5.1f) r = g = b = 214;                                          // edge line
          if (weatherMode == 1 || weatherMode == 3) { r = r * 3 / 4; g = g * 4 / 5; b = b * 5 / 6; } }
        else if (ad < 7.8f) { r = 168 + ck * 6; g = 152 + ck * 6; b = 118 + ck * 5; }                        // strada bianca shoulder
        else if (vil && ad < 36.f) { r = 150 + ck * 8; g = 138 + ck * 8; b = 120 + ck * 6; }                // village square
        else { int ia = (int)(X * .022f + 1000.f), ib = (int)(Z * .022f + 1000.f), fv = (ia * 7 + ib * 13 + ((ia * ib) >> 3)) & 7;
          if (h > 24.f) { r = 150 + ck * 6; g = 146 + ck * 6; b = 132; }                                     // rocky summits
          else if (fv < 2) { r = 208 - ck * 8; g = 176 - ck * 8; b = 82; }                                   // ripe wheat
          else if (fv == 2) { r = 82; g = 142 + ck * 8; b = 60; }                                            // meadow
          else if (fv < 5) { int row = ((int)(X * .8f + Z * .45f + 1000.f)) & 1; r = row ? 100 : 62; g = row ? 124 : 86; b = row ? 56 : 42; }   // vineyard rows
          else if (fv == 5) { r = 142 + ck * 6; g = 100 + ck * 4; b = 70; }                                  // ploughed field
          else if (fv == 6) { r = 118 + ck * 6; g = 132 + ck * 5; b = 82; }                                  // olive grove
          else { r = 48; g = 98 + ck * 6; b = 52; } }                                                        // woods
      }
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
// ---- breakable bodywork (player's car): windows crack then shatter, mirrors hang then tear off, bumpers hang by their last bolts (sim.h)
static uint8_t glsS[4], mirS[2] = {1, 1};                 // glsS: 0 windshield, 1 left window, 2 right window, 3 rear window (0 intact, 1 cracked, 2 shattered); mirS: left, right (0 torn off, 1 fitted, 2 hanging)
static float glsAcc[4], glsThr[4] = {1, 1, 1, 1}, mirAcc[2], mirThr[2] = {1, 1}, mirT[2];
static unsigned glsSeed = 1; static int bodyEp = -1, prevAtt[NG];
static float ibDvx, ibDvy, ibDvz;                          // speed lost by the car during the last frame (set by impact_fx)
static float bhx[10], bhz[10], bpx, bpz; static int bHn;    // body_fx: the car's velocity over the last ~0.3 s (a crash = a big loss of speed over that window)
typedef struct { float x, y, z, vx, vy, vz, life; uint8_t on, cr, cg, cb; } Loose;   // a torn-off part lying on the road
#define NLOOSE 8
static Loose lp[NLOOSE]; static int lpHead;
#define BODYP (optBody && n == nPl)
static int glass_of(const Pat *p) {
  if (p->kind == K_WIN) return p->grp == 6 ? 0 : 3;
  if (p->kind == K_DOOR) return p->grp == 4 ? 1 : (p->grp == 5 ? 2 : -1);
  return -1;
}
// draw one patch; pass 0 = glass of far-side patches, 2 = opaque cells, 3 = glass cells of near patches
#ifndef DBG_HIDE
#define DBG_HIDE 0
#endif
static void drawpat(int pi, const Pat *p, int pass, int facing, float cxm) {
  if (((DBG_HIDE & 1) && PAT[pi].kind == K_HOOD) || ((DBG_HIDE & 2) && PAT[pi].kind == K_TRUNK) || ((DBG_HIDE & 4) && PAT[pi].kind == K_DOOR) || ((DBG_HIDE & 8) && (PAT[pi].kind == K_ROOF || PAT[pi].kind == K_WIN))) return;
  if (camInterior && p->kind == K_WIN && p->grp == 6) return;   // first-person: windshield removed for a clear view
  int nu = p->nu, nv = p->nv; const Veh *V = &VEH[cv]; float dk = 1.f - dmg * .006f; int gw = BODYP ? glass_of(p) : -1;
  float c[4][3]; for (int k = 0; k < 4; k++) { c[k][0] = PX[p->n[k]]; c[k][1] = PY[p->n[k]]; c[k][2] = PZ[p->n[k]]; }
  float ax = c[1][0] - c[0][0], ay = c[1][1] - c[0][1], az = c[1][2] - c[0][2], bx = c[3][0] - c[0][0], by = c[3][1] - c[0][1], bz = c[3][2] - c[0][2]; uint8_t visible[64];
  float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx, nl = fsqrt(nx * nx + ny * ny + nz * nz) + 1e-4f, sg = psg[pi]; nx *= sg / nl; ny *= sg / nl; nz *= sg / nl;
  for (int j = 0; j <= nv; j++) for (int i = 0; i <= nu; i++) {
    float s = p->s0 + (p->s1 - p->s0) * i / nu, t = p->t0 + (p->t1 - p->t0) * j / nv;
    float lu = (float)i / nu, lv = (float)j / nv, w = p->bulge * 16.f * lu * (1 - lu) * lv * (1 - lv); if (w < 0) w = 0;   // bulge uses the patch-local u,v: it is 0 on every edge, so neighbouring patches (arches/rockers/walls...) meet with no gap
    float x = (1 - s) * (1 - t) * c[0][0] + s * (1 - t) * c[1][0] + s * t * c[2][0] + (1 - s) * t * c[3][0] + nx * w, y = (1 - s) * (1 - t) * c[0][1] + s * (1 - t) * c[1][1] + s * t * c[2][1] + (1 - s) * t * c[3][1] + ny * w,
          z = (1 - s) * (1 - t) * c[0][2] + s * (1 - t) * c[1][2] + s * t * c[2][2] + (1 - s) * t * c[3][2] + nz * w; int k = j * (nu + 1) + i;
    g3x[k] = x; g3y[k] = y; g3z[k] = z; gz3[k] = (x - camx) * camhx + (z - camz) * camhz;
    visible[k] = gz3[k] >= .401f; if (visible[k]) proj(x, y, z, &gx[k], &gy[k]);
  }
  int i0 = gz3[0] > gz3[nu] ? 0 : nu - 1, di = i0 == 0 ? 1 : -1, j0 = gz3[0] > gz3[nv * (nu + 1)] ? 0 : nv - 1, dj = j0 == 0 ? 1 : -1;
  for (int jj = 0, j = j0; jj < nv; jj++, j += dj) for (int ii = 0, i = i0; ii < nu; ii++, i += di) {
    int m = cellmat(p, i, j), glass = m == M_GLASS;
    if (pass == 2 && glass) continue; if (pass != 2 && !glass) continue;
    if (glass && gw >= 0 && glsS[gw] == 2 && ((((unsigned)(glsSeed + pi * 7 + i * 13 + j * 29)) * 2654435761u) >> 28) > 3u) continue;   // shattered: only a few ragged fragments stay in the frame
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
  { int crk = (gw >= 0 && glsS[gw] == 1) || (p->kind == K_WIN && p->grp == 6 && (pAtt[6] < pAtt0[6] || dmg > 40));   // cracked glass: a spider web
    if (gw >= 0 && glsS[gw] == 2) crk = 0;
    if (pass != 2 && crk && (p->kind == K_WIN || p->kind == K_DOOR)) {
      int vi0 = 1, vi1 = nu - 1, vj0 = p->kind == K_DOOR ? 3 : 1, vj1 = p->kind == K_DOOR ? 5 : nv - 1, mi = (vi0 + vi1) / 2, mj = (vj0 + vj1) / 2;
      int ei[8] = {vi0, mi, vi1, vi1, vi1, mi, vi0, vi0}, ej[8] = {vj0, vj0, vj0, mj, vj1, vj1, vj1, mj}, ck = mj * (nu + 1) + mi, hv[8]; float cxp[8], cyp[8];
      for (int q = 0; q < 8; q++) { int k = ej[q] * (nu + 1) + ei[q]; hv[q] = visible[ck] && visible[k];
        if (hv[q]) { cxp[q] = (gx[ck] + gx[k]) * .5f; cyp[q] = (gy[ck] + gy[k]) * .5f; line((int)gx[ck], (int)gy[ck], (int)gx[k], (int)gy[k], C(235, 240, 245)); } }
      for (int q = 0; q < 8; q++) { int q2 = (q + 1) & 7; if (hv[q] && hv[q2]) line((int)cxp[q], (int)cyp[q], (int)cxp[q2], (int)cyp[q2], C(215, 225, 235)); }
    } }
}
static void drawbx2(const Bx *B) {
  float x[8], y[8], z[8], zz[8]; int any = 0;
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
static void drawbx(int bi) { drawbx2(bx_at(bi)); }
static void body_reset(void) {                             // new game / repair: every window and mirror is back, loose parts are swept away
  for (int w = 0; w < 4; w++) { glsS[w] = 0; glsAcc[w] = 0.f; glsThr[w] = .8f + fx_rand() * .5f; }
  for (int q = 0; q < 2; q++) { mirS[q] = 1; mirAcc[q] = 0.f; mirThr[q] = .8f + fx_rand() * .5f; mirT[q] = 0.f; }
  glsSeed = (unsigned)(fx_rand() * 65535.f) + 1u; bHn = 0;
  for (int i = 0; i < NLOOSE; i++) lp[i].on = 0;
  for (int g = 0; g < NG; g++) prevAtt[g] = pAtt[g];
}
static void body_toggle(void) { optBody ^= 1; body_reset(); }
static void draw_mirror(int s) {                           // s: 0 left, 1 right. Fitted: housing + mirror face on a stalk. Hanging: swings down on its cable. Torn off: nothing.
  int st = BODYP ? mirS[s] : 1; if (!st || camInterior) return;
  const Veh *V = &VEH[cv]; float sg = s ? 1.f : -1.f, da = 0.f, db = 0.f, dc = 0.f;
  if (st == 2) { float sw = fsin(mirT[s]); da = sg * (.12f + .05f * sw); db = -.3f; dc = -.1f + .12f * fsin(mirT[s] * .7f + 1.f); }
  float ac = sg * 1.02f + da, bc = 1.03f + db, cc = .59f + dc;
  if (st == 2) { float x0, y0, z0, x1, y1, z1, sx0, sy0, sx1, sy1; fpt(0, sg * .93f, 1.f, .59f, &x0, &y0, &z0); fpt(0, ac, bc + .05f, cc, &x1, &y1, &z1);
    if (proj(x0, y0, z0, &sx0, &sy0) && proj(x1, y1, z1, &sx1, &sy1)) line((int)sx0, (int)sy0, (int)sx1, (int)sy1, C(20, 20, 24)); }
  else { Bx S = {0, 3, sg * .95f - .03f, sg * .95f + .03f, .98f, 1.f, .56f, .62f, 30, 30, 34}; drawbx2(&S); }
  Bx H = {0, 3, ac - .07f, ac + .07f, bc - .055f, bc + .055f, cc - .09f, cc + .09f, V->r, V->g, V->b};
  Bx F = {0, 3, ac - .06f, ac + .06f, bc - .045f, bc + .045f, cc - .098f, cc - .088f, 110, 150, 175};
  drawbx2(&H); drawbx2(&F);
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
  if (tsi == 0) { frect(lw - 14, 4, 4, 12, C(255, 255, 255)); frect(lw - 8, 4, 4, 12, C(255, 255, 255)); }   // icone pause: temps stoppe
  int sp = (int)(kmh + .5f), sc = lw >= 150 ? 2 : 1, g = gear < 0 ? 10 : (gear == 0 ? 13 : gear), blink = ((int)(fxClock * 6.f)) & 1;
  float rpm = engineRpm / 7000.f;
  if (stRefuel) { int bw = lw * 40 / 100, bx0 = (lw - bw) / 2, by0 = lh * 38 / 100;       // refuelling bar
    blendA = 200; frect(bx0 - 2, by0 - 2, bw + 4, 10, C(10, 12, 18)); blendA = 0; frect(bx0, by0, bw, 6, C(40, 44, 52)); frect(bx0, by0, (int)(bw * fuel), 6, C(240, 205, 85));
    if (blink) frect(bx0 - 2, by0 - 2, bw + 4, 1, ACC); }
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
  if (!camInterior) for (int q = 0; q < 2; q++) { if ((q ? 1.f : -1.f) * camSide > 0) continue; it[ni] = 300 + q; dp[ni++] = (n[q ? 44 : 40].x - camx) * camhx + (n[q ? 44 : 40].z - camz) * camhz; }   // far-side mirror: behind the body
  for (int i = 1; i < ni; i++) { int a = it[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { it[j + 1] = it[j]; dp[j + 1] = dp[j]; j--; } it[j + 1] = a; dp[j + 1] = v; }
  const Whl *W = &WHL[cw];
  for (int q = 0; q < ni; q++) { int a = it[q];
    if (a >= 300) draw_mirror(a - 300);
    else if (a >= 200) { int w = a - 200; float ax = hz_, az = -hx_; if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); } wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); }
    else if (a >= 100) drawbx(a - 100); else drawpat(a, &PL[a], 2, 0, cxm); }
  // opaque outer patches facing the camera (parts also show their inside when turned away)
  int ok[NPAT], no = 0; float od2[NPAT];
  for (int k = 0; k < NPAT; k++) if (PL[k].cls == 0 && (fc[k] || PL[k].two)) { ok[no] = k; od2[no++] = pd[k]; }
  for (int i = 1; i < no; i++) { int a = ok[i]; float v = od2[i]; int j = i - 1; while (j >= 0 && od2[j] < v) { ok[j + 1] = ok[j]; od2[j + 1] = od2[j]; j--; } ok[j + 1] = a; od2[j + 1] = v; }
  for (int q = 0; q < no; q++) drawpat(ok[q], &PL[ok[q]], 2, fc[ok[q]], cxm);
  for (int w = 14; w < 18; w++) { int sw = (w & 1) ? 1 : -1; if (camInterior || sw * camSide <= 0) continue; float ax = hz_, az = -hx_; if (w >= 16) { float c = fsin(steer + 1.5708f), sn = fsin(steer); ax = hz_ * c - hx_ * sn; az = -(hx_ * c + hz_ * sn); } wheel3d(n[w].x, n[w].y, n[w].z, n[w].r * 1.08f, ax, az, wspin, C(W->cr, W->cg, W->cb)); }
  for (int q = 0; q < no; q++) { int k = ok[q]; if (fc[k] && (PL[k].kind == K_WIN || PL[k].kind == K_DOOR)) drawpat(k, &PL[k], 3, 1, cxm); }
  if (!camInterior) for (int q = 0; q < 2; q++) if ((q ? 1.f : -1.f) * camSide > 0) draw_mirror(q);   // near-side mirror: in front of the body
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
static void draw_city_station(int index) {              // gas station: kiosk, three pumps, canopy on poles, tall sign (sorted far to near)
  CityBlock *B = &cityBlocks[index]; float cx = B->x, cz = B->z, dd = (cx - camx) * camhx + (cz - camz) * camhz;
  typedef struct { float x, z, hx, hz, y0, y1, d; uint8_t r, g, b; } SP; SP it[24]; int m = 0;
#define SADD(X, Z, HX, HZ, Y0, Y1, R, G, B2) do { it[m].x = (X); it[m].z = (Z); it[m].hx = (HX); it[m].hz = (HZ); it[m].y0 = (Y0); it[m].y1 = (Y1); it[m].r = (R); it[m].g = (G); it[m].b = (B2); it[m].d = ((X) - camx) * camhx + ((Z) - camz) * camhz; m++; } while (0)
  int far = dd > 50.f;
  if (!far) {
    SADD(cx, cz - 8.2f, 3.5f, 1.6f, 0.f, 3.f, 225, 225, 215); SADD(cx, cz - 8.2f, 3.8f, 1.9f, 3.f, 3.3f, 205, 55, 40);
    for (int k = 0; k < 3; k++) { float pxk = cx + (k - 1) * 4.f;
      SADD(pxk, cz, .45f, 1.1f, 0.f, .18f, 150, 150, 150); SADD(pxk, cz, .3f, .26f, .18f, 1.5f, k == 1 ? 40 : 205, k == 1 ? 130 : 60, k == 1 ? 70 : 45); }
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) SADD(cx + sx * 7.2f, cz + sz * 2.8f, .18f, .18f, 0.f, 4.3f, 190, 190, 195);
  }
  SADD(cx, cz, 8.f, 3.6f, 4.3f, 4.9f, 205, 55, 40);
  SADD(cx + 7.8f, cz + 9.5f, .12f, .12f, 0.f, 7.5f, 190, 190, 195); SADD(cx + 7.8f, cz + 9.5f, .95f, .16f, 6.5f, 8.1f, 240, 200, 40);
  for (int i = 1; i < m; i++) { SP v = it[i]; int j = i - 1; while (j >= 0 && it[j].d < v.d) { it[j + 1] = it[j]; j--; } it[j + 1] = v; }
  for (int i = 0; i < m; i++) city_box(it[i].x, it[i].z, it[i].hx, it[i].hz, it[i].y0, it[i].y1, it[i].r, it[i].g, it[i].b);
#undef SADD
  if (far) return;
  int here = stRefuel && n[4].x > cx - 9.f && n[4].x < cx + 9.f && n[4].z > cz - 5.f && n[4].z < cz + 5.f;
  for (int k = 0; k < 3; k++) { float pxk = cx + (k - 1) * 4.f;
    city_lamp(pxk, 1.62f, cz, .1f, here ? C(70, 240, 110) : C(240, 240, 235)); city_lamp(pxk, 4.22f, cz, .2f, C(255, 245, 200)); }
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
  int signal = tl_state(index);
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
// turn signals, brake lights, hazard flashing and horn waves of a traffic car. (x,z) = centre, (hx,hz) = heading.
// phys = 1: the car is a physical bot and is the active vehicle (n[] / VEH[cv] belong to it)
static void traffic_lights(const TrafficCar *t, float x, float z, float hx, float hz, int phys) {
  curZ = -1; blendA = 0;
  float rx = hz, rz = -hx, fv = (camx - x) * hx + (camz - z) * hz;
  float hl = phys ? 2.3f * VEH[cv].sz + .03f : 1.85f, hw = phys ? .82f * VEH[cv].sx : .74f, y = phys ? n[4].y + .2f * VEH[cv].sy : gh(x, z) + .58f;
  int on = ((int)(signalClock * 3.f)) & 1, haz = t->honkT > 0.f && (((int)(signalClock * 8.f)) & 1), rear = fv <= 0.f;
  float al = rear ? -hl : hl;                                       // only the end the camera can see
  for (int s = -1; s <= 1; s += 2) {
    float lx = x + rx * s * hw + hx * al, lz = z + rz * s * hw + hz * al;
    if ((t->blink == s && on) || haz) city_lamp(lx, y, lz, .14f, C(255, 170, 20));
    else if (rear && t->brake) city_lamp(lx, y, lz, .12f, C(255, 40, 35));
    else if (t->blink == s || t->honkT > 0.f) city_lamp(lx, y, lz, .09f, C(95, 60, 10));
  }
  if (t->honkT > 0.f) {                                             // horn: sound waves on both sides above the roof
    float sx, sy, dd = (x - camx) * camhx + (z - camz) * camhz;
    if (dd > 1.f && dd < 45.f && proj(x, y + 1.3f, z, &sx, &sy)) {
      int kmax = 1 + ((int)(signalClock * 7.f) % 3), ix = (int)sx, iy = (int)sy; uint16_t c = C(255, 232, 90);
      for (int s = -1; s <= 1; s += 2) for (int k = 0; k <= kmax; k++) { int ox = ix + s * (6 + k * 4), h = 2 + k; line(ox, iy - h, ox + s, iy, c); line(ox + s, iy, ox, iy + h, c); }
    }
  }
}
static void draw_traffic(int index) {
  const TrafficCar *car = &traffic[index];
  static const uint8_t CR[8] = {200, 40, 215, 185, 65, 230, 110, 175}, CG[8] = {48, 135, 170, 90, 165, 180, 90, 140}, CB[8] = {40, 55, 45, 40, 70, 65, 180, 60};
  int color = index & 7;
  traffic_prism(car, .82f, 1.8f, .18f, .82f, CR[color], CG[color], CB[color]);
  traffic_prism(car, .65f, .86f, .84f, 1.27f, 44, 82, 102);
  traffic_prism(car, .66f, .34f, .82f, .89f, 25, 28, 32);
  traffic_lights(car, car->x, car->z, car->hx, car->hz, 0);
}
static void draw_police(void) {
  traffic_prism(&policeCar, .86f, 1.9f, .18f, .84f, 235, 235, 228);
  traffic_prism(&policeCar, .64f, .9f, .86f, 1.28f, 42, 72, 92);
  TrafficCar light = policeCar; float sideX = policeCar.hz * .28f, sideZ = -policeCar.hx * .28f;
  light.x += sideX; light.z += sideZ; traffic_prism(&light, .22f, .18f, 1.28f, 1.43f, ((int)(signalClock * 3.f) & 1) ? 35 : 230, 50, ((int)(signalClock * 3.f) & 1) ? 230 : 35);
  light.x -= sideX * 2.f; light.z -= sideZ * 2.f; traffic_prism(&light, .22f, .18f, 1.28f, 1.43f, ((int)(signalClock * 3.f) & 1) ? 230 : 35, 50, ((int)(signalClock * 3.f) & 1) ? 35 : 230);
}
static void ev_lights(const TrafficCar *c, float y0, float y1, float off, float sz, int lt) {   // two flashing beacons, red / blue
  TrafficCar l = *c; float sx = c->hz * off, sz2 = -c->hx * off;
  l.x += sx; l.z += sz2; traffic_prism(&l, sz, sz * .8f, y0, y1, lt ? 35 : 230, 50, lt ? 230 : 35);
  l.x -= sx * 2.f; l.z -= sz2 * 2.f; traffic_prism(&l, sz, sz * .8f, y0, y1, lt ? 230 : 35, 50, lt ? 35 : 230);
}
static void draw_ebox(int idx) {
  const EBox *B = &ebx[idx]; TrafficCar c = {B->x, B->z, 0.f, 0.f, B->hx, B->hz, 0}, t;
  int lt = ((int)(signalClock * 3.f)) & 1, r = B->r, g = B->g, b = B->b;
  switch (B->kind) {
    case EK_WRECK: {                                                                  // crashed, scorched car
      traffic_prism(&c, B->hw, B->hl, .18f, .78f, r * 5 / 10 + 14, g * 5 / 10 + 12, b * 5 / 10 + 12);
      t = c; t.x -= B->hx * .25f; t.z -= B->hz * .25f; traffic_prism(&t, B->hw * .74f, B->hl * .4f, .78f, 1.18f, 38, 62, 78);
      t = c; t.x += B->hx * (B->hl - .45f); t.z += B->hz * (B->hl - .45f); traffic_prism(&t, B->hw * .92f, .45f, .78f, .95f, 30, 30, 32);   // crumpled bonnet
    } break;
    case EK_POLICE: case EK_CCAR:
      traffic_prism(&c, B->hw, B->hl * .95f, .18f, .84f, r, g, b);
      traffic_prism(&c, B->hw * .74f, B->hl * .46f, .86f, 1.28f, 42, 72, 92);
      ev_lights(&c, 1.28f, 1.43f, .28f, .22f, lt);
      break;
    case EK_AMBU:
      t = c; t.x -= B->hx * B->hl * .25f; t.z -= B->hz * B->hl * .25f;
      traffic_prism(&t, B->hw * .97f, B->hl * .72f, .35f, 2.1f, r, g, b);
      traffic_prism(&t, B->hw * 1.0f, B->hl * .72f, 1.0f, 1.3f, 215, 40, 40);
      t = c; t.x += B->hx * B->hl * .72f; t.z += B->hz * B->hl * .72f;
      traffic_prism(&t, B->hw * .92f, B->hl * .3f, .35f, 1.55f, r - 14, g - 14, b - 10);
      traffic_prism(&t, B->hw * .86f, B->hl * .12f, 1.15f, 1.5f, 42, 72, 92);
      ev_lights(&c, 2.1f, 2.25f, .4f, .2f, lt);
      break;
    case EK_BARRIER: {
      float rx = B->hz, rz = -B->hx; int blink = (((int)(fxClock * 3.f)) + idx) & 1;
      for (int k = -1; k <= 1; k++) { t = c; t.x += rx * k * B->hw * .667f; t.z += rz * k * B->hw * .667f;
        traffic_prism(&t, B->hw * .333f, B->hl, .22f, 1.0f, (k & 1) ? 238 : r, (k & 1) ? 238 : g, (k & 1) ? 232 : b); }
      city_lamp(B->x - rx * B->hw * .8f, 1.12f, B->z - rz * B->hw * .8f, .13f, blink ? C(255, 190, 40) : C(110, 75, 20));
      city_lamp(B->x + rx * B->hw * .8f, 1.12f, B->z + rz * B->hw * .8f, .13f, blink ? C(110, 75, 20) : C(255, 190, 40));
    } break;
    case EK_PILE:
      traffic_prism(&c, B->hw, B->hl, 0.f, B->h * .55f, r, g, b);
      traffic_prism(&c, B->hw * .6f, B->hl * .6f, B->h * .5f, B->h, r + 20, g + 16, b + 10);
      break;
    case EK_TRUCKW:
      t = c; t.x -= B->hx * B->hl * .22f; t.z -= B->hz * B->hl * .22f;
      traffic_prism(&t, B->hw, B->hl * .66f, .45f, 1.7f, 70, 74, 80);
      t = c; t.x += B->hx * B->hl * .7f; t.z += B->hz * B->hl * .7f;
      traffic_prism(&t, B->hw * .95f, B->hl * .3f, .35f, 1.65f, r, g, b);
      traffic_prism(&t, B->hw * .88f, B->hl * .12f, 1.1f, 1.5f, 42, 72, 92);
      city_lamp(B->x - B->hx * B->hl * .1f, 2.15f, B->z - B->hz * B->hl * .1f, .16f, lt ? C(255, 190, 40) : C(110, 75, 20));
      break;
    case EK_CTRUCK:
      t = c; t.x -= B->hx * B->hl * .22f; t.z -= B->hz * B->hl * .22f;
      traffic_prism(&t, B->hw, B->hl * .68f, .45f, B->h, r, g, b);
      t = c; t.x += B->hx * B->hl * .72f; t.z += B->hz * B->hl * .72f;
      traffic_prism(&t, B->hw * .95f, B->hl * .28f, .35f, 1.7f, r * 8 / 10, g * 8 / 10, b * 8 / 10);
      traffic_prism(&t, B->hw * .88f, B->hl * .1f, 1.15f, 1.55f, 42, 72, 92);
      break;
    default:                                                                          // traffic cone (decor, never collides)
      city_box(B->x, B->z, .28f, .28f, .03f, .7f, 235, 105, 35); city_box(B->x, B->z, .2f, .2f, .4f, .5f, 240, 240, 235);
  }
}
static void draw_loose(int idx) {                           // a torn-off mirror lying on the road
  const Loose *q = &lp[idx];
  city_box(q->x, q->z, .08f, .08f, q->y - .05f, q->y + .07f, q->cr, q->cg, q->cb); city_box(q->x, q->z, .055f, .055f, q->y + .07f, q->y + .08f, 90, 130, 150);
}
static void trailer_wheels(int nearPass) {
  float tx = n[ntr+4].x + n[ntr+5].x + n[ntr+6].x + n[ntr+7].x - n[ntr].x - n[ntr+1].x - n[ntr+2].x - n[ntr+3].x, tz = n[ntr+4].z + n[ntr+5].z + n[ntr+6].z + n[ntr+7].z - n[ntr].z - n[ntr+1].z - n[ntr+2].z - n[ntr+3].z;
  float l = fsqrt(tx * tx + tz * tz) + 1e-4f, ax = tz / l, az = -tx / l, cx = (n[ntr+8].x + n[ntr+9].x) * .5f, cz = (n[ntr+8].z + n[ntr+9].z) * .5f, camSide = (camx - cx) * ax + (camz - cz) * az;
  for (int w = 8; w < 10; w++) { float side = (n[ntr + w].x - cx) * ax + (n[ntr + w].z - cz) * az; int near = side * camSide > 0;
    if (near == nearPass) wheel3d(n[ntr + w].x, n[ntr + w].y, n[ntr + w].z, n[ntr + w].r + .03f, ax, az, wspin, C(150, 150, 155)); }
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
  { const TrafficCar *tc = b->src == PSRC ? &policeCar : &traffic[b->src]; traffic_lights(tc, tc->x, tc->z, tc->hx, tc->hz, 1); }
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
#define PART_MAX 120
static Part parts[PART_MAX]; static int partHead, fxSkip;
static void part_clear(void) { for (int i = 0; i < PART_MAX; i++) parts[i].life = 0.f; partHead = 0; }
static void part_add(float x, float y, float z, float vx, float vy, float vz, float life, float size, int type, int cr, int cg, int cb) {
  if (!optPart) return;
  Part *p = &parts[partHead]; partHead = (partHead + 1) % PART_MAX;
  p->x = x; p->y = y; p->z = z; p->vx = vx; p->vy = vy; p->vz = vz; p->life = p->max = life; p->size = size; p->type = (uint8_t)type; p->cr = (uint8_t)cr; p->cg = (uint8_t)cg; p->cb = (uint8_t)cb;
}
static void dust_col(int *r, int *g, int *b) {
  if (mapId == 3) { *r = 128; *g = 126; *b = 120; } else if (mapId == 5) { *r = 170; *g = 150; *b = 112; } else if (mapId == 4) { *r = 210; *g = 210; *b = 212; } else if (mapId == 2) { *r = 125; *g = 125; *b = 130; } else { *r = 130; *g = 108; *b = 76; }
}
static void part_update(float dt) {
  if (dt <= 0.f) return; if (dt > .1f) dt = .1f;
  for (int i = 0; i < PART_MAX; i++) {
    Part *p = &parts[i]; if (p->life <= 0.f) continue;
    p->life -= dt; if (p->life <= 0.f) { p->life = 0.f; continue; }
    p->x += p->vx * dt; p->y += p->vy * dt; p->z += p->vz * dt;
    if (p->type == 1 || p->type == 3 || p->type == 5) {
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
    } else if (p->type == 5) {                                                  // glass shard: a tiny glint that sparkles now and then
      int spk = ((i * 5 + (int)(fxClock * 12.f)) & 3) == 0; uint16_t c = spk ? 0xFFFF : C(p->cr, p->cg, p->cb);
      if (d < 7.f) disc((int)sx, (int)sy, 1, c); else px((int)sx, (int)sy, c);
      if (spk && d < 14.f) { px((int)sx + 1, (int)sy, c); px((int)sx - 1, (int)sy, c); }
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
  ibDvx = ibDvy = ibDvz = 0.f;
  float vx = 0, vy = 0, vz = 0, cx = 0, cy = 0, cz = 0;
  for (int i = 0; i < 20; i++) { vx += n[i].vx * .05f; vy += n[i].vy * .05f; vz += n[i].vz * .05f; cx += n[i].x * .05f; cy += n[i].y * .05f; cz += n[i].z * .05f; }
  float dvx = ipvx - vx, dvy = ipvy - vy, dvz = ipvz - vz, mx = cx - ipcx, mz = cz - ipcz; int skip = fxSkip > 0; if (fxSkip > 0) fxSkip--;
  ipvx = vx; ipvy = vy; ipvz = vz; ipcx = cx; ipcz = cz;
  if (skip || dt <= 0.f || mx * mx + mz * mz > 100.f) return;
  ibDvx = dvx; ibDvy = dvy; ibDvz = dvz;
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
static void lp_add(float x, float y, float z, float vx, float vy, float vz, int r, int g, int b) {
  Loose *q = &lp[lpHead]; lpHead = (lpHead + 1) % NLOOSE;
  q->x = x; q->y = y; q->z = z; q->vx = vx; q->vy = vy; q->vz = vz; q->life = 45.f; q->on = 1; q->cr = (uint8_t)r; q->cg = (uint8_t)g; q->cb = (uint8_t)b;
}
static void glass_burst(int w, int cnt, float k) {         // shards fly out of window w (0 windshield, 1 left, 2 right, 3 rear)
  float hx, hz; heading(&hx, &hz); float rx = hz, rz = -hx, cx = 0, cy = 0, cz = 0, ox, oz, ax, az, span = 1.4f; int ids[4];
  if (w == 0) { ids[0] = 46; ids[1] = 47; ids[2] = 48; ids[3] = 49; ox = hx; oz = hz; ax = rx; az = rz; }
  else if (w == 3) { ids[0] = 10; ids[1] = 11; ids[2] = 20; ids[3] = 21; ox = -hx; oz = -hz; ax = rx; az = rz; }
  else { int b0 = w == 1 ? 38 : 42; for (int q = 0; q < 4; q++) ids[q] = b0 + q; ox = w == 1 ? -rx : rx; oz = w == 1 ? -rz : rz; ax = hx; az = hz; span = .9f; }
  for (int q = 0; q < 4; q++) { cx += n[ids[q]].x * .25f; cy += n[ids[q]].y * .25f; cz += n[ids[q]].z * .25f; }
  if (w == 1 || w == 2) cy += .25f;
  for (int i = 0; i < cnt; i++) { float u = fx_rand() - .5f, v = fx_rand() - .5f, kk = k * (.4f + fx_rand());
    part_add(cx + ax * u * span, cy + v * .35f, cz + az * u * span, ox * kk + ax * u * 2.f + n[4].vx * .5f + (fx_rand() - .5f) * 1.5f, 1.f + fx_rand() * 2.5f,
      oz * kk + az * u * 2.f + n[4].vz * .5f + (fx_rand() - .5f) * 1.5f, 2.2f + fx_rand() * 2.6f, .03f, 5, 185 + (int)(fx_rand() * 70.f), 215 + (int)(fx_rand() * 40.f), 235 + (int)(fx_rand() * 20.f)); }
}
static void mirror_tear(int s, float power) {              // the mirror leaves the car: it flies off, bounces and stays on the road
  float hx, hz; heading(&hx, &hz); float sg = s ? 1.f : -1.f, rx = hz * sg, rz = -hx * sg; const Node *a = &n[s ? 44 : 40], *b = &n[s ? 43 : 39]; const Veh *V = &VEH[cv];
  float x = a->x + (b->x - a->x) * .3f + rx * .12f, y = a->y + (b->y - a->y) * .3f, z = a->z + (b->z - a->z) * .3f + rz * .12f, k = 1.5f + power * .25f;
  lp_add(x, y, z, n[4].vx * .8f + rx * k + (fx_rand() - .5f) * 1.5f, 2.f + fx_rand() * 2.5f, n[4].vz * .8f + rz * k + (fx_rand() - .5f) * 1.5f, V->r, V->g, V->b);
  for (int i = 0; i < 5; i++) part_add(x, y, z, rx * (1.f + fx_rand() * 2.f) + (fx_rand() - .5f) * 2.f, .5f + fx_rand() * 2.f, rz * (1.f + fx_rand() * 2.f) + (fx_rand() - .5f) * 2.f, 2.f + fx_rand() * 2.f, .03f, 5, 185 + (int)(fx_rand() * 70.f), 215 + (int)(fx_rand() * 40.f), 235 + (int)(fx_rand() * 20.f));
  for (int i = 0; i < 3; i++) part_add(x, y, z, rx * 2.f + (fx_rand() - .5f) * 4.f, 1.f + fx_rand() * 2.f, rz * 2.f + (fx_rand() - .5f) * 4.f, .3f + fx_rand() * .3f, .05f, 1, 255, 220, 120);
  mirS[s] = 0; mirAcc[s] = 0.f;
}
static void body_fx(float dt) {                            // once per frame, right after impact_fx: breaks windows / mirrors / bumpers according to the hits
  if (dt <= 0.f) return; if (dt > .1f) dt = .1f;
  if (bodyEp != bodyEpoch) { bodyEp = bodyEpoch; body_reset(); return; }
  for (int i = 0; i < NLOOSE; i++) { Loose *q = &lp[i]; if (!q->on) continue;                    // loose parts: fall, bounce, slide to a stop, vanish after a while
    q->life -= dt; if (q->life <= 0.f) { q->on = 0; continue; }
    q->vy -= 14.f * dt; q->x += q->vx * dt; q->y += q->vy * dt; q->z += q->vz * dt;
    float gy = gh(q->x, q->z) + .05f; if (q->y < gy) { q->y = gy; q->vy = q->vy < -1.f ? -q->vy * .3f : 0.f; q->vx *= .7f; q->vz *= .7f; } }
  if (!BODYP) return;
  float hx, hz; heading(&hx, &hz); float rx = hz, rz = -hx, sp = fsqrt(n[4].vx * n[4].vx + n[4].vz * n[4].vz);
  { float vx = ipvx, vz = ipvz, ex = n[4].x - bpx, ez = n[4].z - bpz; bpx = n[4].x; bpz = n[4].z;       // velocity history (cleared by teleports and "stop")
    if (ex * ex + ez * ez > 100.f) bHn = 0;
    if (bHn < 10) { bhx[bHn] = vx; bhz[bHn] = vz; bHn++; } else { for (int i = 0; i < 9; i++) { bhx[i] = bhx[i + 1]; bhz[i] = bhz[i + 1]; } bhx[9] = vx; bhz[9] = vz; } }
  float hm = 0.f, f = 0.f, r = 0.f;                         // hm: speed lost over the window; f / r: how much of it came from the front / the right of the car
  { float ox = bhx[0], oz = bhz[0], os = fsqrt(ox * ox + oz * oz), dx = ox - bhx[bHn - 1], dz = oz - bhz[bHn - 1];
    if (bHn >= 4 && os > 3.f && (dx * ox + dz * oz) > 0.f) { hm = fsqrt(dx * dx + dz * dz); if (hm > 1.f && hm < 60.f) { f = (dx * hx + dz * hz) / hm; r = (dx * rx + dz * rz) / hm; } else hm = 0.f; } }
  for (int q = 0; q < 2; q++) mirT[q] += dt * (5.f + sp * .5f);
  float S[4] = {0.f, 0.f, 0.f, 0.f};                        // glass stress this frame
  if (hm > 4.f) { S[0] = hm * (f > .15f ? f - .15f : 0.f); S[3] = hm * (-f > .15f ? -f - .15f : 0.f);
    S[1] = hm * (-r > .2f ? -r - .2f : 0.f) * 1.2f; S[2] = hm * (r > .2f ? r - .2f : 0.f) * 1.2f; }
  if (ibDvy < -9.f) S[0] += (-ibDvy - 9.f) * .8f;           // hard landing
  if (exploded) for (int w = 0; w < 4; w++) S[w] = 99.f;
  for (int w = 0; w < 4; w++) { float lim1 = 6.5f * glsThr[w], lim2 = (glsS[w] == 1 ? 9.f : 11.5f) * glsThr[w];
    if (glsS[w] < 2 && S[w] > lim2) { glsS[w] = 2; int c = 14 + (int)(S[w] * 1.2f); glass_burst(w, c > 32 ? 32 : c, 1.5f + (S[w] > 14.f ? 14.f : S[w]) * .2f); }
    else if (glsS[w] == 0 && S[w] > lim1) { glsS[w] = 1; glass_burst(w, 4, 1.2f); } }
  float M[2] = {0.f, 0.f};                                  // mirror stress: side hits
  if (hm > 2.f) for (int q = 0; q < 2; q++) { float side = (q ? 1.f : -1.f) * r; M[q] = hm * (side > .12f ? side - .12f : 0.f) * 1.2f; }
  for (int q = 0; q < 2; q++) {
    if (mirS[q] && pAtt0[q ? 5 : 4] > 0 && pAtt[q ? 5 : 4] <= 0) { mirror_tear(q, 0.f); continue; }          // the door left, and its mirror with it
    if (mirS[q] == 1 && M[q] > 4.f * mirThr[q]) mirS[q] = 2;
    if (mirS[q] && M[q] > (mirS[q] == 2 ? 6.f : 7.5f) * mirThr[q]) mirror_tear(q, M[q] > 12.f ? 12.f : M[q]); }
  for (int g = 0; g < NG; g++) {                           // a panel that just came off bursts into pieces
    if (prevAtt[g] > 0 && pAtt[g] <= 0 && pAtt0[g] > 0 && g < 4) { const Veh *V = &VEH[cv]; float cx = 0, cy = 0, cz = 0; for (int j = 0; j < PN[g]; j++) { cx += n[PB[g] + j].x / PN[g]; cy += n[PB[g] + j].y / PN[g]; cz += n[PB[g] + j].z / PN[g]; }
      for (int i = 0; i < 6; i++) { int body = fx_rand() < .6f; part_add(cx, cy, cz, n[4].vx * .6f + (fx_rand() - .5f) * 5.f, 1.5f + fx_rand() * 3.f, n[4].vz * .6f + (fx_rand() - .5f) * 5.f, 1.f + fx_rand(), .07f, 3, body ? V->r : 40, body ? V->g : 40, body ? V->b : 44); } }
    prevAtt[g] = pAtt[g]; }
  for (int g = 1; g <= 2; g++) if (pAtt[g] > 0 && pAtt[g] * 100 < pAtt0[g] * 88) {      // a bumper hanging by a few bolts drags on the road and throws sparks
    for (int j = 0; j < PN[g]; j++) { Node *p = &n[PB[g] + j]; float spd = fsqrt(p->vx * p->vx + p->vz * p->vz);
      if (spd > 2.5f && p->y < gh(p->x, p->z) + .2f && fx_rand() < dt * 28.f)
        for (int q = 0; q < 2; q++) part_add(p->x, p->y, p->z, -p->vx * .15f + (fx_rand() - .5f) * 3.f, 1.f + fx_rand() * 2.5f, -p->vz * .15f + (fx_rand() - .5f) * 3.f, .3f + fx_rand() * .3f, .05f, 1, 255, 220, 120); } }
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
static void ev_fx(float dt) {                     // dark smoke rising from the crashed cars
  if (mapId != 3 || !optEvents || nebx <= 0 || dt <= 0.f) return;
  for (int e = 0; e < nebx; e++) { const EBox *B = &ebx[e]; if (B->kind != EK_WRECK) continue;
    float d = depth_of(B->x, B->z); if (d < 0.f || d > 55.f || fx_rand() > dt * 7.f) continue;
    int g = 52 + (int)(fx_rand() * 24.f);
    part_add(B->x + (fx_rand() - .5f) * 1.2f, 1.2f, B->z + (fx_rand() - .5f) * 1.2f, (fx_rand() - .5f) * .5f, .9f + fx_rand() * .6f, (fx_rand() - .5f) * .5f, 1.8f + fx_rand() * 1.2f, .35f, 0, g, g, g + 4); }
}
// ---- tyre marks: braking, drifting and burn-outs leave dark strips on the ground
typedef struct { float x0, y0, z0, x1, y1, z1; } Skid;
#define SK_MAX 120
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
static void draw_it(int idx) {                      // Italy decor box (walls, roofs, trees...)
  int c = idx / 16 - 100000, q = idx & 15; const ItBox *B = &it_cell(c)->b[q]; float x[8], y[8], z[8];
  for (int i = 0; i < 8; i++) { x[i] = B->x + ((i & 1) ? B->hx : -B->hx); y[i] = (i & 2) ? B->y1 : B->y0; z[i] = B->z + ((i & 4) ? B->hz : -B->hz); }
  draw_prism3(x, y, z, B->r, B->g, B->b);
}
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
// ---- trailers: tank, flatbed with crates, caravan (the simple one is a plain box)
static void trl_z(const TFr *F) { int zq = (int)(depth_of(F->c[0], F->c[2]) * zsc) + 3; curZ = zq > 255 ? 255 : zq; }
// polygon given in trailer-local coordinates (u right, v up, w forward) with its local outward normal; hidden if facing away
static void trl_poly(const TFr *F, const float (*q)[3], int m, float nx, float ny, float nz, int cr, int cg, int cb) {
  float X[12], Y[12], Z[12], sx[12], sy[12], cx = 0, cy = 0, cz = 0;
  for (int i = 0; i < m; i++) {
    X[i] = F->c[0] + F->x[0] * q[i][0] + F->y[0] * q[i][1] + F->z[0] * q[i][2]; Y[i] = F->c[1] + F->x[1] * q[i][0] + F->y[1] * q[i][1] + F->z[1] * q[i][2]; Z[i] = F->c[2] + F->x[2] * q[i][0] + F->y[2] * q[i][1] + F->z[2] * q[i][2];
    cx += X[i] / m; cy += Y[i] / m; cz += Z[i] / m; }
  float Nx = F->x[0] * nx + F->y[0] * ny + F->z[0] * nz, Ny = F->x[1] * nx + F->y[1] * ny + F->z[1] * nz, Nz = F->x[2] * nx + F->y[2] * ny + F->z[2] * nz;
  if (Nx * (camx - cx) + Ny * (camy - cy) + Nz * (camz - cz) <= 0.f) return;
  for (int i = 0; i < m; i++) if (!proj(X[i], Y[i], Z[i], &sx[i], &sy[i])) return;
  float nl = fsqrt(Nx * Nx + Ny * Ny + Nz * Nz) + 1e-4f, lit = (.3f * Nx + .8f * Ny + .5f * Nz) / nl / .97f; if (lit < 0) lit = -lit;
  float sh = .5f + .5f * lit; polyn(sx, sy, m, C((int)(cr * sh), (int)(cg * sh), (int)(cb * sh)));
}
static void draw_tank(const TFr *F) {
  static const float CS[10] = {1, .809f, .309f, -.309f, -.809f, -1, -.809f, -.309f, .309f, .809f}, SN[10] = {0, .588f, .951f, .951f, .588f, 0, -.588f, -.951f, -.951f, -.588f};
  float R = F->hu * .93f, hw = F->hw * .98f, band = F->hw * .22f; trl_z(F);
  for (int k = 0; k < 10; k++) { int j = (k + 1) % 10; float nx = (CS[k] + CS[j]) * .5f, ny = (SN[k] + SN[j]) * .5f;
    float q[4][3] = {{R * CS[k], R * SN[k], -hw}, {R * CS[j], R * SN[j], -hw}, {R * CS[j], R * SN[j], hw}, {R * CS[k], R * SN[k], hw}};
    trl_poly(F, (const float (*)[3])q, 4, nx, ny, 0, 175, 178, 186);
    float b[4][3] = {{R * 1.012f * CS[k], R * 1.012f * SN[k], -band}, {R * 1.012f * CS[j], R * 1.012f * SN[j], -band}, {R * 1.012f * CS[j], R * 1.012f * SN[j], band}, {R * 1.012f * CS[k], R * 1.012f * SN[k], band}};
    trl_poly(F, (const float (*)[3])b, 4, nx, ny, 0, 205, 55, 40); }
  for (int e = 0; e < 2; e++) { float cap[10][3], sg = e ? 1.f : -1.f; for (int k = 0; k < 10; k++) { cap[k][0] = R * CS[k]; cap[k][1] = R * SN[k]; cap[k][2] = hw * sg; }
    trl_poly(F, (const float (*)[3])cap, 10, 0, 0, sg, 130, 134, 142); }
  float hatch[4][3] = {{-.25f, R + .03f, -.1f * hw}, {.25f, R + .03f, -.1f * hw}, {.25f, R + .03f, .3f * hw}, {-.25f, R + .03f, .3f * hw}};
  trl_poly(F, (const float (*)[3])hatch, 4, 0, 1, 0, 60, 62, 68);
  curZ = -1;
}
static void draw_caravan(const TFr *F) {
  draw_box(ntr, 236, 236, 230); trl_z(F);
  float hu = F->hu * 1.004f, hv = F->hv, hw = F->hw;
  for (int sd = -1; sd <= 1; sd += 2) { float u = sd * hu;
    float st[4][3] = {{u, -.42f * hv, -hw}, {u, -.42f * hv, hw}, {u, -.30f * hv, hw}, {u, -.30f * hv, -hw}}; trl_poly(F, (const float (*)[3])st, 4, sd, 0, 0, 40, 90, 150);
    float w1[4][3] = {{u, .05f * hv, -.8f * hw}, {u, .05f * hv, -.35f * hw}, {u, .5f * hv, -.35f * hw}, {u, .5f * hv, -.8f * hw}}; trl_poly(F, (const float (*)[3])w1, 4, sd, 0, 0, 70, 100, 130);
    float w2[4][3] = {{u, .05f * hv, -.2f * hw}, {u, .05f * hv, .25f * hw}, {u, .5f * hv, .25f * hw}, {u, .5f * hv, -.2f * hw}}; trl_poly(F, (const float (*)[3])w2, 4, sd, 0, 0, 70, 100, 130);
    if (sd > 0) { float dr[4][3] = {{u, -.8f * hv, .45f * hw}, {u, -.8f * hv, .8f * hw}, {u, .6f * hv, .8f * hw}, {u, .6f * hv, .45f * hw}}; trl_poly(F, (const float (*)[3])dr, 4, 1, 0, 0, 205, 205, 198);
      float dw[4][3] = {{u, .15f * hv, .52f * hw}, {u, .15f * hv, .73f * hw}, {u, .5f * hv, .73f * hw}, {u, .5f * hv, .52f * hw}}; trl_poly(F, (const float (*)[3])dw, 4, 1, 0, 0, 70, 100, 130); } }
  float vt[4][3] = {{-.3f * F->hu, hv * 1.004f, -.1f * hw}, {.3f * F->hu, hv * 1.004f, -.1f * hw}, {.3f * F->hu, hv * 1.004f, .3f * hw}, {-.3f * F->hu, hv * 1.004f, .3f * hw}};
  trl_poly(F, (const float (*)[3])vt, 4, 0, 1, 0, 190, 195, 200);
  float rw[4][3] = {{-.55f * F->hu, .0f, -hw * 1.004f}, {.55f * F->hu, .0f, -hw * 1.004f}, {.55f * F->hu, .5f * hv, -hw * 1.004f}, {-.55f * F->hu, .5f * hv, -hw * 1.004f}}; trl_poly(F, (const float (*)[3])rw, 4, 0, 0, -1, 70, 100, 130);
  for (int sd = -1; sd <= 1; sd += 2) { float tl[4][3] = {{sd * .72f * F->hu, -.62f * hv, -hw * 1.004f}, {sd * .9f * F->hu, -.62f * hv, -hw * 1.004f}, {sd * .9f * F->hu, -.4f * hv, -hw * 1.004f}, {sd * .72f * F->hu, -.4f * hv, -hw * 1.004f}}; trl_poly(F, (const float (*)[3])tl, 4, 0, 0, -1, 220, 30, 30); }
  curZ = -1;
}
static void draw_flatbed(void) {
  const Trl *T = &TRL[3]; draw_box(ntr, T->r, T->g, T->b);
  int ord[NCRATE]; float dd[NCRATE];
  for (int c = 0; c < ncg; c++) { const Node *p = &n[ntr + 11 + c * 8]; float x = 0, z = 0; for (int k = 0; k < 8; k++) { x += p[k].x * .125f; z += p[k].z * .125f; } ord[c] = c; dd[c] = depth_of(x, z); }
  for (int i = 1; i < ncg; i++) { int a = ord[i]; float v = dd[a]; int j = i - 1; while (j >= 0 && dd[ord[j]] < v) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = a; }   // far to near
  for (int i = 0; i < ncg; i++) { int c = ord[i]; const Obj *O = &OBJ[CRT[c].kind]; draw_box(ntr + 11 + c * 8, O->r, O->g, O->b); }
}
static void draw_trailer(void) {
  const Trl *T = &TRL[trl]; TFr F; trl_frame(&F);
  trailer_wheels(0);
  if (trl == 2) draw_tank(&F); else if (trl == 3) draw_flatbed(); else if (trl == 4) draw_caravan(&F); else draw_box(ntr, T->r, T->g, T->b);
  trailer_wheels(1);
}
static void scene(int xd0) {
  int xl = xd0 * lw / 320; terrain(xl); draw_skids();
  struct { int ty, idx; float d; } L[200]; int m = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
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
  if (mapId == 5) {
    int c0 = it_cz(camz - zmax - 16.f), c1 = it_cz(camz + zmax + 16.f); if (c1 - c0 > 14) c1 = c0 + 14;
    for (int c = c0; c <= c1; c++) { const ItCell *C = it_cell(c);
      for (int q = 0; q < C->n && m < 190; q++) { const ItBox *B = &C->b[q]; float bd = depth_of(B->x, B->z), bs = (B->x - camx) * camhz - (B->z - camz) * camhx;
        if (bd >= -14.f && bd <= zmax + 16.f && bs >= -bd * 1.7f - 18.f && bs <= bd * 1.7f + 18.f) { L[m].ty = 13; L[m].idx = (c + 100000) * 16 + q; L[m++].d = bd; } } }
  }
  if (mapId == 4 && optRamp) for (int li = 0; li < nloopT && m < 150; li++) {
    float lx = LOOPS[li][0], lz = LOOPS[li][1], R = LOOPS[li][2], dc = depth_of(lx, lz), bs = (lx - camx) * camhz - (lz - camz) * camhx;
    if (dc >= -14.f && dc <= zmax + 16.f && bs >= -dc * 1.7f - 20.f && bs <= dc * 1.7f + 20.f) { L[m].ty = 9; L[m].idx = li * 2; L[m++].d = dc + R; L[m].ty = 9; L[m].idx = li * 2 + 1; L[m++].d = dc - R; }
  }
  for (int e = 0; e < NLOOSE && m < 195; e++) if (lp[e].on) { float bd = depth_of(lp[e].x, lp[e].z); if (bd >= -2.f && bd <= zmax + 4.f) { L[m].ty = 12; L[m].idx = e; L[m++].d = bd; } }
  if (mapId == 3 && optEvents) for (int e = 0; e < nebx && m < 190; e++) {                    // random events: crashes, roadworks, convoy
    float bd = depth_of(ebx[e].x, ebx[e].z), bs = (ebx[e].x - camx) * camhz - (ebx[e].z - camz) * camhx;
    if (bd >= -8.f && bd <= zmax + 8.f && bs >= -bd * 1.7f - 12.f && bs <= bd * 1.7f + 12.f) { L[m].ty = 11; L[m].idx = e; L[m++].d = bd; } }
  if (delOn && !delEnd && m < 150) { L[m].ty = 10; L[m].idx = 0; L[m++].d = depth_of(delTx, delTz); }
  for (int i = 1; i < m; i++) { __typeof__(L[0]) v = L[i]; int j = i - 1; while (j >= 0 && L[j].d < v.d) { L[j + 1] = L[j]; j--; } L[j + 1] = v; }
  for (int k = 0; k < m; k++) {
    if (L[k].ty == 0) { car(); draw_fx(); draw_particles(); }
    else if (L[k].ty == 1) draw_trailer();
    else if (L[k].ty == 2 && L[k].d > 1 && L[k].d < zmax - 2 && !objDead[L[k].idx]) { const Obj *O = &OBJ[okind[L[k].idx]]; draw_box(nob0 + L[k].idx * 8, O->r, O->g, O->b); }
    if (L[k].ty == 3 && cityBlocks[L[k].idx].kind == 4 && L[k].d > -12.f && L[k].d < zmax - 2) draw_city_station(L[k].idx);   // stays drawn while you are inside
    else if (L[k].ty == 3 && L[k].d > 1 && L[k].d < zmax - 2) { if (cityBlocks[L[k].idx].kind == 3) draw_city_park(L[k].idx); else draw_city_block(L[k].idx); }
    else if (L[k].ty == 8 && L[k].d > 1 && L[k].d < zmax - 2) draw_city_details(L[k].idx);
    
    else if (L[k].ty == 4 && L[k].d > 1 && L[k].d < zmax - 2) draw_traffic(L[k].idx);
    else if (L[k].ty == 5 && L[k].d > 1 && L[k].d < zmax - 2) draw_city_wall(L[k].idx);
    else if (L[k].ty == 6 && L[k].d > 1 && L[k].d < zmax - 2) draw_police();
    else if (L[k].ty == 7 && L[k].d > 1 && L[k].d < zmax - 2) draw_bot(L[k].idx);
    else if (L[k].ty == 9) draw_loop(L[k].idx >> 1, L[k].idx & 1);
    else if (L[k].ty == 10 && L[k].d > 1.f && L[k].d < 200.f) draw_marker();
    else if (L[k].ty == 11 && L[k].d > 1.f && L[k].d < zmax - 2) draw_ebox(L[k].idx);
    else if (L[k].ty == 12 && L[k].d > 1.f && L[k].d < zmax - 2) draw_loose(L[k].idx);
    else if (L[k].ty == 13 && L[k].d > -10.f && L[k].d < zmax - 2) draw_it(L[k].idx);
  }
  if (weatherMode == 1 || weatherMode == 3) {
    uint16_t streak = weatherMode == 1 ? C(155, 190, 210) : C(205, 218, 220);
    for (int i = 0; i < 14; i++) { int x = (i * 37 + (int)(signalClock * 45.f)) % (lw - 3) + 2, y = (i * 29 + (int)(signalClock * 71.f)) % (lh - 5); line(x, y, x - 2, y + 5, streak); }
  }
  if (showTop && xd0 == 0 && !camInterior) { int bw = lw / 4, bh = lh * 40 / 112; for (int y = lh - bh; y < lh; y++) for (int x = 0; x < bw; x++) fb[y * lw + x] = C(18, 20, 30); clipOn = 1; clX0 = 0; clX1 = bw; clY0 = lh - bh; clY1 = lh; draw_top(bw / 2, lh - bh / 2, 6.5f * lw / 160, 0); clipOn = 0; }
}
// ---- settings, controls
enum { A_ACC, A_BRK, A_LEFT, A_RIGHT, A_RESET, A_HOOD, A_DOORS, A_TRUNK, A_ALL, A_BEAMS, A_TOP, A_DMG, A_CL, A_CR, A_CU, A_CD, A_ZI, A_ZO, A_CRESET, A_QUICK, A_TURBO, A_CINT, A_SKIP, A_QLAST, A_QUICK2, A_GUP, A_GDN, A_TRANS, A_CLUTCH, A_HBRAKE, A_MAP, A_FUEL, A_TSTOP, A_TSLOW, A_TFAST, NA };
static const char *AN[NA] = {"Accelerer", "Freiner", "Gauche", "Droite", "Remettre/Rejouer", "Capot", "Portes", "Coffre", "Tout ouvrir", "Poutres", "Vue dessus", "Degats", "Camera gauche", "Camera droite", "Camera haut", "Camera bas", "Zoom +", "Zoom -", "Camera reset", "Favoris (menu rapide)", "Turbo (tenir)", "Vue habitacle", "Tutoriel: etape suivante", "Refaire option rapide", "Favoris (acces direct)", "Rapport superieur", "Rapport inferieur", "Boite auto/manuelle", "Embrayage (tenir)", "Frein a main (tenir)", "Mini-carte (changer)", "Essence (tenir)", "Temps: stop/reprendre", "Temps: ralentir", "Temps: accelerer"};
static const int BDEF[NA] = {eadk_key_up, eadk_key_down, eadk_key_left, eadk_key_right, eadk_key_ok, eadk_key_var, eadk_key_xnt, eadk_key_exp, eadk_key_shift, eadk_key_toolbox, eadk_key_ln, eadk_key_log, eadk_key_four, eadk_key_six, eadk_key_eight, eadk_key_two, eadk_key_seven, eadk_key_nine, eadk_key_five, eadk_key_exe, eadk_key_backspace, eadk_key_zero, eadk_key_dot, eadk_key_plus, eadk_key_comma, eadk_key_three, eadk_key_one, eadk_key_minus, 13 /* alpha */, 23 /* power */, 24 /* sin */, 25 /* cos */, 28 /* racine: stop */, 33 /* ( : ralentir */, 34 /* ) : accelerer */};
static int bind[NA];
static const char *KN[53] = {[0]="Gauche",[1]="Haut",[2]="Bas",[3]="Droite",[4]="OK",[5]="Retour",[6]="Home",[8]="On/Off",[12]="Shift",[13]="Alpha",[14]="X,n,t",[15]="Var",[16]="Boite outils",[17]="Effacer",[18]="Exp",[19]="Ln",[20]="Log",[21]="i",[22]="Virgule",[23]="Puissance",[24]="Sin",[25]="Cos",[26]="Tan",[27]="Pi",[28]="Racine",[29]="Carre",[30]="7",[31]="8",[32]="9",[33]="(",[34]=")",[36]="4",[37]="5",[38]="6",[39]="x",[40]="/",[42]="1",[43]="2",[44]="3",[45]="+",[46]="-",[48]="0",[49]=".",[50]="EE",[51]="Ans",[52]="EXE"};
static int qual = 2, fovI = 1, vdI = 1, steerL = 3, camL = 3;
static void setq(void) {
  static const int WQ[3] = {112, 136, 160}, HQ[3] = {79, 95, 112}; static const float FV[3] = {1.3f, 1.f, .78f}, ZM[3] = {45.f, 72.f, 110.f};
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
static float startX(void) { return mapId == 0 ? TA : (mapId == 4 && cmap >= 0 ? cmSx : 0.f); }
static float startZ(void) { return mapId == 4 && cmap >= 0 ? cmSz : 0.f; }
static void garage_car(void) { ev_clear(); car_init(startX(), startZ(), 0, 1, .02f, 0); }
static int ct, stopT, score; static float vpk, impV;
static int tutOn, tutStep, tutFlag, tutOk, evDmg, evRepair, mapBak;
static float tutH0x, tutH0z;
static void startpos(void) {
  car_init(startX(), startZ(), 0, 1, .4f, 1); ct = 0; stopT = 0; vpk = 0; impV = 0; score = 0;
  traffic_reset(); skid_clear(); part_clear(); fxSkip = 3;
  fxSeed ^= (unsigned)eadk_timing_millis() * 2654435761u; events_new(!tutOn);   // a different set of events every game
  if (mapId == 2) for (int i = 0; i < (ntr >= 0 ? ntr + ntrN : NC); i++) n[i].vz = CRASHV[crashI] / 3.6f;
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
// Sauvegarde persistante: un fichier "numbeam.sav" dans le systeme de fichiers d'Epsilon (ef_read / ef_write, comme les autres jeux).
// ---- lifetime statistics (shown in the Statistiques menu; saved with the records when NB_STORAGE is defined)
static float stDist, stTop, stDrift, stDriftBest, stDriftCur, stLastDmg, stCool; static int stCrash, stConfirm;
static float stRev, stZen; static int stDeliv, stStops, stBooms, stLoops, stLaps, stVeh; static uint32_t achMask;   // achievements: reverse top speed (km/h), metres since last crash, deliveries, station fill-ups, explosions, loops, circuit laps, vehicles driven (bit mask), unlocked mask
#define REC_FILE "numbeam.sav"
#define REC_MAGIC 0x4E423346u   // layout v2 (achievements added): older saves are ignored
static uint32_t rec_sum(const uint32_t *d, int cnt) { uint32_t s = 0x4E42u; for (int i = 0; i < cnt; i++) s = s * 31u + d[i]; return s; }
static void rec_save(void) {
  uint32_t d[19] = {REC_MAGIC, (uint32_t)delBest[0], (uint32_t)delBest[1], (uint32_t)delBest[2], (uint32_t)recLap, (uint32_t)stDist, (uint32_t)stCrash, (uint32_t)(stTop * 10.f), (uint32_t)(stDrift * 1000.f), (uint32_t)(stDriftBest * 1000.f),
    achMask, (uint32_t)stDeliv, (uint32_t)(stRev * 10.f), (uint32_t)stStops, (uint32_t)stBooms, (uint32_t)stLoops, (uint32_t)stLaps, (uint32_t)stVeh, 0}; d[18] = rec_sum(d, 18);
  ef_write(REC_FILE, d, sizeof(d));
}
static void rec_load(void) {
  uint32_t n = 0; const uint8_t *p = ef_read(REC_FILE, &n);
  uint32_t d[19]; if (!p || n != sizeof(d)) return;
  for (uint32_t i = 0; i < n; i++) ((uint8_t *)d)[i] = p[i];            // le contenu du fichier n'est pas aligne: copie octet par octet
  if (d[0] == REC_MAGIC && d[18] == rec_sum(d, 18)) { delBest[0] = (int)d[1]; delBest[1] = (int)d[2]; delBest[2] = (int)d[3]; recLap = (int)d[4];
    stDist = (float)d[5]; stCrash = (int)d[6]; stTop = d[7] * .1f; stDrift = d[8] * .001f; stDriftBest = d[9] * .001f;
    achMask = d[10]; stDeliv = (int)d[11]; stRev = d[12] * .1f; stStops = (int)d[13]; stBooms = (int)d[14]; stLoops = (int)d[15]; stLaps = (int)d[16]; stVeh = (int)d[17]; }
}
// ---- succes (achievements): 29 medals based on the lifetime statistics. Counter based ones unlock themselves in ach_check();
// AS_EVT ones are unlocked by an event (ach_unlock) and the last one (platinum) by all the others.
enum { AS_DIST, AS_CRASH, AS_DELIV, AS_REV, AS_DBEST, AS_LAPS, AS_STOP, AS_BOOM, AS_TOP, AS_DTOT, AS_LOOP, AS_ZEN, AS_VEH, AS_EVT, AS_PLAT };
typedef struct { const char *nm, *ds; uint8_t src; float tgt; } Ach;   // tgt: metres (AS_DIST, AS_ZEN), km/h (AS_REV, AS_TOP), seconds (AS_DBEST, AS_DTOT), otherwise a count
#define NACH 29
static const Ach ACH[NACH] = {
  {"Premier crash", "Avoir un accident", AS_CRASH, 1}, {"Petite balade", "Rouler 1 km au total", AS_DIST, 1000}, {"Livreur", "Livrer 10 colis au total", AS_DELIV, 10},
  {"Marche arriere express", "100 km/h en marche arriere", AS_REV, 100}, {"Drifteur", "Deriver 10 s d'affilee", AS_DBEST, 10}, {"Premier tour", "Finir un tour de circuit", AS_LAPS, 1},
  {"Pompiste", "Faire le plein a une station", AS_STOP, 1}, {"Boum !", "Faire exploser sa voiture", AS_BOOM, 1},
  {"Pied au plancher", "Atteindre 200 km/h", AS_TOP, 200}, {"Routard", "Rouler 25 km au total", AS_DIST, 25000}, {"Casseur", "Avoir 25 crashs", AS_CRASH, 25},
  {"Facteur fiable", "3 etoiles en livraison (Normal ou +)", AS_EVT, 1}, {"Roi du drift", "Deriver 30 s d'affilee", AS_DBEST, 30}, {"Pneus fumants", "Deriver 5 min au total", AS_DTOT, 300},
  {"Casse-cou", "Finir le crash-test a 140 km/h", AS_EVT, 1}, {"Chauffeur d'elite", "Livrer 50 colis au total", AS_DELIV, 50},
  {"Marathon", "Rouler 100 km au total", AS_DIST, 100000}, {"Globe-trotter", "Rouler 500 km au total", AS_DIST, 500000}, {"Vitesse lumiere", "Atteindre 300 km/h", AS_TOP, 300},
  {"Marche arriere extreme", "150 km/h en marche arriere", AS_REV, 150}, {"Demolisseur", "Avoir 100 crashs", AS_CRASH, 100}, {"Maitre du drift", "Deriver 60 s d'affilee", AS_DBEST, 60},
  {"Drift sans fin", "Deriver 1 h au total", AS_DTOT, 3600}, {"Livreur legendaire", "Livrer 100 colis au total", AS_DELIV, 100}, {"Expert parfait", "3 etoiles en livraison Expert", AS_EVT, 1},
  {"Looping", "Reussir une boucle (carte Vide)", AS_LOOP, 1}, {"Conduite zen", "Rouler 10 km sans crash", AS_ZEN, 10000}, {"Collectionneur", "Conduire les 16 vehicules", AS_VEH, NVB},
  {"Platine", "Debloquer tous les autres succes", AS_PLAT, NACH - 1}};
static uint8_t achQ[6]; static int achQn, achBanId = -1, achDirty; static float achBanT;   // banner queue
static float ach_val(int s) {
  switch (s) {
    case AS_DIST: return stDist; case AS_CRASH: return (float)stCrash; case AS_DELIV: return (float)stDeliv; case AS_REV: return stRev; case AS_DBEST: return stDriftBest;
    case AS_LAPS: return (float)stLaps; case AS_STOP: return (float)stStops; case AS_BOOM: return (float)stBooms; case AS_TOP: return stTop; case AS_DTOT: return stDrift;
    case AS_LOOP: return (float)stLoops; case AS_ZEN: return stZen;
    case AS_VEH: { int c = 0; for (int i = 0; i < NVB; i++) c += (stVeh >> i) & 1; return (float)c; }
    case AS_PLAT: { int c = 0; for (int i = 0; i < NACH - 1; i++) c += (achMask >> i) & 1; return (float)c; }
  }
  return 0.f;
}
static int ach_count(void) { int c = 0; for (int i = 0; i < NACH; i++) c += (achMask >> i) & 1; return c; }
static void ach_unlock(int i) {
  if (i < 0 || i >= NACH || ((achMask >> i) & 1)) return;
  achMask |= 1u << i; achDirty = 1; if (achQn < 6) achQ[achQn++] = (uint8_t)i;
}
static void ach_check(void) {                                       // unlocks every medal whose counter reached its target, saves when something changed
  for (int i = 0; i < NACH; i++) { const Ach *A = &ACH[i]; if (((achMask >> i) & 1) || A->src == AS_EVT) continue; if (ach_val(A->src) >= A->tgt) ach_unlock(i); }
  if (achDirty) { achDirty = 0; rec_save(); }
}
static void ach_frame(float dt, float sp, float vf) {                // once per frame while driving
  static int pst, pex, loopStage, loopLi; static float loopT;
  if (dt <= 0.f) return;
  if (dt > .1f) dt = .1f;
  if (vf < -.5f && !exploded) { float r = -vf * 3.6f; if (r > stRev && r < 700.f) stRev = r; }          // backwards speed (engine reverse is capped, a slide gets much higher)
  if (sp > 2.f && cv >= 0 && cv < NVB) stVeh |= 1 << cv;                                                  // vehicle driven
  { int cur = stInZone && stRefuel; if (cur && !pst) stStops++; pst = cur; }                             // a station fill-up (not the refuel key)
  if (exploded && !pex) stBooms++;
  pex = exploded;
  if (mapId == 4 && optRamp) {                                                                           // vertical loop: reach the top, then come out of the ring on the exit lane
    float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
    if (loopStage == 0) { for (int li = 0; li < nloopT; li++) { float lx = LOOPS[li][0], lz = LOOPS[li][1], R = LOOPS[li][2];
        if (absf(cx - lx) < 9.f && absf(cz - lz) < R && cy > 2.f * R - 2.5f) { loopStage = 1; loopLi = li; loopT = 0.f; break; } } }
    else { float lx = LOOPS[loopLi][0], lz = LOOPS[loopLi][1], R = LOOPS[loopLi][2]; loopT += dt;
      if (loopT > 8.f || absf(cx - lx) > 10.f || absf(cz - lz) > R + 5.f) loopStage = 0;
      else if (cy < 3.f && cz > lz - 1.f && absf(cx - (lx + LOOP_L * .5f)) < 4.5f) { stLoops++; loopStage = 0; } }
  } else loopStage = 0;
  if (achBanT > 0.f) achBanT -= dt;
  if (achBanT <= 0.f) { achBanId = -1; if (achQn > 0) { achBanId = achQ[0]; for (int i = 1; i < achQn; i++) achQ[i - 1] = achQ[i]; achQn--; achBanT = 4.f; } }
  ach_check();
}
static void ach_banner_bg(void) {                                    // gold banner at the top of the 3D view (the text is pushed after the frame)
  if (achBanId < 0) return;
  curZ = -1; blendA = 0; clipOn = 0; int bh = 20 * lh / 224 + 2, x0 = lw * 8 / 100, w = lw * 84 / 100;
  frect(x0, 1, w, bh, C(24, 20, 8)); frect(x0, 1, w, 1, C(255, 200, 60)); frect(x0, bh, w, 1, C(255, 200, 60));
}
static void ach_banner_txt(void) {
  if (achBanId < 0) return;
  char s[64], *p = s; p = cat(p, "SUCCES: "); cat(p, ACH[achBanId].nm); txt(s, 30, 4, 0, C(255, 210, 80), C(24, 20, 8));
}
static void ach_prog(char *out, int i) {                             // progress text of one medal
  const Ach *A = &ACH[i]; char *p = out; *p = 0;
  if ((achMask >> i) & 1) { cat(p, "OK"); return; }
  float c = ach_val(A->src), t = A->tgt; if (c > t) c = t;
  switch (A->src) {
    case AS_DIST: case AS_ZEN: p = dec1(p, c * .001f); p = cat(p, "/"); p = dec1(p, t * .001f); cat(p, " km"); break;
    case AS_REV: case AS_TOP: p = num(p, (int)c); p = cat(p, "/"); p = num(p, (int)t); cat(p, " km/h"); break;
    case AS_DBEST: p = num(p, (int)c); p = cat(p, "/"); p = num(p, (int)t); cat(p, " s"); break;
    case AS_DTOT: p = num(p, (int)(c / 60.f)); p = cat(p, "/"); p = num(p, (int)(t / 60.f)); cat(p, " min"); break;
    case AS_EVT: cat(p, "--"); break;
    default: p = num(p, (int)c); p = cat(p, "/"); num(p, (int)t);
  }
}
static void draw_ach(int sel) {
  clear(); txt("SUCCES", 125, 3, 1, ACC, BGC);
  { char s[24], *p = s; p = num(p, ach_count()); p = cat(p, "/"); num(p, NACH); txt(s, 262, 8, 0, 0xFFFF, BGC); }
  int top = sel - 3; if (top > NACH - 8) top = NACH - 8; if (top < 0) top = 0;
  for (int r = 0; r < 8; r++) {
    int i = top + r, y = 26 + r * 22, on = (achMask >> i) & 1, se = i == sel; uint16_t bg = se ? PSEL : PNL;
    eadk_display_push_rect_uniform((eadk_rect_t){10, y, 300, 20}, bg); if (se) eadk_display_push_rect_uniform((eadk_rect_t){10, y, 3, 20}, ACC);
    char nm[48], *q = nm; q = cat(q, on ? "[X] " : "[ ] "); cat(q, ACH[i].nm); txt(nm, 20, y + 3, 0, on ? C(255, 210, 80) : (se ? 0xFFFF : C(150, 154, 166)), bg);
    char pr[32]; ach_prog(pr, i); int len = 0; while (pr[len]) len++; txt(pr, 300 - len * 6, y + 3, 0, on ? C(120, 230, 130) : C(185, 188, 198), bg);
  }
  txt(ACH[sel].ds, 20, 204, 0, C(255, 210, 80), BGC);
  txt("Haut/Bas: defiler   Retour: menu", 20, 222, 0, C(150, 160, 190), BGC);
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
    if (ev_near(x, z, 10.f)) continue;
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
  if (delStars == 3 && delLvl >= 1) ach_unlock(11);
  if (delStars == 3 && delLvl == 2) ach_unlock(24);
  ach_check();
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
    delMsgKind = 1; delMsgScore = sc; delMsgT = 2.5f; fuel = 1.f; done = 1; stDeliv++; ach_check();
  } else if (delT <= 0.f) {                                             // too late: this parcel is lost
    legOk[delIdx] = 0; legScore[delIdx] = 0; legTime[delIdx] = delLimit; delMsgKind = 2; delMsgT = 2.5f; done = 1;
  }
  if (done) { delIdx++; if (delIdx >= delN) { deliv_finish(1); return; } deliv_pick(cx, cz); }
  if (driveEff < .05f) delDeadT += dt; else delDeadT = 0.f;
  if (exploded || (burning && fireT > 8.f) || delDeadT > 6.f) deliv_finish(2);
}
// ---- city minimap: corner (north up) or full screen; shows your car, the delivery destination, the police and the events
static int optMap = 1, mmIcon = 1;                       // optMap: 0 off, 1 corner, 2 full screen; mmIcon: icon size 0 small, 1 normal, 2 large
static const char *MMI[3] = {"petites", "normales", "grandes"};
static const char *MMN[3] = {"non", "coin", "plein ecran"};
static void draw_minimap(int top) {                    // top = 0: map and events (under the gauges), 1: your car, destination, police (above the gauges)
  int full = optMap == 2, blink = ((int)(fxClock * 5.f)) & 1; float s, hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  curZ = -1; blendA = 0; clipOn = 0;
  if (full) { s = (lw - 4.f) / (2.f * CITY_X_LIMIT); float s2 = (lh - 4.f) / (2.f * CITY_Z_LIMIT); if (s2 < s) s = s2; } else s = lw * .23f / (2.f * CITY_X_LIMIT);
  int mw = (int)(2.f * CITY_X_LIMIT * s), mh = (int)(2.f * CITY_Z_LIMIT * s), ox = full ? (lw - mw) / 2 : 3, oy = full ? (lh - mh) / 2 : 3;
  if (!top) {
  if (full) { blendA = 232; frect(0, 0, lw, lh, C(8, 10, 16)); }
  blendA = 0; frect(ox - 1, oy - 1, mw + 2, mh + 2, C(96, 104, 116)); frect(ox, oy, mw, mh, C(34, 37, 42));   // frame + asphalt
  for (int i = 0; i < 64; i++) { const CityBlock *B = &cityBlocks[i];
    int x0 = ox + (int)((B->x - 8.5f + CITY_X_LIMIT) * s), y0 = oy + (int)((CITY_Z_LIMIT - B->z - 11.f) * s), w = (int)(17.f * s + .5f), h = (int)(22.f * s + .5f); if (w < 1) w = 1; if (h < 1) h = 1;
    frect(x0, y0, w, h, B->kind == 4 ? C(214, 150, 40) : B->kind == 3 ? C(46, 118, 62) : (B->kind == 1 ? C(122, 130, 146) : (B->kind == 2 ? C(100, 92, 86) : C(84, 90, 102)))); }
  blendA = 0; }
  int d = (full && mmIcon) ? 2 : 1, rr = (full ? 2 : 1) + mmIcon;
  if (!top && optEvents) for (int e = 0; e < nebx; e++) { const EBox *B = &ebx[e]; if (!B->solid || B->kind == EK_PILE) continue;
    uint16_t c = B->mv ? C(240, 240, 240) : (B->kind == EK_WRECK ? C(240, 60, 50) : (B->kind == EK_BARRIER || B->kind == EK_TRUCKW ? C(255, 150, 30) : C(150, 200, 255)));
    frect(ox + (int)((B->x + CITY_X_LIMIT) * s), oy + (int)((CITY_Z_LIMIT - B->z) * s), d, d, c); }
  if (!top) return;
  if (pursuitEnabled) { int r = rr; uint16_t c = blink ? C(255, 50, 50) : C(60, 90, 255);
    disc(ox + (int)((policeCar.x + CITY_X_LIMIT) * s), oy + (int)((CITY_Z_LIMIT - policeCar.z) * s), r, c); }
  if (delOn && !delEnd) { int r = rr, tx = ox + (int)((delTx + CITY_X_LIMIT) * s), ty = oy + (int)((CITY_Z_LIMIT - delTz) * s);
    disc(tx, ty, r, blink ? C(255, 190, 40) : C(255, 120, 20)); ring2(tx, ty, r + 2, 1, C(255, 210, 90)); }
  { int r = rr, px0 = ox + (int)((cx + CITY_X_LIMIT) * s), py0 = oy + (int)((CITY_Z_LIMIT - cz) * s);   // you, with your heading
    line(px0, py0, px0 + (int)(hx * (r + 4)), py0 - (int)(hz * (r + 4)), 0xFFFF); disc(px0, py0, r, C(60, 180, 255)); }
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
static const char *MAPN[6] = {"Circuit", "Route", "Crash-test", "Ville", "Vide", "Italie"}, *SOLN[3] = {"Robuste", "Normale", "Fragile"};
static const char *MN[7] = {"Jouer", "Carte", "Garage", "Defis", "Profil", "Options", "Quitter"};
// ======================= MODS PERSONNALISES (scripts .py de la calculatrice) =======================
// Voir MANUEL_MODS.md. Un script commence par "# numbeam" et ne contient que des appels de fonctions (vehicule, carte, rampe, pouvoir, mode...).
#define NDEV 10
static int ri = 1, gi = 1; static float steerBase = .35f;                    // ri: adherence de la route, gi: gravite (indice du preset); steerBase: braquage de base
static const float ROADF[3] = {.6f, 1.f, 1.3f}, GRAVP[3] = {7.f, 14.f, 22.f};
#define MOD_MAXV (NV - NVB)
#define MOD_MAXC 4
#define MOD_MAXP 8
#define MOD_MAXM 8
#define MOD_MAXR 16
#define MOD_POW0 144     // identifiants de lignes de menu: 144-151 pouvoirs, 152-159 modes
#define MOD_MODE0 152
typedef struct { char nm[16]; uint8_t nr, nl, no, col, cr, cg, cb; float grav, grip, hA, hL, sx, sz; Rmp r[MOD_MAXR]; float lp[NLOOPMAX][3]; uint8_t ok[12]; float ox[12], oz[12]; } ModMap;
typedef struct { char nm[16]; uint8_t stop, rep, fuel, hasTp, turbo; float jump, boost, tp[2], dur; uint16_t set; float val[NDEV]; } ModPow;
typedef struct { char nm[16], mapS[16], vehS[16]; int8_t mapI, vehI, eng, whl, sus, cha, trl, wea; int16_t traf; uint16_t set; float val[NDEV]; } ModMode;
static ModMap cmaps[MOD_MAXC]; static ModPow mpows[MOD_MAXP]; static ModMode mmodes[MOD_MAXM];
static char modVName[MOD_MAXV][16], modErr[64];
static int nCm, nPow, nModes, nModFiles, modeAct = -1, powAct = -1, powTurbo;
static uint16_t mdSet, pwSet; static float mdVal[NDEV], pwVal[NDEV], powT;   // mdSet/mdVal: reglages imposes par le mode actif ; pwSet/pwVal/powT: pouvoir temporaire en cours
static int modIdOk(int id) { if (id >= MOD_MODE0) return id - MOD_MODE0 < nModes; return id >= MOD_POW0 && id - MOD_POW0 < nPow; }
#define NMAPB 6   // cartes integrees: Circuit, Route, Crash-test, Ville, Vide, Italie
// cartes: les 6 cartes d'origine puis les cartes perso (qui utilisent le moteur de la carte "Vide": mapId reste 4, cmap = indice de la carte perso)
static int map_total(void) { return NMAPB + nCm; }
static int map_cur(void) { return mapId == 4 && cmap >= 0 ? NMAPB + cmap : mapId; }
static const char *map_name(void) { return mapId == 4 && cmap >= 0 ? cmaps[cmap].nm : MAPN[mapId]; }
static void cmap_use(int k) {
  G = (k >= 0 && cmaps[k].grav > 0.f) ? cmaps[k].grav : GRAVP[gi]; gripF = (k >= 0 && cmaps[k].grip > 0.f) ? cmaps[k].grip : ROADF[ri];
  cmap = k; cmHillA = 0.f; cmHillL = 30.f; cmSx = cmSz = 0.f; cmCol = 0; cmNobj = 0;
  if (k < 0) { rmpT = RMPD; nrmpT = NRMPD; nloopT = 2; for (int i = 0; i < 2; i++) for (int j = 0; j < 3; j++) LOOPS[i][j] = LOOPD[i][j]; return; }
  const ModMap *M = &cmaps[k]; rmpT = M->r; nrmpT = M->nr; nloopT = M->nl;
  for (int i = 0; i < M->nl; i++) for (int j = 0; j < 3; j++) LOOPS[i][j] = M->lp[i][j];
  cmHillA = M->hA; cmHillL = M->hL; cmSx = M->sx; cmSz = M->sz; cmCol = M->col; cmR = M->cr; cmG = M->cg; cmB = M->cb; cmNobj = M->no;
  for (int i = 0; i < M->no; i++) { cmOk[i] = M->ok[i]; cmOx[i] = M->ox[i]; cmOz[i] = M->oz[i]; }
}
static void map_set(int u) { int t = map_total(); u = ((u % t) + t) % t; if (u < NMAPB) { mapId = u; cmap_use(-1); } else { mapId = 4; cmap_use(u - NMAPB); } }
static float menuT; static int menuPlain;   // menuPlain: 1er lancement = ecran d'accueil sans scene 3D (la map n'apparait qu'apres)
static void menu_init(void) { garage_car(); traffic_reset(); skid_clear(); part_clear(); menuT = 0.f; }
static void menu_bg(void) {                // slow cinematic orbit around the selected map, running live physics / traffic
  thr = 0; brk = 0; steer = 0; turbo = 0;
  for (int i = 0; i < 4; i++) world_step(.0035f);
  traffic_update(.05f); fx_update(.05f); bots_frame(.05f); menuT += .05f;
  float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz);
  float ang = menuT * .14f + .6f, dist = 12.f + 4.f * fsin(menuT * .09f) + (ntr >= 0 ? 3.f + TRL[trl].cam : 0.f), el = 1.6f + 1.4f * (fsin(menuT * .06f) + 1.f);
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
  for (int i = 0; i < 7; i++) { char s[40], *p = s; p = cat(p, MN[i]);
    if (i == 1) { p = cat(p, ": "); cat(p, map_name()); }
    else if (i == 4) { p = cat(p, ": "); p = num(p, ach_count()); p = cat(p, "/"); p = num(p, NACH); cat(p, " succes"); }
    row(s, 4, 52 + i * 24, 144, 22, i == sel); }
  if (menuPlain) {
    eadk_display_push_rect_uniform((eadk_rect_t){154, 0, 166, 224}, BGC);
    txt("Bienvenue !", 190, 80, 1, 0xFFFF, BGC);
    txt("OK sur Jouer pour conduire", 172, 112, 0, ACC, BGC);
    txt("Garage / Options: a ton rythme", 166, 128, 0, C(130, 134, 146), BGC);
  }
  txt("Haut/Bas  Gauche/Droite  OK", 4, 226, 0, C(110, 114, 126), BGC);
  char t[40], *q = t; q = cat(q, "Carte: "); cat(q, map_name()); txt(t, 162, 226, 0, ACC, BGC);
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
  const char *val[11] = {"OK: orbite", VEH[cv].nm, seed, WHL[cw].nm, SUS[cs].nm, ENG[ce].nm, CHA[cc].nm, TRL[trl].nm, tHood > .5f ? "ouvert" : "ferme", tDoor > .5f ? "ouvertes" : "fermees", tTrunk > .5f ? "ouvert" : "ferme"};
  for (int i = 0; i < 12; i++) grow(lab[i], i < 11 ? val[i] : "", 19 + i * 15, i == sel);
  if (gv) { eadk_display_push_rect_uniform((eadk_rect_t){0, 200, 160, 40}, BGC); txt("Fleches: tourner/hauteur", 4, 200, 0, ACC, BGC); txt("+ / - : zoom", 4, 212, 0, ACC, BGC); txt("OK ou Retour: fin", 4, 224, 0, ACC, BGC); return; }
  gbar("Puiss", ENG[ce].a * VEH[cv].pw / 23.f, 201);
  gbar("Grip", WHL[cw].gr * VEH[cv].gr / .45f, 214);
  gbar("Solid", CHA[cc].k / 140.f * SOLF[solid] / 230.f, 227);
}
static void draw_pause(int sel) {
  const char *it[7] = {"Reprendre", "Recommencer", "Favoris", "Options", "Degats (dessus)", "Garage", "Menu principal"};
  eadk_display_push_rect_uniform((eadk_rect_t){70, 8, 180, 224}, BGC); txt("PAUSE", 140, 12, 0, 0xFFFF, BGC);
  for (int i = 0; i < 7; i++) row(it[i], 80, 32 + i * 28, 160, 24, i == sel);
}
static void draw_ctrl(int sel, int cap) {
  clear(); txt("COMMANDES", 115, 2, 1, ACC, BGC);
  int top = sel - 4; if (top < 0) top = 0; if (top > NA + 1 - 9) top = NA + 1 - 9;
  for (int i = 0; i < 9; i++) { int a = top + i; char s[48], *p = s; if (a > NA) break;
    if (a == NA) cat(s, "Restaurer les defauts"); else { p = cat(p, AN[a]); p = cat(p, ": "); cat(p, KN[bind[a]] ? KN[bind[a]] : "?"); if (cap && a == sel) cat(s, "  (appuie sur une touche)"); }
    row(s, 10, 24 + i * 21, 300, 19, a == sel); }
  txt(cap ? "Retour: annuler" : "OK: changer  Retour: quitter", 10, 220, 0, C(150, 160, 190), BGC);
}
static int turboT;
// ---- reglages de base (presets rapides) et variables du mode developpeur
static float *const DEVP[NDEV] = {&engV, &engA, &latG, &G, &gripF, &devTV, &devTA, &tuneBrake, &steerBase, &devEB};   // meme ordre que DEVM
static int devOn; static uint16_t devSet; static float devOv[NDEV];         // devSet: bit i = variable i imposee ; devOv: sa valeur
static float dev_def(int i) {                                                // valeur normale (sans mode dev) de la variable i
  switch (i) {
    case 0: return ENG[ce].v;
    case 1: return ENG[ce].a * VEH[cv].pw * tunePower;
    case 2: { float g = WHL[cw].gr * VEH[cv].gr * tuneGrip; return g > .45f ? .45f : g; }
    case 3: return (mapId == 4 && cmap >= 0 && cmaps[cmap].grav > 0.f) ? cmaps[cmap].grav : GRAVP[gi];
    case 4: return (mapId == 4 && cmap >= 0 && cmaps[cmap].grip > 0.f) ? cmaps[cmap].grip : ROADF[ri];
    case 5: return 1.5f;
    case 6: return 2.2f;
    case 7: return 1.f;
    case 8: return .35f;
    default: return .018f;
  }
}
static void dev_apply(void) {                                                // couches: reglages du mode actif < mode developpeur < pouvoir en cours
  if (n != nPl) return;
  for (int i = 0; i < NDEV; i++) { if ((mdSet >> i) & 1) *DEVP[i] = mdVal[i]; if (devOn && ((devSet >> i) & 1)) *DEVP[i] = devOv[i]; if (powT > 0.f && ((pwSet >> i) & 1)) *DEVP[i] = pwVal[i]; }
}
static void dev_reset(int i) { devSet &= (uint16_t)~(1u << i); *DEVP[i] = dev_def(i); dev_apply(); }
static void dev_reset_all(void) { devSet = 0; for (int i = 0; i < NDEV; i++) *DEVP[i] = dev_def(i); dev_apply(); }
#define QUICK_COUNT 37
// identifiants (anciens 0-21 conserves, nouveaux 22-25)
static const char *QL[QUICK_COUNT] = {"Reparer tout", "Turbo", "Boost !", "Voiture", "Moteur", "Remorque", "Route", "Gravite", "Temps", "Retour au depart", "Trafic IA", "Nombre IA", "Comportement IA", "Vitesse IA", "Plan de ville", "Meteo", "Puissance", "Adherence", "Suspension", "Freinage", "Poursuite police", "Test: percer>feu>boom", "Saut !", "Stop (frein d'urgence)", "Roues", "Carte", "Boite de vitesses", "Crevaison", "Embrayage", "Ceinture", "Airbags", "Evenements (OK: refaire)", "Mini-carte", "Icones carte", "Carrosserie cassable", "Consommation", "Plein d'essence"};
static int lastQid = 0, lastQd = 1;                      // derniere option utilisee (touche "Refaire option rapide")
static void qdesc(int id, char *s) {        // texte d'une option rapide (nom + valeur)
  static const char *RN[3] = {"Seche", "Mouillee", "Glace"}, *GN[3] = {"Normale", "Lune", "Forte"};
  static const char *AI_MODE[3] = {"Circuit", "Suit joueur", "Patrouille"}, *AI_SPEED[5] = {"Tres lent", "Lent", "Normal", "Rapide", "Tres rapide"};
  static const char *CITY_PLAN[3] = {"Regulier", "Decale", "Canalise"}, *WEATHER[4] = {"Clair", "Pluie", "Brouillard", "Glace"};
  char *p = s;
    p = cat(p, QL[id]);
    if (id == 1) cat(p, turboT ? ": oui" : ": non"); else if (id == 10 && mapId != 3) cat(p, trafficEnabled ? ": oui (Ville)" : ": non (Ville)"); else if (id == 3) { p = cat(p, ": "); cat(p, VEH[cv].nm); } else if (id == 4) { p = cat(p, ": "); cat(p, ENG[ce].nm); }
    else if (id == 5) { p = cat(p, ": "); cat(p, TRL[trl].nm); } else if (id == 6) { p = cat(p, ": "); cat(p, RN[ri]); } else if (id == 7) { p = cat(p, ": "); cat(p, GN[gi]); } else if (id == 8) { p = cat(p, ": "); cat(p, TSN[tsi]); }
    else if (id == 10) cat(p, trafficEnabled ? ": oui" : ": non"); else if (id == 11) { p = cat(p, ": "); p = num(p, trafficCount); cat(p, "/64"); }
    else if (id == 12) { p = cat(p, ": "); cat(p, AI_MODE[trafficBehavior]); } else if (id == 13) { p = cat(p, ": "); cat(p, AI_SPEED[trafficSpeed]); }
    else if (id == 14) { p = cat(p, ": "); cat(p, CITY_PLAN[cityPlan]); } else if (id == 15) { p = cat(p, ": "); cat(p, WEATHER[weatherMode]); }
    else if (id >= 16 && id <= 19) { float v = id == 16 ? tunePower : (id == 17 ? tuneGrip : (id == 18 ? tuneSusp : tuneBrake)); p = cat(p, ": "); p = num(p, (int)(v * 100.f)); cat(p, "%"); }
    else if (id == 20) cat(p, pursuitEnabled ? ": active" : ": inactive");
    else if (id == 24) { p = cat(p, ": "); cat(p, WHL[cw].nm); } else if (id == 25) { p = cat(p, ": "); cat(p, map_name()); } else if (id == 26) cat(p, manualGear ? ": Manuelle" : ": Auto");
    else if (id == 28) cat(p, clutchAssist ? ": Assistee" : ": Manuelle"); else if (id == 29) cat(p, beltOn ? ": bouclee" : ": detachee"); else if (id == 30) cat(p, airbagsOn ? ": actifs" : ": desactives");
    else if (id == 27) { int c = 0; for (int w = 0; w < 4; w++) c += tpLeak[w]; p = cat(p, ": "); p = num(p, c); cat(p, "/4 (OK)"); }
    if (id == 31) cat(p, optEvents ? (mapId == 3 ? ": oui" : ": oui (Ville)") : ": non");
    if (id == 32) { p = cat(p, ": "); cat(p, MMN[optMap]); if (mapId != 3 && optMap) cat(p, " (Ville)"); }
    if (id == 33) { p = cat(p, ": "); cat(p, MMI[mmIcon]); }
    if (id == 34) cat(p, optBody ? ": oui" : ": non");
    if (id == 35) { p = cat(p, ": x"); num(p, fuelX); }
}
// applique une option rapide. ok = OK appuye (requis pour les actions "ponctuelles"). Retour : bit0 = fermer le menu, bit1 = remettre les tours a zero
static int quick_apply(int id, int d, int ok) {
  int close = 0, lapReset = 0; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); fxSkip = 3;
  int cnt = ntr >= 0 ? ntr + ntrN : NC;
  if (id == 0 && ok) { repair(cx, cz, hx, hz); if (tutOn) evRepair = 1; close = 1; }
  else if (id == 1) turboT ^= 1;
  else if (id == 2 && ok) { for (int i = 0; i < cnt; i++) { n[i].vx += hx * 10.f; n[i].vz += hz * 10.f; } close = 1; }
  else if (id == 22 && ok) { for (int i = 0; i < cnt; i++) n[i].vy += 7.f; close = 1; }
  else if (id == 23 && ok) { for (int i = 0; i < cnt; i++) n[i].vx = n[i].vy = n[i].vz = 0.f; bHn = 0; close = 1; }
  else if (id == 3) { cv = (cv + nVeh + d) % nVeh; if (cv == 4) gen_car(genSeed); if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
  else if (id == 24) { cw = (cw + 4 + d) % 4; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
  else if (id == 4) { ce = (ce + 4 + d) % 4; engA = ENG[ce].a * VEH[cv].pw * tunePower; engV = ENG[ce].v; }
  else if (id == 5) { trl = (trl + NTRL + d) % NTRL; if (mapId == 2) startpos(); else car_init(cx, cz, hx, hz, .5f, 1); close = 1; }
  else if (id == 6) { ri = (ri + 3 + d) % 3; gripF = ROADF[ri]; } else if (id == 7) { gi = (gi + 3 + d) % 3; G = GRAVP[gi]; }
  else if (id == 8) { tsi = (tsi - 1 + 5 + d) % 5 + 1; tscale = TSV[tsi]; }
  else if (id == 9 && ok) { startpos(); lapReset = 1; close = 1; }
  else if (id == 25 && !tutOn && !delOn) { map_set(map_cur() + d); startpos(); lapReset = 1; close = 1; }
  else if (id == 26) { manualGear ^= 1; }
  else if (id == 28) clutchAssist ^= 1; else if (id == 29) beltOn ^= 1; else if (id == 30) airbagsOn ^= 1;
  else if (id == 27 && ok && optPunct) { int w = 0; while (w < 4 && tpLeak[w]) w++; if (w < 4) tpLeak[w] = 1; else for (int q = 0; q < 4; q++) { tpLeak[q] = 0; tirePres[q] = 1.f; } }   // burst the next tyre; all flat -> re-inflate
  else if (id == 10) { trafficEnabled ^= 1; traffic_reset(); }
  else if (id == 11) { trafficCount += d; if (trafficCount < 0) trafficCount = 64; if (trafficCount > 64) trafficCount = 0; traffic_reset(); }
  else if (id == 12) { trafficBehavior = (trafficBehavior + 3 + d) % 3; traffic_reset(); }
  else if (id == 13) { trafficSpeed = (trafficSpeed + 5 + d) % 5; traffic_reset(); }
  else if (id == 14) { cityPlan = (cityPlan + 3 + d) % 3; events_new(!tutOn); }   // the roads move: roll the events again
  else if (id == 32) optMap = (optMap + 3 + d) % 3;
  else if (id == 33) mmIcon = (mmIcon + 3 + d) % 3;
  else if (id == 34) body_toggle();
  else if (id == 36 && ok) { fuel = 1.f; close = 1; }
  else if (id == 35) fuelX = fuelX == 1 ? 3 : (fuelX == 3 ? 6 : 1);
  else if (id == 31) { if (ok) { optEvents = 1; events_new(!tutOn); close = 1; } else { optEvents ^= 1; if (optEvents) events_new(!tutOn); else ev_clear(); } }
  else if (id == 15) { weatherMode = (weatherMode + 4 + d) % 4; }
  else if (id >= 16 && id <= 19) {
    float *tune = id == 16 ? &tunePower : (id == 17 ? &tuneGrip : (id == 18 ? &tuneSusp : &tuneBrake));
    float old = *tune; *tune += d * .1f; if (*tune < .5f) *tune = .5f; if (*tune > 1.5f) *tune = 1.5f;
    if (id == 16) engA = ENG[ce].a * VEH[cv].pw * tunePower;
    if (id == 17) { latG = WHL[cw].gr * VEH[cv].gr * tuneGrip; if (latG > .45f) latG = .45f; }
    if (id == 18 && old > 0.f) for (int b = 0; b < nb; b++) if (bm[b].f == 1) { bm[b].k *= *tune / old; bm[b].c *= *tune / old; }
  } else if (id == 20) { pursuitEnabled ^= 1; traffic_reset(); }
  else if (id == 21 && ok) { static int demo; demo = (demo + 1) % 3; if (demo == 1) tankHp = .3f; else if (demo == 2) { burning = 1; fireT = 0.f; fireI = .1f; boomAt = 1e9f; } else explode(); close = 1; }
  dev_apply();                                                  // mode dev: les valeurs imposees priment sur les options normales
  return close | (lapReset << 1);
}
// ======================= MENUS: pages, favoris, mode Simple/Avance, mode developpeur =======================
// Un seul systeme de pages pour tout: chaque ligne est un identifiant (0-36 options rapides, 40+ reglages, 100+ = sous-page, 128+ = variable dev).
// Les lignes marquees AV n'apparaissent qu'en mode Avance. Shift = ajouter/retirer des favoris. Les favoris s'ouvrent avec la touche "menu rapide".
#define AV 0x8000u
enum { P_FAV, P_OPT, P_VEH, P_WORLD, P_DRIVE, P_DISP, P_TOOLS, P_SYS, P_DEV, P_CHALL, P_PROF, P_MODS, NPAGE };
enum { O_QUAL = 40, O_STEER, O_CAM, O_FOV, O_VIEW, O_SKID, O_PART, O_OBJ, O_RAMP, O_PUNCT, O_KEYS, O_CTRL, O_SOLID, O_MODE, O_DEV, O_CRASHV, O_FAVRESET, O_DEVRESET, O_TUTO, O_DELIV, O_STATS, O_ACH, O_MODINFO, O_MODRELOAD };
#define NAV(p) (100 + (p))
#define DV(i) (128 + (i))
#define NFAV 8
static const char *const PGN[NPAGE] = {"FAVORIS", "OPTIONS", "VEHICULE", "MONDE", "CONDUITE", "AFFICHAGE", "OUTILS (EN JEU)", "SYSTEME", "VARIABLES DEV", "DEFIS", "PROFIL", "MODES PERSO"};
static const char *const PGL[NPAGE] = {"Favoris", "Options", "Vehicule", "Monde", "Conduite", "Affichage", "Outils (en jeu)", "Systeme", "Variables dev", "Defis", "Profil", "Modes perso"};
static const uint16_t PT_OPT[] = {O_MODE, NAV(P_VEH), NAV(P_WORLD), NAV(P_DRIVE), NAV(P_DISP), NAV(P_TOOLS), NAV(P_SYS), NAV(P_MODS), NAV(P_FAV)};
static const uint16_t PT_VEH[] = {3, 4, 24, 5, O_SOLID, 26, 28 | AV, 16 | AV, 17 | AV, 18 | AV, 19 | AV, 35 | AV, 34 | AV, 29 | AV, 30 | AV};
static const uint16_t PT_WORLD[] = {25, 6, 15, 7, 10, 11, 31, 12 | AV, 13 | AV, 20 | AV, 14 | AV, O_OBJ | AV, O_RAMP | AV};
static const uint16_t PT_DRIVE[] = {8, 1, O_STEER, O_CAM, O_CTRL, O_PUNCT | AV};
static const uint16_t PT_DISP[] = {O_QUAL, 32, 33 | AV, O_FOV | AV, O_VIEW | AV, O_SKID | AV, O_PART | AV};
static const uint16_t PT_TOOLS[] = {0, 36, 2, 22, 23, 9, 27 | AV, 21 | AV};
static const uint16_t PT_SYS[] = {O_KEYS | AV, O_DEV | AV, NAV(P_DEV), O_FAVRESET | AV};
static const uint16_t PT_DEV[] = {DV(0), DV(1), DV(2), DV(3), DV(4), DV(5), DV(6), DV(7), DV(8), DV(9), O_DEVRESET};
static const uint16_t PT_CHALL[] = {O_DELIV, O_TUTO, O_CRASHV, NAV(P_MODS)};
static const uint16_t PT_PROF[] = {O_STATS, O_ACH};
#define PLEN(t) ((uint8_t)(sizeof(t) / sizeof((t)[0])))
static const uint16_t *const PGT[NPAGE] = {0, PT_OPT, PT_VEH, PT_WORLD, PT_DRIVE, PT_DISP, PT_TOOLS, PT_SYS, PT_DEV, PT_CHALL, PT_PROF, 0};
static const uint8_t PGLEN[NPAGE] = {0, PLEN(PT_OPT), PLEN(PT_VEH), PLEN(PT_WORLD), PLEN(PT_DRIVE), PLEN(PT_DISP), PLEN(PT_TOOLS), PLEN(PT_SYS), PLEN(PT_DEV), PLEN(PT_CHALL), PLEN(PT_PROF), 0};
static int advMode, pgCur, pgDepth, pgInGame, pgRet, pgBase, pgSelSave, pgHold, pgMsgT; static const char *pgMsg;
static uint8_t pgStk[6], pgSelStk[6], fav[NFAV];
static int gameOnly(int id) { return id == 0 || id == 2 || id == 3 || id == 5 || id == 9 || id == 21 || id == 22 || id == 23 || id == 24 || id == 25 || id == 27 || id == 36 || (id >= MOD_POW0 && id < MOD_MODE0); }   // actions qui n'ont de sens qu'en jeu
static int is_fav(int id) { for (int i = 0; i < NFAV; i++) if (fav[i] == id) return 1; return 0; }
static void fav_default(void) { static const uint8_t D[NFAV] = {0, 1, 2, 3, 8, 9, 10, 15}; for (int i = 0; i < NFAV; i++) fav[i] = D[i]; }
static int fav_toggle(int id) {                                  // 1 = ajoute, 0 = retire, -1 = favoris pleins
  for (int i = 0; i < NFAV; i++) if (fav[i] == id) { for (int j = i; j < NFAV - 1; j++) fav[j] = fav[j + 1]; fav[NFAV - 1] = 255; return 0; }
  for (int i = 0; i < NFAV; i++) if (fav[i] == 255) { fav[i] = (uint8_t)id; return 1; }
  return -1;
}
static void fav_ensure(int id) { if (is_fav(id)) return; for (int i = NFAV - 1; i >= 0; i--) if (fav[i] != 0 && fav[i] != 1) { fav[i] = (uint8_t)id; return; } }   // le tutoriel a besoin de "Reparer" (0) et "Turbo" (1)
static int page_list(int pg, uint16_t *o);
static int page_count(int pg) { uint16_t t[24]; return page_list(pg, t); }
static int page_list(int pg, uint16_t *o) {                      // lignes visibles d'une page dans le contexte courant
  int c = 0;
  if (pg == P_FAV) { for (int i = 0; i < NFAV; i++) if (fav[i] != 255 && (pgInGame || !gameOnly(fav[i])) && (fav[i] < MOD_POW0 || modIdOk(fav[i]))) o[c++] = fav[i]; o[c++] = NAV(P_OPT); return c; }
  if (pg == P_MODS) { o[c++] = O_MODINFO; for (int i = 0; i < nModes; i++) o[c++] = (uint16_t)(MOD_MODE0 + i); if (pgInGame) for (int i = 0; i < nPow; i++) o[c++] = (uint16_t)(MOD_POW0 + i); o[c++] = O_MODRELOAD; return c; }
  const uint16_t *t = PGT[pg];
  for (int i = 0; i < PGLEN[pg]; i++) { int id = t[i] & 0x7FFF;
    if ((t[i] & AV) && !advMode) continue;
    if (gameOnly(id) && !pgInGame) continue;
    if (id == NAV(P_DEV) && !devOn) continue;
    if (id >= 100 && id < 128 && id != NAV(P_FAV) && id != NAV(P_DEV) && page_count(id - 100) == 0) continue;   // pas de sous-page vide
    o[c++] = (uint16_t)id; }
  return c;
}
// variables du mode developpeur: nom, min, max, pas, echelle d'affichage, decimales, unite (l'ordre suit DEVP dans sim.h)
typedef struct { const char *nm; float lo, hi, st, sc; uint8_t dec; const char *un; } DevM;
static const DevM DEVM[NDEV] = {
  {"Vitesse max", 3.f, 150.f, 1.f, 3.6f, 0, " km/h"}, {"Acceleration", 1.f, 100.f, 1.f, 1.f, 0, ""}, {"Adherence laterale", .02f, 2.f, .02f, 1.f, 2, ""},
  {"Gravite (g)", 0.f, 80.f, 1.f, 1.f, 1, ""}, {"Adherence globale", .05f, 3.f, .05f, 1.f, 2, ""}, {"Turbo: vitesse", 1.f, 5.f, .1f, 1.f, 1, " x"},
  {"Turbo: acceleration", 1.f, 10.f, .1f, 1.f, 1, " x"}, {"Freinage", 0.f, 10.f, .1f, 1.f, 1, " x"}, {"Braquage", .05f, 1.f, .01f, 1.f, 2, ""}, {"Frein moteur", 0.f, .2f, .002f, 1.f, 3, ""}};
static char *fmtf(char *p, float v, int dec) {
  if (v < 0.f) { *p++ = '-'; v = -v; }
  int mul = 1; for (int i = 0; i < dec; i++) mul *= 10;
  int q = (int)(v * (float)mul + .5f); p = num(p, q / mul);
  if (dec) { *p++ = '.'; int f = q % mul; for (int dv = mul / 10; dv >= 1; dv /= 10) *p++ = (char)('0' + f / dv % 10); *p = 0; }
  return p;
}
static void dev_edit(int i, int d, int mul) {
  if (!devOn) { pgMsg = "Active le mode developpeur"; pgMsgT = 40; return; }
  float v = *DEVP[i] + (float)(d * mul) * DEVM[i].st; if (v < DEVM[i].lo) v = DEVM[i].lo; if (v > DEVM[i].hi) v = DEVM[i].hi;
  devOv[i] = v; devSet |= (uint16_t)(1u << i); *DEVP[i] = v;
}
// ---- lecteur de scripts: un sous-ensemble de Python (appels de fonctions, variables, + - * / **, tuples, commentaires). Rien n'est execute: le script est seulement lu.
typedef struct { float f[4]; int n; const char *s; int sl; } MV;          // valeur: nombre / tuple (f, n) ou texte (s, sl)
typedef struct { char k[24]; MV v; } MKw;
static const char *mq, *mqe, *mqName; static int mqLine, mqNest, nmv;
static float mvv[16]; static char mvn[16][12];
static MV mpos[8]; static MKw mkw[24]; static int mnp, mnk;
static ModMap *curMap;
static const char *const DKEY[NDEV] = {"vitesse_max", "acceleration", "adherence_laterale", "gravite", "adherence", "turbo_vitesse", "turbo_acceleration", "freinage", "braquage", "frein_moteur"};
static void mq_err(const char *m) {
  if (modErr[0]) return;
  char *p = modErr; for (int i = 0; mqName[i] && i < 10; i++) *p++ = mqName[i];
  *p = 0; p = cat(p, " l."); p = num(p, mqLine); p = cat(p, ": "); cat(p, m);
}
static void mq_ws(void) {
  while (mq < mqe) {
    char c = *mq;
    if (c == ' ' || c == '\t' || c == '\r') mq++;
    else if (c == '\\' && mq + 1 < mqe && mq[1] == '\n') { mq += 2; mqLine++; }
    else if (c == '\n') { if (!mqNest) return; mq++; mqLine++; }
    else if (c == '#') { while (mq < mqe && *mq != '\n') mq++; }
    else return;
  }
}
static char mq_pk(void) { return mq < mqe ? *mq : 0; }
static int mq_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int mq_digit(char c) { return c >= '0' && c <= '9'; }
static int mq_id(char *o, int mx) {
  int i = 0;
  while (mq < mqe && (mq_alpha(*mq) || (i && mq_digit(*mq)))) { if (i < mx - 1) o[i++] = (*mq >= 'A' && *mq <= 'Z') ? (char)(*mq + 32) : *mq; mq++; }
  o[i] = 0; return i;
}
static int mq_eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void mq_zero(void *p, unsigned len) { char *c = (char *)p; for (unsigned i = 0; i < len; i++) c[i] = 0; }
static float mq_num(void) {
  float v = 0.f; while (mq < mqe && mq_digit(*mq)) v = v * 10.f + (float)(*mq++ - '0');
  if (mq_pk() == '.') { mq++; float s = .1f; while (mq < mqe && mq_digit(*mq)) { v += s * (float)(*mq++ - '0'); s *= .1f; } }
  if ((mq_pk() == 'e' || mq_pk() == 'E') && mq + 1 < mqe && (mq_digit(mq[1]) || ((mq[1] == '-' || mq[1] == '+') && mq + 2 < mqe && mq_digit(mq[2])))) {
    mq++; int sg = 1; if (*mq == '-') { sg = -1; mq++; } else if (*mq == '+') mq++;
    int e = 0; while (mq < mqe && mq_digit(*mq)) e = e * 10 + (*mq++ - '0');
    if (e > 30) e = 30;
    while (e--) v = sg > 0 ? v * 10.f : v * .1f;
  }
  return v;
}
static int mq_expr(MV *v);
static int mq_prim(MV *v) {
  mq_ws(); char c = mq_pk(); v->n = 1; v->s = 0; v->sl = 0; v->f[0] = v->f[1] = v->f[2] = v->f[3] = 0.f;
  if (mq_digit(c) || c == '.') { v->f[0] = mq_num(); return 1; }
  if (c == '"' || c == '\'') {
    mq++; v->s = mq; while (mq < mqe && *mq != c && *mq != '\n') mq++;
    if (mq_pk() != c) { mq_err("guillemet manque"); return 0; }
    v->sl = (int)(mq - v->s); mq++; v->n = 0; return 1;
  }
  if (c == '(' || c == '[') {
    char cl = c == '(' ? ')' : ']'; int k = 0, commas = 0; mq++; mqNest++;
    for (;;) {
      MV t; mq_ws(); if (mq_pk() == cl) { mq++; break; }
      if (!mq_expr(&t)) { mqNest--; return 0; }
      if (k < 4) v->f[k] = t.f[0];
      k++; mq_ws();
      if (mq_pk() == ',') { mq++; commas++; continue; }
      if (mq_pk() == cl) { mq++; break; }
      mqNest--; mq_err("virgule attendue"); return 0;
    }
    mqNest--; v->n = (commas || cl == ']' || k != 1) ? (k > 4 ? 4 : k) : 1; return 1;
  }
  if (mq_alpha(c)) {
    char id[12]; mq_id(id, 12);
    if (mq_eq(id, "true")) v->f[0] = 1.f; else if (mq_eq(id, "false") || mq_eq(id, "none")) v->f[0] = 0.f; else if (mq_eq(id, "pi")) v->f[0] = 3.14159265f;
    else { int i = 0; while (i < nmv && !mq_eq(mvn[i], id)) i++; if (i == nmv) { mq_err("nom inconnu"); return 0; } v->f[0] = mvv[i]; }
    return 1;
  }
  mq_err("valeur attendue"); return 0;
}
static int mq_un(MV *v) {
  mq_ws();
  if (mq_pk() == '-' || mq_pk() == '+') { char c = *mq++; if (!mq_un(v)) return 0; if (c == '-') for (int i = 0; i < 4; i++) v->f[i] = -v->f[i]; return 1; }
  return mq_prim(v);
}
static int mq_term(MV *v) {
  if (!mq_un(v)) return 0;
  for (;;) {
    mq_ws(); char c = mq_pk(); MV t;
    if (c == '*' && mq + 1 < mqe && mq[1] == '*') {                              // puissance entiere: x ** 2
      mq += 2; if (!mq_un(&t)) return 0;
      int e = (int)t.f[0]; if (e < 0) e = 0; if (e > 8) e = 8;
      float b = v->f[0], r = 1.f; while (e--) r *= b;
      v->f[0] = r; v->s = 0; v->n = 1; continue;
    }
    if (c != '*' && c != '/') return 1;
    mq++; if (!mq_un(&t)) return 0;
    if (c == '*') v->f[0] *= t.f[0]; else { if (t.f[0] == 0.f) { mq_err("division par 0"); return 0; } v->f[0] /= t.f[0]; }
    v->s = 0; v->n = 1;
  }
}
static int mq_expr(MV *v) {
  if (!mq_term(v)) return 0;
  for (;;) {
    mq_ws(); char c = mq_pk(); if (c != '+' && c != '-') return 1;
    mq++; MV t; if (!mq_term(&t)) return 0;
    v->f[0] += c == '+' ? t.f[0] : -t.f[0]; v->s = 0; v->n = 1;
  }
}
// arguments de l'appel en cours: position (0 = premier) ou mot-cle
static const MV *marg(const char *k, int pos) { for (int i = 0; i < mnk; i++) if (mq_eq(mkw[i].k, k)) return &mkw[i].v; return (pos >= 0 && pos < mnp) ? &mpos[pos] : 0; }
static float marf(const char *k, int pos, float d, float lo, float hi) { const MV *v = marg(k, pos); float x = (v && !v->s) ? v->f[0] : d; return x < lo ? lo : (x > hi ? hi : x); }
static void mcopy(char *d, int mx, const MV *v, const char *def) {
  int i = 0;
  if (v && v->s) { for (; i < v->sl && i < mx - 1; i++) d[i] = v->s[i]; } else if (def) { for (; def[i] && i < mx - 1; i++) d[i] = def[i]; }
  d[i] = 0;
}
static void mcol(const MV *v, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (!v || v->s) return;
  float c[3]; for (int i = 0; i < 3; i++) { float x = v->n >= 3 ? v->f[i] : v->f[0]; c[i] = x < 0.f ? 0.f : (x > 255.f ? 255.f : x); }
  *r = (uint8_t)c[0]; *g = (uint8_t)c[1]; *b = (uint8_t)c[2];
}
static int mq_cmpn(const char *s, int sl, const char *nm) {     // 2: identique, 1: debut du nom, 0: different (sans tenir compte des majuscules)
  if (sl <= 0) return 0;
  int i = 0;
  for (; i < sl; i++) { char a = s[i], b = nm[i]; if (!b) return 0; if (a >= 'A' && a <= 'Z') a = (char)(a + 32); if (b >= 'A' && b <= 'Z') b = (char)(b + 32); if (a != b) return 0; }
  return nm[i] ? 1 : 2;
}
#define MQ_FIND(v, cnt, NAME, out) do { int bs_ = 0; (out) = -1; if ((v)->s) { for (int q_ = 0; q_ < (cnt); q_++) { int m_ = mq_cmpn((v)->s, (v)->sl, NAME); if (m_ > bs_) { bs_ = m_; (out) = q_; } } } else { int q_ = (int)(v)->f[0]; if (q_ >= 0 && q_ < (cnt)) (out) = q_; } } while (0)
static int md_findmap(const char *s, int sl) { int best = -1, bs = 0; for (int u = 0; u < map_total(); u++) { int m = mq_cmpn(s, sl, u < NMAPB ? MAPN[u] : cmaps[u - NMAPB].nm); if (m > bs) { bs = m; best = u; } } return best; }
static int md_findveh(const char *s, int sl) { int best = -1, bs = 0; for (int u = 0; u < nVeh; u++) { int m = mq_cmpn(s, sl, VEH[u].nm); if (m > bs) { bs = m; best = u; } } return best; }
static int md_len(const char *s) { int i = 0; while (s[i]) i++; return i; }
static int mq_tok(const char *list, const char *k) {
  const char *p = list;
  while (*p) { const char *a = p, *b = k; while (*a && *a != ' ' && *a == *b) { a++; b++; } if ((*a == ' ' || !*a) && !*b) return 1; while (*p && *p != ' ') p++; while (*p == ' ') p++; }
  return 0;
}
static void mq_known(const char *ok, int dev) {                 // detecte les fautes de frappe dans les noms d'options
  for (int i = 0; i < mnk; i++) {
    int good = mq_tok(ok, mkw[i].k);
    for (int d = 0; dev && d < NDEV && !good; d++) good = mq_eq(DKEY[d], mkw[i].k);
    if (!good) { char m[40], *p = m; p = cat(p, "option "); cat(p, mkw[i].k); mq_err(m); return; }
  }
}
static void mq_dev(uint16_t *set, float *val) {                 // options de reglage (vitesse_max, gravite...): valeurs bornees comme dans la page Variables dev
  for (int i = 0; i < NDEV; i++) {
    const MV *v = marg(DKEY[i], -1); if (!v || v->s) continue;
    float x = v->f[0] / DEVM[i].sc; if (x < DEVM[i].lo) x = DEVM[i].lo; if (x > DEVM[i].hi) x = DEVM[i].hi;
    *set |= (uint16_t)(1u << i); val[i] = x;
  }
}
static void mq_call(const char *f) {
  if (mq_eq(f, "vehicule") || mq_eq(f, "vehicle")) {
    if (nVeh >= NV) { mq_err("max 8 vehicules"); return; }
    mq_known("nom largeur hauteur longueur masse puissance grip couleur propulseur", 0);
    int k = nVeh - NVB; Veh *V = &VEH[nVeh];
    mcopy(modVName[k], 16, marg("nom", 0), "Mod"); V->nm = modVName[k];
    V->sx = marf("largeur", 1, 1.f, .3f, 3.f); V->sy = marf("hauteur", 2, 1.f, .3f, 3.f); V->sz = marf("longueur", 3, 1.f, .3f, 3.f);
    V->im = 1.f / marf("masse", 4, 1.f, .3f, 4.f); V->pw = marf("puissance", 5, 1.f, .3f, 3.f); V->gr = marf("grip", 6, 1.f, .3f, 2.f);
    V->r = V->g = V->b = 150; mcol(marg("couleur", 7), &V->r, &V->g, &V->b);
    V->th = marf("propulseur", -1, 0.f, 0.f, 1.f) > .5f ? 1.f : 0.f; nVeh++;
  } else if (mq_eq(f, "carte") || mq_eq(f, "map")) {
    if (nCm >= MOD_MAXC) { mq_err("max 4 cartes"); return; }
    mq_known("nom sol depart collines gravite adherence", 0);
    ModMap *M = &cmaps[nCm++]; mq_zero(M, sizeof *M); curMap = M; M->hL = 30.f;
    mcopy(M->nm, 16, marg("nom", 0), "Carte"); M->grav = marf("gravite", -1, 0.f, 0.f, 80.f); M->grip = marf("adherence", -1, 0.f, 0.f, 3.f);
    const MV *s = marg("sol", -1); if (s && !s->s) { M->col = 1; mcol(s, &M->cr, &M->cg, &M->cb); }
    s = marg("depart", -1); if (s && !s->s) { M->sx = s->f[0]; M->sz = s->f[1]; }
    s = marg("collines", -1); if (s && !s->s) { M->hA = s->f[0] < 0.f ? 0.f : (s->f[0] > 8.f ? 8.f : s->f[0]); if (s->n > 1 && s->f[1] >= 3.f) M->hL = s->f[1] > 200.f ? 200.f : s->f[1]; }
  } else if (mq_eq(f, "rampe") || mq_eq(f, "ramp")) {
    if (!curMap) { mq_err("rampe sans carte"); return; }
    if (curMap->nr >= MOD_MAXR) { mq_err("max 16 rampes"); return; }
    mq_known("x largeur z0 z1 h0 h1", 0);
    Rmp *R = &curMap->r[curMap->nr++];
    R->cx = marf("x", 0, 0.f, -500.f, 500.f); R->hw = marf("largeur", 1, 8.f, .5f, 100.f) * .5f;
    R->z0 = marf("z0", 2, 0.f, -1000.f, 1000.f); R->z1 = marf("z1", 3, 20.f, -1000.f, 1000.f);
    R->h0 = marf("h0", 4, 0.f, 0.f, 40.f); R->h1 = marf("h1", 5, 2.f, 0.f, 40.f);
    if (R->z1 <= R->z0) R->z1 = R->z0 + .5f;
  } else if (mq_eq(f, "colline") || mq_eq(f, "hills")) {
    if (!curMap) { mq_err("colline sans carte"); return; }
    mq_known("amplitude longueur", 0);
    curMap->hA = marf("amplitude", 0, 0.f, 0.f, 8.f); curMap->hL = marf("longueur", 1, 30.f, 3.f, 200.f);
  } else if (mq_eq(f, "boucle") || mq_eq(f, "loop")) {
    if (!curMap) { mq_err("boucle sans carte"); return; }
    if (curMap->nl >= NLOOPMAX) { mq_err("max 4 boucles"); return; }
    mq_known("x z rayon", 0);
    float *L = curMap->lp[curMap->nl++]; L[0] = marf("x", 0, 0.f, -500.f, 500.f); L[1] = marf("z", 1, 60.f, -1000.f, 1000.f); L[2] = marf("rayon", 2, 6.5f, 3.f, 15.f);
  } else if (mq_eq(f, "objet") || mq_eq(f, "object")) {
    if (!curMap) { mq_err("objet sans carte"); return; }
    if (curMap->no >= 12) { mq_err("max 12 objets"); return; }
    mq_known("type x z", 0);
    const MV *v = marg("type", 0); int t = -1; if (v) MQ_FIND(v, 7, OBJ[q_].nm, t);
    if (t < 0) { mq_err("objet inconnu"); return; }
    curMap->ok[curMap->no] = (uint8_t)t; curMap->ox[curMap->no] = marf("x", 1, 0.f, -1000.f, 1000.f); curMap->oz[curMap->no] = marf("z", 2, 20.f, -1000.f, 1000.f); curMap->no++;
  } else if (mq_eq(f, "depart") || mq_eq(f, "start")) {
    if (!curMap) { mq_err("depart sans carte"); return; }
    mq_known("x z", 0);
    curMap->sx = marf("x", 0, 0.f, -1000.f, 1000.f); curMap->sz = marf("z", 1, 0.f, -1000.f, 1000.f);
  } else if (mq_eq(f, "pouvoir") || mq_eq(f, "power")) {
    if (nPow >= MOD_MAXP) { mq_err("max 8 pouvoirs"); return; }
    mq_known("nom saut boost stop reparer essence tp turbo duree", 1);
    ModPow *P = &mpows[nPow++]; mq_zero(P, sizeof *P);
    mcopy(P->nm, 16, marg("nom", 0), "Pouvoir");
    P->jump = marf("saut", -1, 0.f, 0.f, 40.f); P->boost = marf("boost", -1, 0.f, 0.f, 60.f); P->dur = marf("duree", -1, 0.f, 0.f, 120.f);
    P->stop = marf("stop", -1, 0.f, 0.f, 1.f) > .5f; P->rep = marf("reparer", -1, 0.f, 0.f, 1.f) > .5f; P->fuel = marf("essence", -1, 0.f, 0.f, 1.f) > .5f; P->turbo = marf("turbo", -1, 0.f, 0.f, 1.f) > .5f;
    const MV *t = marg("tp", -1); if (t && !t->s && t->n >= 2) { P->hasTp = 1; P->tp[0] = t->f[0]; P->tp[1] = t->f[1]; }
    mq_dev(&P->set, P->val);
    if ((P->set || P->turbo) && P->dur <= 0.f) P->dur = 10.f;          // un effet de reglage dure 10 s par defaut
  } else if (mq_eq(f, "mode")) {
    if (nModes >= MOD_MAXM) { mq_err("max 8 modes"); return; }
    mq_known("nom carte vehicule moteur roues suspension chassis remorque meteo trafic", 1);
    ModMode *M = &mmodes[nModes++]; mq_zero(M, sizeof *M);
    mcopy(M->nm, 16, marg("nom", 0), "Mode"); M->mapI = M->vehI = M->eng = M->whl = M->sus = M->cha = M->trl = -1;
    const MV *v = marg("carte", -1); if (v) { if (v->s) { mcopy(M->mapS, 16, v, 0); M->mapI = -2; } else M->mapI = (int8_t)v->f[0]; }
    v = marg("vehicule", -1); if (v) { if (v->s) { mcopy(M->vehS, 16, v, 0); M->vehI = -2; } else M->vehI = (int8_t)v->f[0]; }
    int r;
    v = marg("moteur", -1); if (v) { MQ_FIND(v, 4, ENG[q_].nm, r); M->eng = (int8_t)r; if (r < 0) mq_err("moteur inconnu"); }
    v = marg("roues", -1); if (v) { MQ_FIND(v, 4, WHL[q_].nm, r); M->whl = (int8_t)r; if (r < 0) mq_err("roues inconnues"); }
    v = marg("suspension", -1); if (v) { MQ_FIND(v, 4, SUS[q_].nm, r); M->sus = (int8_t)r; if (r < 0) mq_err("suspension inconnue"); }
    v = marg("chassis", -1); if (v) { MQ_FIND(v, 3, CHA[q_].nm, r); M->cha = (int8_t)r; if (r < 0) mq_err("chassis inconnu"); }
    v = marg("remorque", -1); if (v) { MQ_FIND(v, NTRL, TRL[q_].nm, r); M->trl = (int8_t)r; if (r < 0) mq_err("remorque inconnue"); }
    M->wea = (int8_t)marf("meteo", -1, -1.f, -1.f, 3.f); M->traf = (int16_t)marf("trafic", -1, -1.f, -1.f, 64.f);
    mq_dev(&M->set, M->val);
  } else mq_err("fonction inconnue");
}
static void mq_skip(void) { while (mq < mqe && *mq != '\n') mq++; mqNest = 0; }
static void mq_stmt(void) {
  mqNest = 0; mq_ws(); if (mq >= mqe) return;
  if (*mq == '\n') { mq++; mqLine++; return; }
  char id[16];
  if (!mq_id(id, 16)) { mq_err("instruction ?"); mq_skip(); return; }
  mq_ws();
  if (mq_pk() == '=' && !(mq + 1 < mqe && mq[1] == '=')) {                                  // nom = valeur
    MV v; mq++;
    if (!mq_expr(&v)) { mq_skip(); return; }
    if (v.s) { mq_err("nombre attendu"); mq_skip(); return; }
    int i = 0; while (i < nmv && !mq_eq(mvn[i], id)) i++;
    if (i == nmv) { if (nmv >= 16) { mq_err("max 16 variables"); mq_skip(); return; } nmv++; for (int j = 0; j < 11 && id[j]; j++) { mvn[i][j] = id[j]; mvn[i][j + 1] = 0; } }
    mvv[i] = v.f[0];
  } else if (mq_pk() == '(') {                                                              // fonction(arguments)
    mq++; mqNest++; mnp = 0; mnk = 0;
    for (;;) {
      mq_ws(); if (mq_pk() == ')') { mq++; break; }
      const char *sv = mq; int sl = mqLine, isk = 0; char kn[24];
      if (mq_alpha(mq_pk())) { mq_id(kn, 24); mq_ws(); if (mq_pk() == '=' && !(mq + 1 < mqe && mq[1] == '=')) { mq++; isk = 1; } else { mq = sv; mqLine = sl; } }
      MV v; if (!mq_expr(&v)) { mq_skip(); return; }
      if (isk) { if (mnk >= 24) { mq_err("trop d'arguments"); mq_skip(); return; } int j = 0; for (; kn[j]; j++) mkw[mnk].k[j] = kn[j]; mkw[mnk].k[j] = 0; mkw[mnk].v = v; mnk++; }
      else { if (mnp >= 8) { mq_err("trop d'arguments"); mq_skip(); return; } mpos[mnp++] = v; }
      mq_ws();
      if (mq_pk() == ',') { mq++; continue; }
      if (mq_pk() == ')') { mq++; break; }
      mq_err("virgule attendue"); mq_skip(); return;
    }
    mqNest--; mq_call(id);
  } else if (!(mq_eq(id, "import") || mq_eq(id, "from"))) mq_err("instruction ?");        // import / from: lignes ignorees
  mq_skip();
}
static void mods_parse(const char *name, const char *src, int len) {
  mq = src; mqe = src + len; mqName = name; mqLine = 1; mqNest = 0; nmv = 0; curMap = 0;
  while (mq < mqe) mq_stmt();
}
static int mods_marked(const char *s, int len) {                // le script doit contenir "numbeam" dans ses premiers caracteres (ex: premiere ligne "# numbeam")
  static const char M[] = "numbeam"; if (len > 160) len = 160;
  for (int i = 0; i + 7 <= len; i++) { int j = 0; while (j < 7 && ((s[i + j] >= 'A' && s[i + j] <= 'Z') ? s[i + j] + 32 : s[i + j]) == M[j]) j++; if (j == 7) return 1; }
  return 0;
}
static void mods_load(void) {                                   // lit tous les scripts .py de la calculatrice (appele au demarrage et par "Recharger")
  nCm = nPow = nModes = nModFiles = 0; nVeh = NVB; modErr[0] = 0; curMap = 0;
  ef_fs_t fs;
  if (ef_open(&fs)) {
    int end = ef_end(&fs);
    for (uint32_t p = 0; end >= 0 && (int)p < end; p += ef_rd16(fs.buf + p)) {
      uint16_t sz = ef_rd16(fs.buf + p); const char *name = (const char *)fs.buf + p + 2; int nl = (int)ef_strlen(name);
      if (nl < 4 || sz < 3 + nl) continue;
      if (name[nl - 3] != '.' || (name[nl - 2] | 32) != 'p' || (name[nl - 1] | 32) != 'y') continue;
      const char *body = name + nl + 1; int len = (int)sz - 2 - (nl + 1);
      if (len > 0 && (uint8_t)body[0] < 32 && body[0] != '\n' && body[0] != '\r' && body[0] != '\t') { body++; len--; }   // octet d'etat de l'editeur Python
      int l2 = 0; while (l2 < len && body[l2]) l2++;
      if (!mods_marked(body, l2)) continue;
      nModFiles++; mods_parse(name, body, l2);
    }
  }
  if (cv >= nVeh) cv = 0;
  if (cmap >= 0) { if (cmap < nCm) cmap_use(cmap); else cmap_use(-1); }
}
// ---- pouvoirs et modes
static void power_end(void) {
  for (int i = 0; i < NDEV; i++) if ((pwSet >> i) & 1) *DEVP[i] = dev_def(i);
  pwSet = 0; powT = 0.f; powTurbo = 0; powAct = -1; dev_apply();
}
static void power_use(int i) {
  const ModPow *P = &mpows[i]; float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); fxSkip = 3;
  int cnt = ntr >= 0 ? ntr + ntrN : NC;
  if (P->rep) repair(cx, cz, hx, hz);
  if (P->stop) { for (int j = 0; j < cnt; j++) n[j].vx = n[j].vy = n[j].vz = 0.f; bHn = 0; }
  if (P->jump > 0.f) for (int j = 0; j < cnt; j++) n[j].vy += P->jump;
  if (P->boost > 0.f) for (int j = 0; j < cnt; j++) { n[j].vx += hx * P->boost; n[j].vz += hz * P->boost; }
  if (P->fuel) fuel = 1.f;
  if (P->hasTp) { float dy = gh(P->tp[0], P->tp[1]) - gh(cx, cz) + .5f; for (int j = 0; j < cnt; j++) { n[j].x += P->tp[0] - cx; n[j].z += P->tp[1] - cz; n[j].y += dy; } bHn = 0; }
  if (P->set || P->turbo) { power_end(); pwSet = P->set; for (int j = 0; j < NDEV; j++) pwVal[j] = P->val[j]; powT = P->dur; powTurbo = P->turbo; powAct = i; dev_apply(); }
}
static void mode_clear(void) {
  power_end();
  for (int i = 0; i < NDEV; i++) if ((mdSet >> i) & 1) *DEVP[i] = dev_def(i);
  mdSet = 0; modeAct = -1; dev_apply();
}
static void mode_apply(int i) {                                 // choisit carte, vehicule, equipement et reglages du mode (le depart de la partie est fait par l'appelant)
  const ModMode *M = &mmodes[i]; mode_clear();
  if (M->mapI != -1) { int u = M->mapI == -2 ? md_findmap(M->mapS, md_len(M->mapS)) : M->mapI; if (u >= 0 && u < map_total()) map_set(u); }
  if (M->vehI != -1) { int v = M->vehI == -2 ? md_findveh(M->vehS, md_len(M->vehS)) : M->vehI; if (v >= 0 && v < nVeh) { cv = v; if (cv == 4) gen_car(genSeed); } }
  if (M->eng >= 0) ce = M->eng; if (M->whl >= 0) cw = M->whl; if (M->sus >= 0) cs = M->sus; if (M->cha >= 0) cc = M->cha; if (M->trl >= 0) trl = M->trl;
  if (M->wea >= 0) weatherMode = M->wea;
  if (M->traf >= 0) { trafficCount = M->traf; trafficEnabled = M->traf > 0; }
  mdSet = M->set; for (int j = 0; j < NDEV; j++) mdVal[j] = M->val[j];
  modeAct = i; dev_apply();
}
static void mod_text(int id, char *s) {
  char *p = s;
  if (id < MOD_MODE0) { int i = id - MOD_POW0; p = cat(p, mpows[i].nm); if (mpows[i].dur > 0.f) { p = cat(p, " ("); p = num(p, (int)(mpows[i].dur + .5f)); p = cat(p, "s)"); } if (powAct == i && powT > 0.f) cat(p, " [actif]"); }
  else { int i = id - MOD_MODE0; p = cat(p, "> "); p = cat(p, mmodes[i].nm); if (modeAct == i) cat(p, "  [actif]"); }
}
// texte d'une ligne (nom + valeur)
static void item_text(int id, char *s) {
  static const char *QN[3] = {"Basse", "Normale", "Haute"}, *FN[3] = {"Etroit", "Normal", "Large"}, *VN[3] = {"Courte", "Normale", "Longue"};
  char *p = s;
  if (id < QUICK_COUNT) { qdesc(id, s); return; }
  if (id >= MOD_POW0) { mod_text(id, s); return; }
  if (id >= 128) { int i = id - 128; if (i >= NDEV) return; p = cat(p, DEVM[i].nm); p = cat(p, ": "); p = fmtf(p, *DEVP[i] * DEVM[i].sc, DEVM[i].dec); p = cat(p, DEVM[i].un); if ((devSet >> i) & 1) cat(p, " (dev)"); return; }
  if (id >= 100) { p = cat(p, PGL[id - 100]); cat(p, "  >"); return; }
  switch (id) {
    case O_QUAL: p = cat(p, "Qualite: "); cat(p, QN[qual]); break;
    case O_STEER: p = cat(p, "Direction: "); num(p, steerL); break;
    case O_CAM: p = cat(p, "Camera: "); num(p, camL); break;
    case O_FOV: p = cat(p, "Champ de vision: "); cat(p, FN[fovI]); break;
    case O_VIEW: p = cat(p, "Distance de vue: "); cat(p, VN[vdI]); break;
    case O_SKID: p = cat(p, "Traces de pneus: "); cat(p, optSkid ? "Oui" : "Non"); break;
    case O_PART: p = cat(p, "Particules et fumee: "); cat(p, optPart ? "Oui" : "Non"); break;
    case O_OBJ: p = cat(p, "Objets (barils...): "); cat(p, optObj ? "Oui" : "Non"); break;
    case O_RAMP: p = cat(p, "Rampes/boucles/ralentis: "); cat(p, optRamp ? "Oui" : "Non"); break;
    case O_PUNCT: p = cat(p, "Crevaisons: "); cat(p, optPunct ? "Oui" : "Non"); break;
    case O_KEYS: cat(p, "Test des touches  >"); break;
    case O_CTRL: cat(p, "Commandes  >"); break;
    case O_SOLID: p = cat(p, "Solidite: "); cat(p, SOLN[solid]); break;
    case O_MODE: cat(p, advMode ? "Mode: Avance (tout)" : "Mode: Simple (essentiel)"); break;
    case O_DEV: cat(p, devOn ? "Mode developpeur: oui" : "Mode developpeur: non"); break;
    case O_CRASHV: p = cat(p, "Vitesse crash-test: "); p = num(p, CRASHV[crashI]); cat(p, " km/h"); break;
    case O_FAVRESET: cat(p, "Favoris: remettre par defaut (OK)"); break;
    case O_DEVRESET: cat(p, "Variables dev: tout reinitialiser (OK)"); break;
    case O_TUTO: cat(p, "Tutoriel  >"); break;
    case O_DELIV: cat(p, "Livraisons  >"); break;
    case O_STATS: cat(p, "Statistiques  >"); break;
    case O_MODINFO: if (modErr[0]) { p = cat(p, "! "); cat(p, modErr); } else { p = cat(p, "Scripts: "); p = num(p, nModFiles); p = cat(p, "  V"); p = num(p, nVeh - NVB); p = cat(p, " C"); p = num(p, nCm); p = cat(p, " P"); p = num(p, nPow); p = cat(p, " M"); num(p, nModes); } break;
    case O_MODRELOAD: cat(p, "Recharger les scripts (OK)"); break;
    case O_ACH: p = cat(p, "Succes: "); p = num(p, ach_count()); p = cat(p, "/"); p = num(p, NACH); cat(p, "  >"); break;
    default: break;
  }
}
static void draw_page(const uint16_t *it, int cnt, int sel) {
  char s[64], *p = s;
  if (pgRet == S_GAME) eadk_display_push_rect_uniform((eadk_rect_t){20, 0, 280, 240}, BGC); else clear();
  p = cat(p, PGN[pgCur]); if (pgCur != P_FAV && pgCur != P_CHALL && pgCur != P_PROF && pgCur != P_DEV && pgCur != P_MODS) cat(p, advMode ? "   [Avance]" : "   [Simple]");
  txt(s, 30, 3, 0, C(255, 210, 80), BGC);
  int top = sel - 5; if (top > cnt - 11) top = cnt - 11; if (top < 0) top = 0;
  for (int line = 0; line < 11 && top + line < cnt; line++) { int i = top + line, id = it[i]; p = s;
    if (id < 100 || id >= 128) { if (is_fav(id)) p = cat(p, "* "); }
    item_text(id, p);
    row(s, 30, 18 + line * 18, 260, 17, i == sel); }
  txt(pgMsgT > 0 ? pgMsg : (pgCur == P_DEV ? "Gauche/Droite: valeur (tenir)  OK: defaut" : "OK/Gauche/Droite: changer  Shift: favori"), 30, 224, 0, pgMsgT > 0 ? C(255, 150, 90) : C(150, 160, 190), BGC);
}
// ---- config persistante: favoris, mode Simple/Avance, mode developpeur et ses valeurs (fichier separe des records)
#define CFG_FILE "numbeam.cfg"
#define CFG_MAGIC 0x4E424331u
#define CFG_N (5 + NDEV + 1)
static void cfg_save(void) {
  uint32_t w[CFG_N]; for (int i = 0; i < CFG_N; i++) w[i] = 0;
  w[0] = CFG_MAGIC; for (int i = 0; i < NFAV; i++) w[1 + i / 4] |= (uint32_t)fav[i] << (8 * (i % 4));
  w[3] = (uint32_t)(advMode | (devOn << 1)); w[4] = devSet;
  for (int i = 0; i < NDEV; i++) { union { float f; uint32_t u; } c; c.f = devOv[i]; w[5 + i] = c.u; }
  w[CFG_N - 1] = rec_sum(w, CFG_N - 1);
  ef_write(CFG_FILE, w, sizeof(w));
}
static void cfg_load(void) {
  fav_default(); uint32_t n = 0; const uint8_t *src = ef_read(CFG_FILE, &n); uint32_t w[CFG_N];
  if (!src || n != sizeof(w)) return;
  for (uint32_t i = 0; i < n; i++) ((uint8_t *)w)[i] = src[i];                  // le fichier n'est pas aligne: copie octet par octet
  if (w[0] != CFG_MAGIC || w[CFG_N - 1] != rec_sum(w, CFG_N - 1)) return;
  for (int i = 0; i < NFAV; i++) { int id = (int)((w[1 + i / 4] >> (8 * (i % 4))) & 255u); fav[i] = (uint8_t)((id < QUICK_COUNT || (id >= 40 && id <= O_ACH) || (id >= 128 && id < 128 + NDEV) || (id >= MOD_POW0 && id < MOD_MODE0 + MOD_MAXM)) ? id : 255); }
  advMode = (int)(w[3] & 1u); devOn = (int)((w[3] >> 1) & 1u); devSet = (uint16_t)(w[4] & ((1u << NDEV) - 1u));
  for (int i = 0; i < NDEV; i++) { union { float f; uint32_t u; } c; c.u = w[5 + i]; devOv[i] = c.f; if (!(devOv[i] >= DEVM[i].lo && devOv[i] <= DEVM[i].hi)) devSet &= (uint16_t)~(1u << i); }
}
static void draw_tutorial(void) {
  char text[72]; tut_text(text); eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14));
  txt(text, 4, 225, 0, tutOk > 0 ? C(120, 255, 140) : C(255, 210, 80), C(10, 10, 14));
}
static void stats_update(float dt, float sp) {        // once per frame while driving: distance, top speed, crashes, drift time
  if (dt <= 0.f) return; if (dt > .1f) dt = .1f;
  stDist += sp * dt; stZen += sp * dt; { float k = sp * 3.6f; if (k > stTop && k < 700.f) stTop = k; }
  if (stCool > 0.f) stCool -= dt;
  if (dmg < stLastDmg || burning || exploded) stLastDmg = dmg;           // repaired / new game / fire damage do not count
  if (dmg - stLastDmg >= 1.5f) { stZen = 0.f; if (stCool <= 0.f) stCrash++; stCool = 1.f; stLastDmg = dmg; }   // a crash = a jump of damage (one per second at most)
  float hx, hz; heading(&hx, &hz); int dc = 0;
  for (int w = 0; w < 4; w++) { Node *p = &n[14 + w]; if (!p->gnd || wAtt[w] <= 0) continue;
    float spd = fsqrt(p->vx * p->vx + p->vz * p->vz), lat = p->vx * hz - p->vz * hx; if (lat < 0) lat = -lat;
    if (lat > 2.6f && spd > 6.f) dc++; }
  if (dc >= 2) { stDrift += dt; stDriftCur += dt; if (stDriftCur > stDriftBest) stDriftBest = stDriftCur; } else stDriftCur = 0.f;
}
static char *fmt_time(char *p, float t) { int sec = (int)t; p = num(p, sec / 60); p = cat(p, ":"); if (sec % 60 < 10) p = cat(p, "0"); return num(p, sec % 60); }
static void draw_stats(void) {
  clear(); txt("STATISTIQUES", 95, 6, 1, ACC, BGC);
  char s[48], *p;
  p = s; p = cat(p, "Distance: "); p = dec1(p, stDist / 1000.f); cat(p, " km"); row(s, 40, 36, 240, 22, 0);
  p = s; p = cat(p, "Crashs: "); num(p, stCrash); row(s, 40, 62, 240, 22, 0);
  p = s; p = cat(p, "Vitesse max: "); p = num(p, (int)(stTop + .5f)); cat(p, " km/h"); row(s, 40, 88, 240, 22, 0);
  p = s; p = cat(p, "Temps de derive: "); fmt_time(p, stDrift); row(s, 40, 114, 240, 22, 0);
  p = s; p = cat(p, "Plus longue derive: "); p = dec1(p, stDriftBest); cat(p, " s"); row(s, 40, 140, 240, 22, 0);
  p = s; p = cat(p, "Livraisons: "); num(p, stDeliv); row(s, 40, 166, 240, 22, 0);
  p = s; p = cat(p, "Marche arriere max: "); p = num(p, (int)(stRev + .5f)); cat(p, " km/h"); row(s, 40, 192, 240, 22, 0);
  txt(stConfirm ? "OK: CONFIRMER la remise a zero" : "OK: remise a zero    Retour: menu", 40, 214, 0, stConfirm ? C(255, 110, 90) : C(150, 160, 190), BGC);
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
  if (optBody) { int fis = 0, bri = 0, mp = 0, mh = 0; for (int w = 0; w < 4; w++) { fis += glsS[w] == 1; bri += glsS[w] == 2; } for (int q = 0; q < 2; q++) { mp += mirS[q] == 0; mh += mirS[q] == 2; }
    char t[28], *q = t; q = cat(q, "Vitres: "); q = num(q, fis); q = cat(q, " fis. "); q = num(q, bri); cat(q, " bris."); txt(t, 190, 164, 0, bri ? C(255, 120, 100) : (fis ? C(255, 210, 80) : C(120, 220, 120)), BGC);
    q = t; q = cat(q, "Retros: "); q = num(q, mp); q = cat(q, " perdu "); q = num(q, mh); cat(q, " pend."); txt(t, 190, 178, 0, mp ? C(255, 120, 100) : (mh ? C(255, 210, 80) : C(120, 220, 120)), BGC); }
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
  char s[144], *p = s; p = num(p, kmh); p = cat(p, "km/h "); if (gear < 0) p = cat(p, "R"); else if (gear == 0) p = cat(p, "N"); else { p = cat(p, "G"); p = num(p, gear); if (manualGear) p = cat(p, "M"); }
  if (tsi != 4) { p = cat(p, " TEMPS "); p = cat(p, TSN[tsi]); }
  p = cat(p, " Deg "); p = num(p, (int)dmg); p = cat(p, "%"); if (turbo) p = cat(p, VEH[cv].th > 0.f ? " PROPULSEURS" : " TURBO");
  if (exploded) p = cat(p, " BOOM!"); else if (burning) p = cat(p, " FEU!"); else if (tankHp < .6f && fuel > .01f) p = cat(p, " FUITE");
  { int c = 0; for (int w = 0; w < 4; w++) c += tpLeak[w]; if (c) p = cat(p, " CREV"); }
  if (mapId == 3 && optEvents && nebx > 0) { p = cat(p, " Ev"); if (evCnt[0]) { p = cat(p, " A"); p = num(p, evCnt[0]); } if (evCnt[1]) { p = cat(p, " T"); p = num(p, evCnt[1]); } if (evCnt[2]) p = cat(p, " C"); }
  if (stRefuel) { p = cat(p, " POMPE "); p = num(p, (int)(fuel * 100.f)); p = cat(p, "%"); } else if (stInZone && fuel < .98f) p = cat(p, " STATION: ARRETE-TOI");
  else if (mapId == 3 && fuel < .3f && !burning) { p = cat(p, " STN "); p = num(p, (int)station_dist((n[4].x + n[5].x) * .5f, (n[4].z + n[5].z) * .5f)); p = cat(p, "m"); }
  if (honkMsgT > 0.f) p = cat(p, " KLAXON!"); if (stallT > 0.f) p = cat(p, " CALE"); if (grindT > 0.f) p = cat(p, " CRAC"); if (hbrake > .3f) p = cat(p, " FREIN");
  if (engTemp > 1.f && !burning) p = cat(p, " TEMP!"); if (fuel < .12f && !burning) p = cat(p, " ESS");
  if (mapId == 0) { p = cat(p, " T"); p = num(p, lap); p = cat(p, " "); p = num(p, ms / 60000); p = cat(p, ":"); int sc = ms / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); p = cat(p, "."); p = num(p, ms / 100 % 10);
    if (best) { p = cat(p, " B"); p = num(p, best / 60000); p = cat(p, ":"); sc = best / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); } }
  else if (mapId == 2) { p = cat(p, " Test "); p = num(p, CRASHV[crashI]); if (ct == 2) { p = cat(p, " SCORE "); p = num(p, score); } else if (ct == 1) p = cat(p, " IMPACT!"); }
  eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14)); txt(s, 4, 225, 0, 0xFFFF, C(10, 10, 14));
}
int main(void) {
  np_app_begin();                                       // Home devient une touche normale (sinon Epsilon quitte l'app et la calculatrice reset)
  int st = S_MENU, sel = 0, redraw = 1, gsel = 0, gview = 0, cap = 0, fr = 0, lap = 1, best = 0, cp = 0;
  float acc = 0, stw = 0, pz = 0, gorb = .6f, gelev = 3.6f, gzoom = 7.5f, shx = 0, shz = 1, yawO = 0, hO = 0, dO = 0; uint64_t last = eadk_timing_millis(), lapT = last;
  for (int i = 0; i < NA; i++) bind[i] = BDEF[i];
  { uint32_t fn = 0; menuPlain = !ef_read(CFG_FILE, &fn) && !ef_read(REC_FILE, &fn); }   // aucun fichier de sauvegarde = premier lancement
  gen_car(genSeed); rec_load(); cfg_load(); mods_load(); dev_apply();
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
    if (st != lastSt) { if (st == S_MENU) { mode_clear(); menu_init(); redraw = 1; } if (lastSt != -1) menuPlain = 0; lastSt = st; }
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    int U = pressed(k, eadk_key_up), D = pressed(k, eadk_key_down), L = pressed(k, eadk_key_left), R = pressed(k, eadk_key_right), O = pressed(k, eadk_key_ok), B = pressed(k, eadk_key_back);
    if ((k >> eadk_key_home) & 1) { rec_save(); return np_app_end(); }   // Home: on sauvegarde et on quitte proprement
    if (st == S_MENU) {
      if (redraw) { draw_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 6) % 7; redraw = 1; } if (D) { sel = (sel + 1) % 7; redraw = 1; }
      if ((L || R || O) && sel == 1) { map_set(map_cur() + (L ? -1 : 1)); redraw = 1; menu_init(); menuPlain = 0; }
      else if (O) { if (sel == 0) { tutOn = 0; delOn = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 2) { garage_car(); st = S_GARAGE; redraw = 1; }
        else if (sel >= 3 && sel <= 5) { pgBase = sel; pgRet = S_MENU; pgInGame = 0; pgDepth = 0; pgCur = sel == 3 ? P_CHALL : (sel == 4 ? P_PROF : P_OPT); sel = 0; st = S_PAGE; redraw = 1; }
        else if (sel == 6) { rec_save(); return np_app_end(); } }
      if (st == S_MENU && !menuPlain) menu_bg();
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
          if (gsel == 1) { cv = (cv + nVeh + d) % nVeh; if (cv == 4) gen_car(genSeed); }
          else if (gsel == 2) { int seed = (int)genSeed + d; if (seed < 1) seed = 9999; if (seed > 9999) seed = 1; gen_car((unsigned)seed); cv = 4; }
          else if (gsel == 3) cw = (cw + 4 + d) % 4; else if (gsel == 4) cs = (cs + 4 + d) % 4; else if (gsel == 5) ce = (ce + 4 + d) % 4; else if (gsel == 6) cc = (cc + 3 + d) % 3;
          else if (gsel == 7) trl = (trl + NTRL + d) % NTRL; else if (gsel == 8) tHood = tHood > .5f ? 0 : 1; else if (gsel == 9) tDoor = tDoor > .5f ? 0 : 1; else if (gsel == 10) tTrunk = tTrunk > .5f ? 0 : 1; else ch = 0; }
        if (ch && gsel >= 1 && gsel <= 7) garage_car();
        if (O && gsel == 11) { tutOn = 0; delOn = 0; startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        if (B) { st = S_MENU; redraw = 1; }
      }
      if (st == S_GARAGE) {
        oHood += (tHood - oHood) * .15f; oDoor += (tDoor - oDoor) * .15f; oTrunk += (tTrunk - oTrunk) * .15f;
        float hx, hz, cx, cy, cz; frame(&hx, &hz, &cx, &cy, &cz); float dist = gzoom + (trl ? 3.5f + TRL[trl].cam : 0.f);
        camhx = fsin(gorb); camhz = fsin(gorb + 1.5708f); camx = cx - camhx * dist; camz = cz - camhz * dist; camy = cy + gelev; CXc = lw * 3 / 4;
        hor = (int)(lh * .66f - gelev * foc / dist);
        int sv = showTop; showTop = 0; scene(160); showTop = sv; present(160);
      }
    } else if (st == S_GAME) {
      if (B) { st = S_PAUSE; sel = 0; redraw = 1; pk = k; rec_save(); continue; }
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
      if (bpress(k, A_MAP)) optMap = (optMap + 1) % 3;
      if (bpress(k, A_DMG)) { if (tutOn) evDmg = 1; st = S_DMG; redraw = 1; pk = k; continue; }
      if (tutOn && bpress(k, A_SKIP)) tut_adv(hx, hz);
      if (bpress(k, A_QUICK) || bpress(k, A_QUICK2)) { st = S_PAGE; pgRet = S_GAME; pgInGame = 1; pgDepth = 0; pgCur = P_FAV; pgBase = 0; sel = 0; redraw = 1; pk = k; continue; }
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
      if (bpress(k, A_TSTOP)) { if (tsi) { tsPrev = tsi; tsi = 0; } else tsi = tsPrev ? tsPrev : 4; }          // stop / reprise du temps
      if (bpress(k, A_TSLOW) && tsi > 1) tsi--;                                                            // plus lent (x0.1 minimum)
      if (bpress(k, A_TFAST)) { if (!tsi) tsi = tsPrev ? tsPrev : 4; else if (tsi < 5) tsi++; }            // plus vite (x2 maximum), reprend si stop
      tscale = TSV[tsi];
      clutchKey = bdown(k, A_CLUTCH) ? 1.f : 0.f; hbrake += ((bdown(k, A_HBRAKE) ? 1.f : 0.f) - hbrake) * .6f; turbo = turboT || bdown(k, A_TURBO) || (powT > 0.f && powTurbo); thr = bdown(k, A_ACC) ? 1.f : 0; brk = 0;
      if (bdown(k, A_BRK)) { if (vf > 1.f) brk = 1; else thr = -.6f; }
      float tgt = (bdown(k, A_RIGHT) ? 1.f : 0) - (bdown(k, A_LEFT) ? 1.f : 0);
      stw += (tgt * (steerBase + .05f * steerL) / (1.f + sp * .06f) - stw) * (.1f + .05f * steerL); steer = stw;
      uint64_t now = eadk_timing_millis(); float dtf = (float)(now - last) * tscale; last = now;
      if (dtf > 0.f) { wspin += vf * dtf * .001f / .35f; acc += dtf; if (acc > 60) acc = 60;
      while (acc >= 3.5f) { world_step(.0035f); acc -= 3.5f; }
      traffic_update(dtf * .001f); ev_update(dtf * .001f); fx_update(dtf * .001f); bots_frame(dtf * .001f); skid_update(); tires_update(dtf * .001f); impact_fx(dtf * .001f); body_fx(dtf * .001f); blast_fx(); airbag_fx(); part_update(dtf * .001f); ev_fx(dtf * .001f); if (grindT > 0.f) grindT -= dtf * .001f; if (honkMsgT > 0.f) honkMsgT -= dtf * .001f;
      stats_update(dtf * .001f, sp); refuel_update(dtf * .001f, sp);
      if (powT > 0.f) { powT -= dtf * .001f; if (powT <= 0.f) power_end(); else dev_apply(); }   // pouvoir temporaire (mods)
      if (bdown(k, A_FUEL) && !burning && !exploded && fuel < 1.f) { fuel += dtf * .0003f; if (fuel > 1.f) fuel = 1.f; stRefuel = 1; }   // fuel button: hold to refill (about 3 s from empty)
      ach_frame(dtf * .001f, sp, vf);
      if (delOn) { deliv_update(dtf * .001f); if (delEnd) { st = S_DRES; redraw = 1; pk = k; continue; } }
      }   // fin de la mise a jour (sautee quand le temps est stoppe)
      if (mapId == 0) {
        if (cx < -TA * .8f && cz > -20 && cz < 20) cp = 1;
        if (cp && pz < 0 && cz >= 0 && cx > 0) { int t = (int)(now - lapT); if (!best || t < best) best = t; if (!recLap || t < recLap) { recLap = t; rec_save(); } stLaps++; ach_check(); lap++; lapT = now; cp = 0; }
        pz = cz;
      } else if (mapId == 2) {
        if (sp > vpk) vpk = sp;
        if (ct == 0 && dmg > 3.f) { ct = 1; impV = vpk; }
        if (ct == 1) { stopT = sp < 1.5f ? stopT + 1 : 0; if (stopT > 70) { ct = 2; score = (int)(dmg * impV * 3.6f / 5.f); if (crashI == 4 && impV * 3.6f > 125.f) { ach_unlock(14); ach_check(); } st = S_DMG; redraw = 1; pk = k; continue; } }
      }
      shx += (hx - shx) * .1f; shz += (hz - shz) * .1f; float l = fsqrt(shx * shx + shz * shz) + 1e-4f; shx /= l; shz /= l;
      float c = fsin(yawO + 1.5708f), s = fsin(yawO); camhx = shx * c + shz * s; camhz = shz * c - shx * s;
      float dist = camInterior ? 3.4f : (ntr >= 0 ? 10.f + TRL[trl].cam : 8.5f) + dO; if (dist < 3.f) dist = 3.f;
      if (camInterior) { camx = cx - .28f * camhz + .12f * camhx; camz = cz + .28f * camhx + .12f * camhz; camy = cy + .55f * VEH[cv].sy; }
      else { camx = cx - camhx * dist; camz = cz - camhz * dist;
        float gy = gh(camx, camz) + 1.4f, ty = cy + 2.6f + hO; if (ty < gy) ty = gy; camy += (ty - camy) * .15f; }
      hor = (int)(lh * (camInterior ? .42f : .72f) - (camy - cy) * foc / dist);
      scene(0); draw_overlay(sp * 3.6f, vf < -.5f ? -1 : gearNow); if (delOn) draw_deliv_hud(); if (mapId == 3 && optMap) { draw_minimap(0); draw_minimap(1); } ach_banner_bg(); present(0); ach_banner_txt();   // the map is drawn last: in full screen it hides the gauges and the rest
      if (++fr % 6 == 0) { if (mapId == 3 && optMap == 2) eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14)); else if (delOn) deliv_bar((int)(sp * 3.6f)); else hud((int)(sp * 3.6f), vf < -.5f ? -1 : gearNow, lap, (int)(now - lapT), recLap); }
      if (tutOn) draw_tutorial();
    } else if (st == S_DMG) {
      if (redraw) { draw_dmg(); redraw = 0; }
      if (U || D || L || R || O || B) { if (O && mapId == 2 && ct == 2) { startpos(); } st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
      eadk_timing_msleep(30);
    } else if (st == S_PAUSE) {
      if (redraw) { draw_pause(sel); redraw = 0; }
      if (U) { sel = (sel + 6) % 7; redraw = 1; } if (D) { sel = (sel + 1) % 7; redraw = 1; }
      if (B) { sel = 0; O = 1; }
      if (O) { if (sel == 0) { st = S_GAME; last = eadk_timing_millis(); acc = 0; }
        else if (sel == 1) { if (tutOn) { tutStep = tutFlag = tutOk = 0; evDmg = evRepair = 0; } if (delOn) deliv_begin(); else startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; }
        else if (sel == 2 || sel == 3) { pgBase = sel; pgRet = S_PAUSE; pgInGame = 1; pgDepth = 0; pgCur = sel == 2 ? P_FAV : P_OPT; sel = 0; st = S_PAGE; redraw = 1; }
        else if (sel == 4) { st = S_DMG; redraw = 1; }
        else if (sel == 5) { if (tutOn || delOn) { tutOn = 0; delOn = 0; mapId = mapBak; } garage_car(); st = S_GARAGE; redraw = 1; }
        else { if (tutOn || delOn) { tutOn = 0; delOn = 0; mapId = mapBak; } st = S_MENU; sel = 0; redraw = 1; } }
      eadk_timing_msleep(30);
    } else if (st == S_PAGE) {
      uint16_t it[24]; int cnt = page_list(pgCur, it), closeP = 0;
      if (sel >= cnt) sel = cnt - 1; if (sel < 0) sel = 0;
      if (pgMsgT > 0 && --pgMsgT == 0) redraw = 1;
      if (redraw) { draw_page(it, cnt, sel); redraw = 0; }
      if (cnt > 1) { if (U) { sel = (sel + cnt - 1) % cnt; redraw = 1; } if (D) { sel = (sel + 1) % cnt; redraw = 1; } }
      int id = cnt ? it[sel] : -1, hl = (int)((k >> eadk_key_left) & 1), hr = (int)((k >> eadk_key_right) & 1), rep = 0, mul = 1;
      if (hl || hr) pgHold++; else pgHold = 0;
      if (id >= 128 && id < MOD_POW0 && pgHold > 10 && (pgHold & 1)) { rep = 1; mul = pgHold > 40 ? 10 : (pgHold > 20 ? 4 : 1); }   // maintenir Gauche/Droite: repetition acceleree
      if (id >= 0 && (L || R || O || rep)) { int d = (L || (rep && hl)) ? -1 : 1; redraw = 1;
        if (id >= MOD_POW0) {                                                     // mods: pouvoir (en jeu) ou mode (lance la partie)
          if (id < MOD_MODE0) { if (O) { power_use(id - MOD_POW0); closeP = 1; } }
          else if (O) { mode_apply(id - MOD_MODE0); tutOn = 0; delOn = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); } }
        else if (id >= 128) { if (O) { if (devOn) { dev_reset(id - 128); cfg_save(); } else { pgMsg = "Active le mode developpeur"; pgMsgT = 40; } } else { dev_edit(id - 128, d, mul); cfg_save(); } }
        else if (id >= 100) { if (O || R) { if (pgDepth < 5) { pgStk[pgDepth] = (uint8_t)pgCur; pgSelStk[pgDepth] = (uint8_t)sel; pgDepth++; } pgCur = id - 100; sel = 0; } }
        else if (id < QUICK_COUNT) { lastQid = id; lastQd = d; int r = quick_apply(id, d, O);
          if (r & 2) { lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); }
          if ((r & 1) && pgInGame) closeP = 1; }
        else switch (id) {
          case O_QUAL: qual = (qual + 3 + d) % 3; setq(); break;
          case O_STEER: steerL += d; if (steerL < 1) steerL = 1; if (steerL > 5) steerL = 5; break;
          case O_CAM: camL += d; if (camL < 1) camL = 1; if (camL > 5) camL = 5; break;
          case O_FOV: fovI = (fovI + 3 + d) % 3; setq(); break;
          case O_VIEW: vdI = (vdI + 3 + d) % 3; setq(); break;
          case O_SKID: optSkid ^= 1; skid_clear(); break;
          case O_PART: optPart ^= 1; part_clear(); break;
          case O_OBJ: optObj ^= 1; if (!optObj) objects_clear(); break;                   // objets remis au prochain depart
          case O_RAMP: optRamp ^= 1; break;
          case O_PUNCT: optPunct ^= 1; break;
          case O_SOLID: solid = (solid + 3 + d) % 3; if (!pgInGame) menu_init(); break;
          case O_CRASHV: crashI = (crashI + 5 + d) % 5; break;
          case O_MODE: advMode ^= 1; cfg_save(); break;
          case O_DEV: devOn ^= 1; if (devOn) dev_apply(); else dev_reset_all(); cfg_save(); break;
          case O_FAVRESET: if (O) { fav_default(); cfg_save(); pgMsg = "Favoris remis par defaut"; pgMsgT = 40; } break;
          case O_DEVRESET: if (O) { dev_reset_all(); cfg_save(); pgMsg = "Variables reinitialisees"; pgMsgT = 40; } break;
          case O_KEYS: if (O) { pgSelSave = sel; st = S_KEYS; redraw = 1; } break;
          case O_CTRL: if (O) { pgSelSave = sel; cap = 0; sel = 0; st = S_CTRL; redraw = 1; } break;
          case O_STATS: if (O) { pgSelSave = sel; st = S_STATS; stConfirm = 0; redraw = 1; } break;
          case O_MODINFO: if (O) { pgMsg = modErr[0] ? modErr : "Scripts .py avec # numbeam"; pgMsgT = 60; } break;
          case O_MODRELOAD: if (O) { mods_load(); pgMsg = modErr[0] ? modErr : "Scripts recharges"; pgMsgT = 60; } break;
          case O_ACH: if (O) { pgSelSave = sel; st = S_ACH; sel = 0; redraw = 1; } break;
          case O_DELIV: if (O) { pgSelSave = sel; mapBak = mapId; mapId = 3; sel = 0; delOn = 0; st = S_DELIV; menu_init(); redraw = 1; } break;
          case O_TUTO: if (O) { fav_ensure(0); fav_ensure(1); cfg_save(); mapBak = mapId; mapId = 3; tutOn = 1; tutStep = tutFlag = tutOk = evDmg = evRepair = 0; yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); } break;
          default: break; }
      }
      if (st == S_PAGE && pressed(k, eadk_key_shift) && id >= 0 && id != O_MODINFO && id != O_MODRELOAD && (id < 100 || id >= 128)) {      // Shift: ajouter / retirer des favoris
        int r = fav_toggle(id); cfg_save(); redraw = 1;
        pgMsg = r < 0 ? "Favoris pleins (8 max)" : (r ? "Ajoute aux favoris" : "Retire des favoris"); pgMsgT = 40; }
      if (st == S_PAGE && !closeP) {
        if (B) { if (pgDepth > 0) { pgCur = pgStk[--pgDepth]; sel = pgSelStk[pgDepth]; redraw = 1; } else if (pgRet == S_GAME) closeP = 1; else { st = pgRet; sel = pgBase; redraw = 1; } }
        else if (pgRet == S_GAME && (bpress(k, A_QUICK) || bpress(k, A_QUICK2))) closeP = 1; }
      if (closeP) { st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; }
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
        if (B) { clear(); st = S_PAGE; sel = pgSelSave; redraw = 1; }
      }
      eadk_timing_msleep(30);
    } else if (st == S_DELIV) {
      if (redraw) { draw_deliv_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 2) % 3; redraw = 1; } if (D) { sel = (sel + 1) % 3; redraw = 1; }
      if ((L || R || O) && sel == 0) { delLvl = (delLvl + 3 + (L ? -1 : 1)) % 3; redraw = 1; }
      else if (O && sel == 1) { delOn = 1; tutOn = 0; deliv_begin(); yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
      else if (B || (O && sel == 2)) { mapId = mapBak; clear(); st = S_PAGE; sel = pgSelSave; redraw = 1; }
      if (st == S_DELIV) menu_bg();
    } else if (st == S_DRES) {
      if (redraw) { draw_deliv_res(); redraw = 0; }
      if (O) { deliv_begin(); yawO = hO = dO = 0; tHood = tDoor = tTrunk = oHood = oDoor = oTrunk = 0; st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
      else if (B) { delOn = 0; mapId = mapBak; st = S_MENU; sel = 3; redraw = 1; }
      eadk_timing_msleep(30);
    } else if (st == S_ACH) {
      if (redraw) { draw_ach(sel); redraw = 0; }
      if (U) { sel = (sel + NACH - 1) % NACH; redraw = 1; } if (D) { sel = (sel + 1) % NACH; redraw = 1; }
      if (B) { clear(); st = S_PAGE; sel = pgSelSave; redraw = 1; }
      eadk_timing_msleep(30);
    } else if (st == S_STATS) {
      if (redraw) { draw_stats(); redraw = 0; }
      if (O) { if (stConfirm) { stDist = stTop = stDrift = stDriftBest = stDriftCur = 0.f; stCrash = 0; stRev = stZen = 0.f; stDeliv = stStops = stBooms = stLoops = stLaps = stVeh = 0; stConfirm = 0; rec_save(); } else stConfirm = 1; redraw = 1; }
      else if (B) { clear(); st = S_PAGE; sel = pgSelSave; redraw = 1; }
      eadk_timing_msleep(30);
    } else {                                              // S_KEYS: key tester
      if (redraw) { clear(); txt("TEST DES TOUCHES", 90, 8, 1, ACC, BGC); txt("Appuie sur des touches. Retour: sortir", 30, 40, 0, C(150, 160, 190), BGC); redraw = 2; }
      if (B) { clear(); st = S_PAGE; sel = pgSelSave; redraw = 1; }
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

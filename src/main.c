#include <eadk.h>
#include "sim.h"
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumBeam3D";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

#define LW 160
#define LH 112
#define FOC 130.f
#define HOR 50
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static uint16_t fb[LW * LH], buf[320 * 16];
static int structure;
static float camhx = 0, camhz = 1, camx, camy = 5, camz;

static inline void px(int x, int y, uint16_t c) { if ((unsigned)x < LW && (unsigned)y < LH) fb[y * LW + x] = c; }
static void line(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (int i = 0; i < 300; i++) { px(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
}
static void quad(const float *xs, const float *ys, uint16_t c) {   // convex quad fill
  float mn = 1e9f, mx = -1e9f;
  for (int i = 0; i < 4; i++) { if (ys[i] < mn) mn = ys[i]; if (ys[i] > mx) mx = ys[i]; }
  for (int y = (int)mn < 0 ? 0 : (int)mn; y <= (int)mx && y < LH; y++) {
    float lo = 1e9f, hi = -1e9f, yy = y + .5f;
    for (int i = 0; i < 4; i++) {
      int j = (i + 1) & 3; float a = ys[i], b = ys[j];
      if ((a <= yy) == (b <= yy)) continue;
      float x = xs[i] + (yy - a) / (b - a) * (xs[j] - xs[i]); if (x < lo) lo = x; if (x > hi) hi = x;
    }
    for (int x = (int)lo; x <= (int)hi; x++) px(x, y, c);
  }
}
static void terrain(void) {
  int top[LW]; float rx = camhz, rz = -camhx;
  for (int y = 0; y < LH; y++) { uint16_t s = C(60 + y, 120 + y, 215 + (y > 39 ? 40 : y)); for (int x = 0; x < LW; x++) fb[y * LW + x] = s; }
  for (int i = 0; i < LW; i++) top[i] = LH;
  for (float z = 1.2f; z < 72.f; z += .3f + z * .05f) {
    float t = z / 72.f, fx = camx + camhx * z, fz = camz + camhz * z;
    for (int i = 0; i < LW; i++) {
      float k = (i - LW / 2 + .5f) / FOC * z, X = fx + rx * k, Z = fz + rz * k, h = gh(X, Z);
      int sy = (int)(HOR + (camy - h) * FOC / z); if (sy < 0) sy = 0;
      if (sy >= top[i]) continue;
      float ax = X < 0 ? -X : X; int r, g, b, ck = ((int)(X * .5f + 1000) ^ (int)(Z * .5f + 1000)) & 1;
      if (gfeat > .25f) { r = 175; g = 170; b = 150; }
      else if (ax < 6.f) { r = 72; g = 72; b = 78; if (ax < .15f && (((int)(Z / 3.f)) & 1)) { r = 230; g = 210; b = 70; } if (ax > 5.6f) { r = g = b = 200; } }
      else { r = 50 + ck * 8; g = 130 + (int)(h * 5) + ck * 10; b = 45; if (g > 200) g = 200; if (g < 60) g = 60; }
      r += (int)((100 - r) * t); g += (int)((170 - g) * t); b += (int)((225 - b) * t);
      uint16_t col = C(r, g, b);
      for (int y = sy; y < top[i]; y++) fb[y * LW + i] = col;
      top[i] = sy;
    }
  }
}
static const uint8_t FA[11][4] = {{0,2,8,6},{1,3,9,7},{0,1,7,6},{2,3,9,8},{18,19,9,8},{12,13,19,18},{10,11,13,12},{6,7,11,10},{6,10,12,18},{7,11,13,19},{0,1,3,2}};
static const uint8_t GL[11] = {0,0,0,0,0,1,0,1,1,1,0};
static float sxp[NN], syp[NN], zr[NN];
static void car(void) {
  float rx = camhz, rz = -camhx;
  for (int i = 0; i < NN; i++) {
    float dx = n[i].x - camx, dz = n[i].z - camz; zr[i] = dx * camhx + dz * camhz;
    if (zr[i] < .4f) zr[i] = .4f;
    sxp[i] = LW / 2 + (dx * rx + dz * rz) / zr[i] * FOC; syp[i] = HOR - (n[i].y - camy) / zr[i] * FOC;
  }
  if (structure) {
    for (int i = 0; i < nb; i++) if (bm[i].f != 2) {
      float d = (bm[i].l0 - bm[i].lr) / bm[i].lr; d = (d < 0 ? -d : d) * 12; if (d > 1) d = 1;
      line((int)sxp[bm[i].a], (int)syp[bm[i].a], (int)sxp[bm[i].b], (int)syp[bm[i].b], C(255, 255 - (int)(d * 230), 255 - (int)(d * 255)));
    }
  }
  int id[15]; float dp[15]; int m = 0;
  for (int f = 0; f < 11 && !structure; f++) { id[m] = f; dp[m++] = (zr[FA[f][0]] + zr[FA[f][1]] + zr[FA[f][2]] + zr[FA[f][3]]) * .25f; }
  for (int w = 14; w < 18; w++) { id[m] = 100 + w; dp[m++] = zr[w] - .3f; }
  for (int i = 1; i < m; i++) { int a = id[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { id[j + 1] = id[j]; dp[j + 1] = dp[j]; j--; } id[j + 1] = a; dp[j + 1] = v; }
  for (int k = 0; k < m; k++) {
    if (id[k] >= 100) { int w = id[k] - 100, rr = (int)(.38f * FOC / zr[w]); for (int y = -rr; y <= rr; y++) { int hw = (int)fsqrt((float)(rr * rr - y * y)); for (int x = -hw; x <= hw; x++) px((int)sxp[w] + x, (int)syp[w] + y, rr > 3 && x * x + y * y < rr * rr / 4 ? C(170, 170, 175) : C(22, 22, 26)); } continue; }
    int f = id[k]; float xs[4], ys[4]; const uint8_t *q = FA[f];
    for (int i = 0; i < 4; i++) { xs[i] = sxp[q[i]]; ys[i] = syp[q[i]]; }
    float ux = P[0][0], nx, ny, nz; (void)ux;
    float ax = n[q[1]].x - n[q[0]].x, ay = n[q[1]].y - n[q[0]].y, az = n[q[1]].z - n[q[0]].z, bx = n[q[3]].x - n[q[0]].x, by = n[q[3]].y - n[q[0]].y, bz = n[q[3]].z - n[q[0]].z;
    nx = ay * bz - az * by; ny = az * bx - ax * bz; nz = ax * by - ay * bx;
    float nl = fsqrt(nx * nx + ny * ny + nz * nz) + 1e-4f, lit = (.3f * nx + .8f * ny + .5f * nz) / nl / .97f; if (lit < 0) lit = -lit;
    float sh = .5f + .5f * lit, dk = 1.f - dmg * .006f;
    quad(xs, ys, GL[f] ? C((int)(120 * (.8f + .2f * lit)), (int)(170 * (.8f + .2f * lit)), 210) : C((int)(205 * sh * dk), (int)(35 * sh), (int)(30 * sh)));
  }
}
static void present(void) {
  for (int by = 0; by < 224; by += 16) {
    for (int r = 0; r < 8; r++) for (int x = 0; x < LW; x++) {
      uint16_t c = fb[(by / 2 + r) * LW + x]; uint16_t *d = &buf[(2 * r) * 320 + 2 * x];
      d[0] = d[1] = d[320] = d[321] = c;
    }
    eadk_display_push_rect((eadk_rect_t){0, by, 320, 16}, buf);
  }
}
static void num(char *s, int v) { char t[8]; int k = 0; if (v < 0) v = 0; do { t[k++] = '0' + v % 10; v /= 10; } while (v && k < 7); while (k) *s++ = t[--k]; *s = 0; }
static void hud(int kmh, int m) {
  char s[48], *p = s; const char *a = "km/h  Degats ", *e = "%  ";
  num(p, kmh); while (*p) p++; while (*a) *p++ = *a++;
  num(p, (int)dmg); while (*p) p++;
  while (*e) *p++ = *e++;
  num(p, m); while (*p) p++;
  *p++ = 'm'; *p = 0;
  eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14));
  eadk_display_draw_string(s, (eadk_point_t){4, 225}, false, 0xFFFF, C(10, 10, 14));
}
int main(void) {
  car_init(0, 0); float acc = 0, st = 0; uint64_t last = eadk_timing_millis(); int fr = 0, pT = 0, pO = 0;
  eadk_display_push_rect_uniform(eadk_screen_rect, 0);
  for (;;) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (eadk_keyboard_key_down(k, eadk_key_back) || eadk_keyboard_key_down(k, eadk_key_home)) return 0;
    float cx = 0, cy = 0, cz = 0, vx = 0, vz = 0;
    for (int i = 0; i < NN; i++) { cx += n[i].x; cy += n[i].y; cz += n[i].z; vx += n[i].vx; vz += n[i].vz; }
    cx /= NN; cy /= NN; cz /= NN; vx /= NN; vz /= NN;
    float hx = n[16].x + n[17].x - n[14].x - n[15].x, hz = n[16].z + n[17].z - n[14].z - n[15].z, hl = fsqrt(hx * hx + hz * hz) + 1e-4f; hx /= hl; hz /= hl;
    float vf = vx * hx + vz * hz, sp = fsqrt(vx * vx + vz * vz);
    int o = eadk_keyboard_key_down(k, eadk_key_ok), t = eadk_keyboard_key_down(k, eadk_key_toolbox);
    if (o && !pO) car_init(cx, cz); if (t && !pT) structure ^= 1; pO = o; pT = t;
    int U = eadk_keyboard_key_down(k, eadk_key_up), D = eadk_keyboard_key_down(k, eadk_key_down);
    thr = U ? 1.f : 0; brk = 0;
    if (D) { if (vf > 1.f) brk = 1; else thr = -.6f; }
    float tgt = (eadk_keyboard_key_down(k, eadk_key_right) ? 1.f : 0) - (eadk_keyboard_key_down(k, eadk_key_left) ? 1.f : 0);
    st += (tgt * .45f / (1.f + sp * .06f) - st) * .25f; steer = st;
    uint64_t now = eadk_timing_millis(); acc += (float)(now - last); last = now; if (acc > 60) acc = 60;
    while (acc >= 3.5f) { step(.0035f); acc -= 3.5f; }
    camhx += (hx - camhx) * .1f; camhz += (hz - camhz) * .1f; float l = fsqrt(camhx * camhx + camhz * camhz) + 1e-4f; camhx /= l; camhz /= l;
    camx = cx - camhx * 8.5f; camz = cz - camhz * 8.5f;
    float gy = gh(camx, camz) + 1.4f, ty = cy + 2.6f; if (ty < gy) ty = gy; camy += (ty - camy) * .15f;
    terrain(); car(); present();
    if (++fr % 6 == 0) hud((int)(sp * 3.6f), (int)cz);
  }
}

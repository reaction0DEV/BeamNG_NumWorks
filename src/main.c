#include <eadk.h>
#include "sim.h"
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumBeam";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

#define W EADK_SCREEN_WIDTH
#define H EADK_SCREEN_HEIGHT
#define BH 16
#define HUD 16
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static uint16_t buf[W * BH], dcol[W];
static int16_t gcol[W], hcol[W];
static int by0, structure;
static float ang;

static inline void plot(int x, int y, uint16_t c) {
  y -= by0; if ((unsigned)x < W && (unsigned)y < BH) buf[y * W + x] = c;
}
static void line(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (int i = 0; i < 400; i++) {
    plot(x0, y0, c); plot(x0 + 1, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
// scanline polygon fill, vertices in screen coords
static void poly(const float *vx, const float *vy, int k, uint16_t c) {
  for (int row = by0; row < by0 + BH; row++) {
    float xs[10]; int m = 0, yy = row;
    for (int i = 0; i < k; i++) {
      int j = (i + 1) % k; float ya = vy[i], yb = vy[j];
      if ((ya <= yy + .5f) == (yb <= yy + .5f)) continue;
      float t = (yy + .5f - ya) / (yb - ya);
      if (m < 10) xs[m++] = vx[i] + t * (vx[j] - vx[i]);
    }
    for (int i = 1; i < m; i++) { float v = xs[i]; int j = i - 1; while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; } xs[j + 1] = v; }
    for (int i = 0; i + 1 < m; i += 2)
      for (int x = (int)xs[i]; x <= (int)xs[i + 1]; x++) plot(x, yy, c);
  }
}
static void wheel(float cx, float cy) {
  for (int row = by0; row < by0 + BH; row++) {
    int dy = row - (int)cy; if (dy < -7 || dy > 7) continue;
    int hw = (int)fsqrt(49.f - dy * dy);
    for (int x = -hw; x <= hw; x++) plot((int)cx + x, row, C(25, 25, 28));
    if (dy > -4 && dy < 4) { int h2 = (int)fsqrt(16.f - dy * dy); for (int x = -h2; x <= h2; x++) plot((int)cx + x, row, C(170, 170, 175)); }
  }
  line((int)cx, (int)cy, (int)(cx + 4 * fsin(ang + 1.5708f)), (int)(cy + 4 * fsin(ang)), C(60, 60, 60));
}
static void render(float camx, float camy) {
  for (int c = 0; c < W; c++) {
    float xw = camx + c;
    gcol[c] = (int)(gy(xw) - camy);
    float xm = xw * 0.4f;
    hcol[c] = (int)(105 + 28 * fsin(xm * 0.02f) + 13 * fsin(xm * 0.055f + 1.f) - camy * 0.4f);
    dcol[c] = (((int)xw >> 5) & 1) ? C(112, 82, 52) : C(100, 72, 45);
  }
  float px[NN], py[NN];
  for (int i = 0; i < NN; i++) { px[i] = n[i].x - camx; py[i] = n[i].y - camy; }
  static const uint8_t hull[9] = {0, 1, 2, 3, 8, 7, 6, 5, 4};
  for (by0 = HUD; by0 < H; by0 += BH) {
    for (int r = 0; r < BH; r++) {
      uint16_t sky = C(70 + (by0 + r) / 3, 130 + (by0 + r) / 4, 220);
      for (int c = 0; c < W; c++) {
        int y = by0 + r; uint16_t col = sky;
        if (y >= hcol[c]) col = C(96, 112, 140);
        if (y >= gcol[c]) col = y < gcol[c] + 3 ? C(70, 170, 60) : dcol[c];
        buf[r * W + c] = col;
      }
    }
    if (structure) {
      for (int i = 0; i < nb; i++) if (bm[i].f != 2) {
        float d = (bm[i].l0 - bm[i].lr) / bm[i].lr; d = d < 0 ? -d : d; d *= 12; if (d > 1) d = 1;
        line((int)px[bm[i].a], (int)py[bm[i].a], (int)px[bm[i].b], (int)py[bm[i].b], C(255, 255 - (int)(d * 230), 255 - (int)(d * 255)));
      }
      wheel(px[9], py[9]); wheel(px[10], py[10]);
    } else {
      float hx[9], hy[9], wx[4], wy[4], mx = 0, my = 0;
      for (int i = 0; i < 9; i++) { hx[i] = px[hull[i]]; hy[i] = py[hull[i]]; }
      int dk = dmg > 66 ? 90 : (dmg > 33 ? 140 : 200);
      poly(hx, hy, 9, C(dk, 25, 25));
      const uint8_t wn[4] = {7, 6, 5, 4};
      for (int i = 0; i < 4; i++) { wx[i] = px[wn[i]]; wy[i] = py[wn[i]]; mx += wx[i] / 4; my += wy[i] / 4; }
      for (int i = 0; i < 4; i++) { wx[i] += (mx - wx[i]) * .22f; wy[i] += (my - wy[i]) * .22f; }
      poly(wx, wy, 4, C(150, 200, 235));
      for (int i = 0; i < 9; i++) line((int)hx[i], (int)hy[i], (int)hx[(i + 1) % 9], (int)hy[(i + 1) % 9], C(40, 8, 8));
      wheel(px[9], py[9]); wheel(px[10], py[10]);
    }
    eadk_display_push_rect((eadk_rect_t){0, by0, W, BH}, buf);
  }
}
static void num(char *s, int v) { char t[8]; int k = 0; if (v < 0) v = 0; do { t[k++] = '0' + v % 10; v /= 10; } while (v && k < 7); while (k) *s++ = t[--k]; *s = 0; }
static void hud(int kmh, int m) {
  char s[48], *p = s; const char *a = "km/h ", *d = "  Degats ", *e = "%  ";
  num(p, kmh); while (*p) p++; while (*a) *p++ = *a++;
  while (*d) *p++ = *d++; num(p, (int)dmg); while (*p) p++; while (*e) *p++ = *e++;
  num(p, m); while (*p) p++; *p++ = 'm'; *p = 0;
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, W, HUD}, C(10, 10, 14));
  eadk_display_draw_string(s, (eadk_point_t){4, 1}, false, 0xFFFF, C(10, 10, 14));
}
int main(void) {
  car_init(120); float camy = 30, acc = 0; uint64_t last = eadk_timing_millis(); int fr = 0, pT = 0, pO = 0;
  eadk_display_push_rect_uniform(eadk_screen_rect, 0);
  for (;;) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (eadk_keyboard_key_down(k, eadk_key_back) || eadk_keyboard_key_down(k, eadk_key_home)) return 0;
    float cx = 0, cy = 0, vx = 0;
    for (int i = 0; i < 9; i++) { cx += n[i].x; cy += n[i].y; vx += n[i].vx; }
    cx /= 9; cy /= 9; vx /= 9;
    int o = eadk_keyboard_key_down(k, eadk_key_ok), t = eadk_keyboard_key_down(k, eadk_key_toolbox);
    if (o && !pO) car_init(cx); if (t && !pT) structure ^= 1; pO = o; pT = t;
    int L = eadk_keyboard_key_down(k, eadk_key_left), R = eadk_keyboard_key_down(k, eadk_key_right);
    thr = R ? 1.f : 0; brk = 0;
    if (L) { if (vx > 8) brk = 1; else thr = -.6f; }
    pitch = eadk_keyboard_key_down(k, eadk_key_up) ? 1.f : (eadk_keyboard_key_down(k, eadk_key_down) ? -1.f : 0);
    uint64_t now = eadk_timing_millis(); acc += (float)(now - last); last = now;
    if (acc > 60) acc = 60;
    while (acc >= 3.5f) { step(0.0035f); acc -= 3.5f; }
    ang += vx * 0.0143f * 0.3f;
    camy += (cy - 125 - camy) * 0.12f;
    render(cx - 120 + vx * 0.12f, camy);
    if (++fr % 6 == 0) hud((int)(vx * 0.3f > 0 ? vx * 0.3f : -vx * 0.3f), (int)((cx - 120) / 12));
  }
}

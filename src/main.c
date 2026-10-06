#include <eadk.h>
#include "sim.h"
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumBeam3D";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#define LW 160
#define LH 112
#define FOC 130.f
#define HOR 50
#define C(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
enum { S_MENU, S_GARAGE, S_GAME, S_PAUSE, S_HELP };
static uint16_t fb[LW * LH], buf[320 * 16];
static int structure, CXc = 80, mapSel;
static float camhx = 0, camhz = 1, camx, camy = 5, camz, oHood, oDoor, oTrunk, tHood, tDoor, tTrunk;
static uint64_t pk;

static inline void px(int x, int y, uint16_t c) { if ((unsigned)x < LW && (unsigned)y < LH) fb[y * LW + x] = c; }
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
static void terrain(int xl) {
  int top[LW]; float rx = camhz, rz = -camhx;
  for (int y = 0; y < LH; y++) { uint16_t s = C(60 + y, 120 + y, 215 + (y > 39 ? 40 : y)); for (int x = 0; x < LW; x++) fb[y * LW + x] = s; }
  for (int i = 0; i < LW; i++) top[i] = LH;
  for (float z = 1.2f; z < 72.f; z += .3f + z * .05f) {
    float t = z / 72.f, fx = camx + camhx * z, fz = camz + camhz * z;
    for (int i = xl; i < LW; i++) {
      float k = (i - CXc + .5f) / FOC * z, X = fx + rx * k, Z = fz + rz * k, h = gh(X, Z);
      int sy = (int)(HOR + (camy - h) * FOC / z); if (sy < 0) sy = 0;
      if (sy >= top[i]) continue;
      int r, g, b, ck = ((int)(X * .5f + 1000) ^ (int)(Z * .5f + 1000)) & 1; float ad = gdist < 0 ? -gdist : gdist;
      if (gfeat > .25f) { if (mapId == 0) { r = ((int)((X + Z) * .5f + 1000) & 1) ? 230 : 200; g = ((int)((X + Z) * .5f + 1000) & 1) ? 230 : 40; b = g; } else { r = 175; g = 170; b = 150; } }
      else if (mapId == 0 && ad < 6.f) { r = 70; g = 70; b = 76; if (Z > -1.5f && Z < 1.5f && X > TA - 6 && X < TA + 6) { r = g = b = (((int)(X * 1.5f + 1000) + (int)(Z * 1.5f + 1000)) & 1) ? 240 : 25; } else if (ad > 5.5f) r = g = b = 200; }
      else if (mapId == 0 && ad < 7.4f) { int s = (int)((X + Z) * .3f + 1000) & 1; r = s ? 220 : 235; g = s ? 40 : 235; b = g; }
      else if (mapId == 1 && ad < 6.f) { r = 72; g = 72; b = 78; if (ad < .15f && (((int)(Z / 3.f)) & 1)) { r = 230; g = 210; b = 70; } if (ad > 5.6f) r = g = b = 200; }
      else { r = 50 + ck * 8; g = 130 + (int)(h * 5) + ck * 10; b = 45; if (g > 200) g = 200; if (g < 60) g = 60; }
      r += (int)((100 - r) * t); g += (int)((170 - g) * t); b += (int)((225 - b) * t);
      uint16_t col = C(r, g, b);
      for (int y = sy; y < top[i]; y++) fb[y * LW + i] = col;
      top[i] = sy;
    }
  }
}
// ---- car drawing: real nodes (0..19) + virtual points (panels that open, engine)
static float PX[48], PY[48], PZ[48], sxp[48], syp[48], zr[48];
static int np;
static int vp(float x, float y, float z) { PX[np] = x; PY[np] = y; PZ[np] = z; return np++; }
static int lerpn(int a, int b, float t) { return vp(PX[a] + (PX[b] - PX[a]) * t, PY[a] + (PY[b] - PY[a]) * t, PZ[a] + (PZ[b] - PZ[a]) * t); }
static int lerpp(int a, int b, float t) { return lerpn(a, b, t); }
// rotate point p around axis a->b by angle, picking the sign that moves it "up" (mode 0) or "outward" (mode 1, away from x=cx)
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
static void car(void) {
  float cxm = 0; np = NN;
  for (int i = 0; i < NN; i++) { PX[i] = n[i].x; PY[i] = n[i].y; PZ[i] = n[i].z; cxm += n[i].x; } cxm /= NN;
  // panels: hood hinge 18-19, tailgate hinge 10-11, doors hinge on the front edge
  int hd8 = 8, hd9 = 9, tg6 = 6, tg7 = 7;
  if (oHood > .02f) { hd8 = rot(8, 18, 19, oHood * 1.05f, 0, cxm); hd9 = rot(9, 18, 19, oHood * 1.05f, 0, cxm); }
  if (oTrunk > .02f) { tg6 = rot(6, 10, 11, oTrunk * 1.2f, 0, cxm); tg7 = rot(7, 10, 11, oTrunk * 1.2f, 0, cxm); }
  int dq[2][4];
  for (int s = 0; s < 2; s++) {
    int a = s ? 1 : 0, b = s ? 3 : 2, c1 = s ? 11 : 10, c2 = s ? 13 : 12;
    int p0 = lerpn(a, b, .3f), p1 = lerpn(a, b, .65f), q1 = lerpn(c1, c2, .85f), q0 = lerpn(c1, c2, .1f);
    PX[p0] += s ? .03f : -.03f; PX[p1] += s ? .03f : -.03f; PX[q0] += s ? .03f : -.03f; PX[q1] += s ? .03f : -.03f;
    if (oDoor > .02f) { int r0 = rot(p0, p1, q1, oDoor * 1.1f, 1, cxm), r1 = rot(q0, p1, q1, oDoor * 1.1f, 1, cxm); p0 = r0; q0 = r1; }
    dq[s][0] = p0; dq[s][1] = p1; dq[s][2] = q1; dq[s][3] = q0;
  }
  uint8_t fa[18][4]; uint8_t kind[18]; int m = 0;
#define F(k, a, b, c, d) do { fa[m][0] = a; fa[m][1] = b; fa[m][2] = c; fa[m][3] = d; kind[m++] = k; } while (0)
  F(0, 0, 2, 8, 6); F(0, 1, 3, 9, 7); F(0, 0, 1, 7, 6); F(0, 2, 3, 9, 8); F(0, 0, 1, 3, 2); F(0, 10, 11, 13, 12);
  F(0, 18, 19, hd9, hd8); F(1, 12, 13, 19, 18); F(1, 6, 10, 12, 18); F(1, 7, 11, 13, 19); F(1, tg6, tg7, 11, 10);
  F(3, dq[0][0], dq[0][1], dq[0][2], dq[0][3]); F(3, dq[1][0], dq[1][1], dq[1][2], dq[1][3]);
  if (oHood > .1f) {                                    // engine block in the bay
    int cn[8] = {8, 9, 19, 18, 2, 3, lerpn(5, 3, .4f), lerpn(4, 2, .4f)};
    float mx = 0, my = 0, mz = 0; for (int i = 0; i < 8; i++) { mx += PX[cn[i]] / 8; my += PY[cn[i]] / 8; mz += PZ[cn[i]] / 8; }
    int e[8]; for (int i = 0; i < 8; i++) { int v = vp(PX[cn[i]] + (mx - PX[cn[i]]) * .28f, PY[cn[i]] + (my - PY[cn[i]]) * .2f - (i < 4 ? .1f : 0), PZ[cn[i]] + (mz - PZ[cn[i]]) * .28f); e[i] = v; }
    F(2, e[0], e[1], e[2], e[3]); F(2, e[0], e[1], e[5], e[4]); F(2, e[3], e[2], e[6], e[7]); F(2, e[0], e[3], e[7], e[4]); F(2, e[1], e[2], e[6], e[5]);
  }
  for (int i = 0; i < np; i++) {
    float dx = PX[i] - camx, dz = PZ[i] - camz; zr[i] = dx * camhx + dz * camhz; if (zr[i] < .4f) zr[i] = .4f;
    sxp[i] = CXc + (dx * camhz - dz * camhx) / zr[i] * FOC; syp[i] = HOR - (PY[i] - camy) / zr[i] * FOC;
  }
  if (structure) {
    for (int i = 0; i < nb; i++) if (bm[i].f != 2) {
      float d = (bm[i].l0 - bm[i].lr) / bm[i].lr; d = (d < 0 ? -d : d) * 12; if (d > 1) d = 1;
      line((int)sxp[bm[i].a], (int)syp[bm[i].a], (int)sxp[bm[i].b], (int)syp[bm[i].b], C(255, 255 - (int)(d * 230), 255 - (int)(d * 255)));
    }
  }
  int id[40]; float dp[40]; int k2 = 0;
  for (int f = 0; f < m && !structure; f++) { id[k2] = f; dp[k2++] = (zr[fa[f][0]] + zr[fa[f][1]] + zr[fa[f][2]] + zr[fa[f][3]]) * .25f - (kind[f] == 3 ? .1f : 0) - (kind[f] == 2 ? .3f : 0); }
  for (int w = 14; w < 18; w++) { id[k2] = 100 + w; dp[k2++] = zr[w] - .3f; }
  for (int i = 1; i < k2; i++) { int a = id[i]; float v = dp[i]; int j = i - 1; while (j >= 0 && dp[j] < v) { id[j + 1] = id[j]; dp[j + 1] = dp[j]; j--; } id[j + 1] = a; dp[j + 1] = v; }
  const Veh *V = &VEH[cv]; const Eng *E = &ENG[ce]; const Whl *W = &WHL[cw];
  for (int k = 0; k < k2; k++) {
    if (id[k] >= 100) {
      int w = id[k] - 100, rr = (int)(n[w].r * 1.08f * FOC / zr[w]);
      for (int y = -rr; y <= rr; y++) { int hw = (int)fsqrt((float)(rr * rr - y * y)); for (int x = -hw; x <= hw; x++) px((int)sxp[w] + x, (int)syp[w] + y, rr > 3 && x * x + y * y < rr * rr / 3 ? C(W->cr, W->cg, W->cb) : C(22, 22, 26)); }
      continue;
    }
    int f = id[k]; float xs[4], ys[4]; const uint8_t *q = fa[f];
    for (int i = 0; i < 4; i++) { xs[i] = sxp[q[i]]; ys[i] = syp[q[i]]; }
    float ax = PX[q[1]] - PX[q[0]], ay = PY[q[1]] - PY[q[0]], az = PZ[q[1]] - PZ[q[0]], bx = PX[q[3]] - PX[q[0]], by = PY[q[3]] - PY[q[0]], bz = PZ[q[3]] - PZ[q[0]];
    float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
    float nl = fsqrt(nx * nx + ny * ny + nz * nz) + 1e-4f, lit = (.3f * nx + .8f * ny + .5f * nz) / nl / .97f; if (lit < 0) lit = -lit;
    float sh = .5f + .5f * lit, dk = 1.f - dmg * .006f; uint16_t col;
    if (kind[f] == 1) col = C((int)(120 * (.8f + .2f * lit)), (int)(170 * (.8f + .2f * lit)), 210);
    else if (kind[f] == 2) col = C((int)(E->r * sh), (int)(E->g * sh), (int)(E->b * sh));
    else { float d2 = kind[f] == 3 ? .85f : 1.f; col = C((int)(V->r * sh * dk * d2), (int)(V->g * sh * dk * d2), (int)(V->b * sh * dk * d2)); }
    quad(xs, ys, col);
  }
}
static void present(int xl) {
  int w = (LW - xl) * 2;
  for (int by = 0; by < 224; by += 16) {
    for (int r = 0; r < 8; r++) for (int x = xl; x < LW; x++) {
      uint16_t c = fb[(by / 2 + r) * LW + x]; uint16_t *d = &buf[(2 * r) * w + 2 * (x - xl)]; d[0] = d[1] = d[w] = d[w + 1] = c;
    }
    eadk_display_push_rect((eadk_rect_t){xl * 2, by, w, 16}, buf);
  }
}
// ---- text helpers (ASCII only)
static char *cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *num(char *s, int v) { char t[8]; int k = 0; if (v < 0) v = 0; do { t[k++] = '0' + v % 10; v /= 10; } while (v && k < 7); while (k) *s++ = t[--k]; *s = 0; return s; }
static void txt(const char *s, int x, int y, int big, uint16_t fg, uint16_t bg) { eadk_display_draw_string(s, (eadk_point_t){x, y}, big, fg, bg); }
static void row(const char *s, int x, int y, int w, int sel) {
  uint16_t bg = sel ? C(40, 90, 200) : C(15, 18, 28);
  eadk_display_push_rect_uniform((eadk_rect_t){x, y, w, 18}, bg); txt(s, x + 6, y + 2, 0, 0xFFFF, bg);
}
static int pressed(uint64_t k, int key) { return ((k >> key) & 1) && !((pk >> key) & 1); }
static int down(uint64_t k, int key) { return (k >> key) & 1; }
static void clear(void) { eadk_display_push_rect_uniform(eadk_screen_rect, C(15, 18, 28)); }
static void garage_car(void) { car_init(mapId ? 0 : TA, 0, 0, 1, .02f); }
static void startpos(void) { car_init(mapId ? 0 : TA, 0, 0, 1, .4f); }
// ---- screens
static const char *MN[5] = {"Jouer", "Garage", "Carte", "Commandes", "Quitter"};
static void draw_menu(int sel) {
  clear(); txt("NumBeam 3D", 90, 24, 1, C(230, 60, 50), C(15, 18, 28)); txt("simulateur de collision", 82, 52, 0, C(150, 160, 190), C(15, 18, 28));
  for (int i = 0; i < 5; i++) { char s[32], *p = s; p = cat(p, MN[i]); if (i == 2) cat(p, mapId ? ": Route" : ": Circuit"); row(s, 90, 80 + i * 24, 140, i == sel); }
  txt("Haut/Bas + OK", 105, 214, 0, C(110, 120, 150), C(15, 18, 28));
}
static void draw_garage(int sel) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 160, 240}, C(15, 18, 28));
  const char *lab[9] = {"Vehicule", "Roues", "Susp.", "Moteur", "Chassis", "Capot", "Portes", "Coffre", "JOUER"};
  const char *val[8] = {VEH[cv].nm, WHL[cw].nm, SUS[cs].nm, ENG[ce].nm, CHA[cc].nm, tHood > .5f ? "ouvert" : "ferme", tDoor > .5f ? "ouvertes" : "fermees", tTrunk > .5f ? "ouvert" : "ferme"};
  for (int i = 0; i < 9; i++) { char s[32], *p = s; p = cat(p, lab[i]); if (i < 8) { p = cat(p, ": "); cat(p, val[i]); } row(s, 4, 6 + i * 21, 152, i == sel); }
  char s[32], *p = s; p = cat(p, "Puiss "); p = num(p, (int)(ENG[ce].a * VEH[cv].pw * 10)); p = cat(p, " Vmax "); num(p, (int)(ENG[ce].v * 3.6f));
  txt(s, 6, 202, 0, C(180, 190, 220), C(15, 18, 28));
  p = s; p = cat(p, "Grip "); p = num(p, (int)(WHL[cw].gr * VEH[cv].gr * 100)); p = cat(p, " Solid. "); num(p, (int)(CHA[cc].k / 140));
  txt(s, 6, 218, 0, C(180, 190, 220), C(15, 18, 28));
}
static void draw_pause(int sel) {
  const char *it[5] = {"Reprendre", "Recommencer", "Garage", "Commandes", "Menu principal"};
  eadk_display_push_rect_uniform((eadk_rect_t){70, 40, 180, 150}, C(15, 18, 28)); txt("PAUSE", 140, 46, 0, 0xFFFF, C(15, 18, 28));
  for (int i = 0; i < 5; i++) row(it[i], 80, 66 + i * 22, 160, i == sel);
}
static void draw_help(void) {
  clear(); txt("COMMANDES", 115, 8, 1, C(230, 60, 50), C(15, 18, 28));
  const char *l[9] = {"Haut / Bas : accelerer / freiner", "Gauche / Droite : tourner", "OK : remettre la voiture", "Boite a outils : poutres", "Var : capot   X,n,t : portes", "Exp : coffre", "Retour : pause", "Home : quitter l'app", "(une touche pour revenir)"};
  for (int i = 0; i < 9; i++) txt(l[i], 20, 40 + i * 20, 0, C(210, 215, 235), C(15, 18, 28));
}
static void hud(int kmh, int gear, int lap, int ms, int best) {
  char s[64], *p = s; p = num(p, kmh); p = cat(p, "km/h "); if (gear < 0) p = cat(p, "R"); else { p = cat(p, "G"); p = num(p, gear); }
  p = cat(p, "  Deg "); p = num(p, (int)dmg); p = cat(p, "%");
  if (mapId == 0) { p = cat(p, "  T"); p = num(p, lap); p = cat(p, " "); p = num(p, ms / 60000); p = cat(p, ":"); int sc = ms / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); p = cat(p, "."); p = num(p, ms / 100 % 10);
    if (best) { p = cat(p, " B"); p = num(p, best / 60000); p = cat(p, ":"); sc = best / 1000 % 60; if (sc < 10) p = cat(p, "0"); p = num(p, sc); } }
  eadk_display_push_rect_uniform((eadk_rect_t){0, 224, 320, 16}, C(10, 10, 14)); txt(s, 4, 225, 0, 0xFFFF, C(10, 10, 14));
}
int main(void) {
  int st = S_MENU, sel = 0, redraw = 1, gsel = 0, prev = S_MENU, fr = 0, lap = 1, best = 0, cp = 0;
  float acc = 0, stw = 0, orb = 0, pz = 0; uint64_t last = eadk_timing_millis(), lapT = last;
#ifdef TESTST
  st = TESTST; tHood = oHood = TESTO; tDoor = oDoor = TESTO; tTrunk = oTrunk = TESTO; cv = TESTV; ce = 3; cw = 1;
  if (st == S_GARAGE) garage_car(); else startpos();
#endif
  for (;;) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    int U = pressed(k, eadk_key_up), D = pressed(k, eadk_key_down), L = pressed(k, eadk_key_left), R = pressed(k, eadk_key_right), O = pressed(k, eadk_key_ok), B = pressed(k, eadk_key_back);
    if (st == S_MENU) {
      if (redraw) { draw_menu(sel); redraw = 0; }
      if (U) { sel = (sel + 4) % 5; redraw = 1; } if (D) { sel = (sel + 1) % 5; redraw = 1; }
      if ((L || R) && sel == 2) { mapId ^= 1; redraw = 1; }
      if (O) { if (sel == 0) { startpos(); st = S_GAME; lap = 1; best = 0; cp = 0; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
        else if (sel == 1) { garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 2) { mapId ^= 1; redraw = 1; } else if (sel == 3) { prev = S_MENU; st = S_HELP; redraw = 1; } else return 0; }
      eadk_timing_msleep(30);
    } else if (st == S_GARAGE) {
      if (redraw) { draw_garage(gsel); redraw = 0; }
      int ch = 0;
      if (U) { gsel = (gsel + 8) % 9; redraw = 1; } if (D) { gsel = (gsel + 1) % 9; redraw = 1; }
      if (L || R) { int d = R ? 1 : 3; redraw = 1; ch = 1;
        if (gsel == 0) cv = (cv + d) % 4; else if (gsel == 1) cw = (cw + d) % 4; else if (gsel == 2) cs = (cs + d) % 4; else if (gsel == 3) ce = (ce + d) % 4; else if (gsel == 4) cc = (cc + (R ? 1 : 2)) % 3;
        else if (gsel == 5) tHood = tHood > .5f ? 0 : 1; else if (gsel == 6) tDoor = tDoor > .5f ? 0 : 1; else if (gsel == 7) tTrunk = tTrunk > .5f ? 0 : 1; else ch = 0; }
      if (O && gsel >= 5 && gsel <= 7) { if (gsel == 5) tHood = tHood > .5f ? 0 : 1; if (gsel == 6) tDoor = tDoor > .5f ? 0 : 1; if (gsel == 7) tTrunk = tTrunk > .5f ? 0 : 1; redraw = 1; }
      if (ch && gsel < 5) garage_car();
      if (O && gsel == 8) { startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; eadk_display_push_rect_uniform(eadk_screen_rect, 0); }
      if (B) { st = S_MENU; redraw = 1; }
      if (st == S_GARAGE) {
        orb += .035f; oHood += (tHood - oHood) * .15f; oDoor += (tDoor - oDoor) * .15f; oTrunk += (tTrunk - oTrunk) * .15f;
        float cx = 0, cy = 0, cz = 0; for (int i = 0; i < NN; i++) { cx += n[i].x; cy += n[i].y; cz += n[i].z; } cx /= NN; cy /= NN; cz /= NN;
        camhx = fsin(orb); camhz = fsin(orb + 1.5708f); camx = cx - camhx * 11.f; camz = cz - camhz * 11.f; camy = cy + 3.4f; CXc = 120;
        terrain(80); car(); present(80);
      }
    } else if (st == S_GAME) {
      if (B) { st = S_PAUSE; sel = 0; redraw = 1; pk = k; continue; }
      CXc = 80;
      float cx = 0, cy = 0, cz = 0, vx = 0, vz = 0;
      for (int i = 0; i < NN; i++) { cx += n[i].x; cy += n[i].y; cz += n[i].z; vx += n[i].vx; vz += n[i].vz; }
      cx /= NN; cy /= NN; cz /= NN; vx /= NN; vz /= NN;
      float hx = n[16].x + n[17].x - n[14].x - n[15].x, hz = n[16].z + n[17].z - n[14].z - n[15].z, hl = fsqrt(hx * hx + hz * hz) + 1e-4f; hx /= hl; hz /= hl;
      float vf = vx * hx + vz * hz, sp = fsqrt(vx * vx + vz * vz);
      if (O) { car_init(cx, cz, hx, hz, .5f); }
      if (pressed(k, eadk_key_toolbox)) structure ^= 1;
      if (pressed(k, eadk_key_var)) tHood = tHood > .5f ? 0 : 1; if (pressed(k, eadk_key_xnt)) tDoor = tDoor > .5f ? 0 : 1; if (pressed(k, eadk_key_exp)) tTrunk = tTrunk > .5f ? 0 : 1;
      oHood += (tHood - oHood) * .2f; oDoor += (tDoor - oDoor) * .2f; oTrunk += (tTrunk - oTrunk) * .2f;
      thr = down(k, eadk_key_up) ? 1.f : 0; brk = 0;
      if (down(k, eadk_key_down)) { if (vf > 1.f) brk = 1; else thr = -.6f; }
      float tgt = (down(k, eadk_key_right) ? 1.f : 0) - (down(k, eadk_key_left) ? 1.f : 0);
      stw += (tgt * .45f / (1.f + sp * .06f) - stw) * .25f; steer = stw;
      uint64_t now = eadk_timing_millis(); acc += (float)(now - last); last = now; if (acc > 60) acc = 60;
      while (acc >= 3.5f) { step(.0035f); acc -= 3.5f; }
      if (mapId == 0) {                                 // lap counting: far side checkpoint, then cross the start line
        if (cx < -TA * .8f && cz > -20 && cz < 20) cp = 1;
        if (cp && pz < 0 && cz >= 0 && cx > 0) { int t = (int)(now - lapT); if (!best || t < best) best = t; lap++; lapT = now; cp = 0; }
        pz = cz;
      }
      camhx += (hx - camhx) * .1f; camhz += (hz - camhz) * .1f; float l = fsqrt(camhx * camhx + camhz * camhz) + 1e-4f; camhx /= l; camhz /= l;
      camx = cx - camhx * 8.5f; camz = cz - camhz * 8.5f;
      float gy = gh(camx, camz) + 1.4f, ty = cy + 2.6f; if (ty < gy) ty = gy; camy += (ty - camy) * .15f;
      terrain(0); car(); present(0);
      if (++fr % 6 == 0) { int g = 1 + (int)(sp / (engV / 5.2f)); if (g > 5) g = 5; hud((int)(sp * 3.6f), vf < -.5f ? -1 : g, lap, (int)(now - lapT), best); }
    } else if (st == S_PAUSE) {
      if (redraw) { draw_pause(sel); redraw = 0; }
      if (U) { sel = (sel + 4) % 5; redraw = 1; } if (D) { sel = (sel + 1) % 5; redraw = 1; }
      if (B) { sel = 0; O = 1; }
      if (O) { if (sel == 0) { st = S_GAME; last = eadk_timing_millis(); acc = 0; }
        else if (sel == 1) { startpos(); lap = 1; best = 0; cp = 0; st = S_GAME; lapT = eadk_timing_millis(); last = lapT; acc = 0; }
        else if (sel == 2) { garage_car(); st = S_GARAGE; redraw = 1; } else if (sel == 3) { prev = S_PAUSE; st = S_HELP; redraw = 1; } else { st = S_MENU; redraw = 1; } }
      eadk_timing_msleep(30);
    } else {
      if (redraw) { draw_help(); redraw = 0; }
      if (U || D || L || R || O || B) { if (prev == S_PAUSE) { st = S_GAME; eadk_display_push_rect_uniform(eadk_screen_rect, 0); last = eadk_timing_millis(); acc = 0; } else { st = S_MENU; redraw = 1; } }
      eadk_timing_msleep(30);
    }
    pk = k;
  }
}

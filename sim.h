// NumBeam physics: soft-body (node/beam) car, like BeamNG's engine but in 2D.
#include <stdint.h>
#define NN 11
#define MB 72
#define G 330.f
typedef struct { float x, y, vx, vy, im, r; uint8_t gnd; } Node;
typedef struct { uint8_t a, b, f; float l0, lr, k, c, yl; } Beam;  // f: 0 body, 1 suspension, 2 broken

static Node n[NN];
static Beam bm[MB];
static int nb;
static float thr, brk, pitch;       // inputs: throttle -1..1, brake 0..1, pitch -1..1
static float dmg;                   // accumulated deformation (0..100 %)

// hull (0..8) and wheels (9,10): rest positions, car 56 px long
static const float P[NN][2] = {
  {-26,-4},{-8,-4},{12,-4},{28,-4},{-26,-14},{-12,-24},{6,-24},{14,-14},{28,-12},{-18,2},{18,2}};

static float fsin(float x) {          // cheap sine, no libm needed
  x *= 0.15915494f; x -= (float)(int)x; if (x < 0) x += 1.f;
  x = x * 6.2831853f - 3.1415927f;
  float y = 1.2732395f * x - 0.4052847f * x * (x < 0 ? -x : x);
  y = 0.225f * (y * (y < 0 ? -y : y) - y) + y;
  return -y;
}
static float bump(float x, float c, float wl, float wr, float h) {
  float t = x < c ? (c - x) / wl : (x - c) / wr;
  if (t >= 1.f) return 0; t = 1.f - t * t; return h * t * t;
}
// ground height (screen-style y, down is +). Flat start, then hills, ramps and concrete walls.
static float gy(float x) {
  float a = x < 0 ? 0 : (x > 500 ? 1.f : x / 500.f);
  float h = 170 + a * (16 * fsin(x * 0.011f) + 9 * fsin(x * 0.037f + 2.f));
  float f = x / 2400.f; f = x - 2400.f * (float)(int)f; if (f < 0) f += 2400.f;
  if (x > 600)
    h -= bump(f, 700, 150, 16, 34) + bump(f, 1150, 26, 26, 36) + bump(f, 1500, 200, 200, 40)
       + bump(f, 1850, 24, 24, 46) + bump(f, 2150, 120, 20, 40);
  return h;
}
static float slope(float x) {
  float s = (gy(x + 1.5f) - gy(x - 1.5f)) / 3.f;
  return s > 7 ? 7 : (s < -7 ? -7 : s);
}
static float fsqrt(float v) { return __builtin_sqrtf(v); }

static void car_init(float x0) {
  float oy = gy(x0) - 10.f;
  for (int i = 0; i < NN; i++) {
    n[i] = (Node){x0 + P[i][0], oy + P[i][1], 0, 0, i >= 9 ? 0.4f : 1.f, i >= 9 ? 7.f : 1.5f, 0};
  }
  nb = 0; dmg = 0;
  for (int i = 0; i < NN; i++) for (int j = i + 1; j < NN; j++) {
    float dx = P[j][0] - P[i][0], dy = P[j][1] - P[i][1], d = fsqrt(dx * dx + dy * dy);
    int w = (i >= 9) + (j >= 9);
    if (w == 2 || d > (w ? 24.f : 30.f) || nb >= MB) continue;
    bm[nb++] = (Beam){i, j, w ? 1 : 0, d, d, w ? 6000.f : 15000.f, w ? 60.f : 40.f, 0.08f};
  }
}

static void step(float dt) {
  for (int i = 0; i < NN; i++) {                       // engine, brake, gravity
    Node *p = &n[i];
    p->vy += G * dt;
    if (i >= 9 && p->gnd) {
      float sp = slope(p->x), il = 1.f / fsqrt(1 + sp * sp);
      float v = p->vx;
      if (thr != 0 && !(thr > 0 && v > 430) && !(thr < 0 && v < -160)) {
        p->vx += thr * 520.f * p->im * dt * il; p->vy += thr * 520.f * sp * il * p->im * dt;
      }
      if (brk > 0) { p->vx *= 1.f - 0.06f * brk; p->vy *= 1.f - 0.06f * brk; }
    }
  }
  if (pitch != 0) {                                     // air control: pitch the car
    float cx = 0; for (int i = 0; i < 9; i++) cx += n[i].x; cx /= 9;
    for (int i = 0; i < 9; i++) n[i].vy += -pitch * (n[i].x - cx) * 2.4f * dt;
  }
  float tot = 0;
  for (int i = 0; i < nb; i++) {                        // beams: spring + damper + plasticity
    Beam *b = &bm[i]; if (b->f == 2) { tot += 0.45f; continue; }
    Node *p = &n[b->a], *q = &n[b->b];
    float dx = q->x - p->x, dy = q->y - p->y, L = fsqrt(dx * dx + dy * dy) + 1e-4f;
    float ux = dx / L, uy = dy / L;
    float rel = (q->vx - p->vx) * ux + (q->vy - p->vy) * uy;
    float F = (b->k * (L - b->l0) + b->c * rel) * dt;
    p->vx += F * ux * p->im; p->vy += F * uy * p->im;
    q->vx -= F * ux * q->im; q->vy -= F * uy * q->im;
    float s = (L - b->l0) / b->l0;
    if (b->f == 0) {
      if (s > b->yl) b->l0 += (s - b->yl) * b->l0 * 0.5f;
      else if (s < -b->yl) b->l0 += (s + b->yl) * b->l0 * 0.5f;
      if (s > 0.6f) b->f = 2;                          // tears apart
      float d = (b->l0 - b->lr) / b->lr; tot += d < 0 ? -d : d;
    } else if (s > 1.f) b->f = 2;
  }
  { float nd = tot * 100.f / 6.f; if (nd > dmg) dmg = nd; if (dmg > 100) dmg = 100; }
  for (int i = 0; i < NN; i++) {                        // integrate + ground contact
    Node *p = &n[i];
    p->vx *= 0.9997f; p->vy *= 0.9997f;
    if (p->vx > 700) p->vx = 700; if (p->vx < -700) p->vx = -700; if (p->vy > 900) p->vy = 900; if (p->vy < -700) p->vy = -700;
    p->x += p->vx * dt; p->y += p->vy * dt;
    p->gnd = 0;
    float g = gy(p->x), pen = p->y + p->r - g;
    if (pen > 0) {
      float s = slope(p->x), il = 1.f / fsqrt(1 + s * s);
      float nx = -s * il, ny = -il, d = pen * il; if (d > 4.f) d = 4.f;
      p->x += nx * d; p->y += ny * d; p->gnd = 1;
      float vn = p->vx * nx + p->vy * ny;
      if (vn < 0) { p->vx -= vn * nx * 1.1f; p->vy -= vn * ny * 1.1f; }
      float tx = -ny, ty = nx, vt = p->vx * tx + p->vy * ty;
      float fr = i >= 9 ? 0.0004f : 0.05f;             // rolling wheels vs scraping metal
      p->vx -= vt * tx * fr; p->vy -= vt * ty * fr;
    }
  }
}

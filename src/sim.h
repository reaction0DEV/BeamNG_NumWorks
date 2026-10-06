// NumBeam 3D v2 physics. x = right, y = up, z = forward.
#include <stdint.h>
#define NN 20
#define MB 150
#define G 14.f
typedef struct { float x, y, z, vx, vy, vz, im, r; uint8_t gnd; } Node;
typedef struct { uint8_t a, b, f; float l0, lr, k, c, yl, bk; } Beam;   // f: 0 body, 1 suspension, 2 broken
static Node n[NN]; static Beam bm[MB]; static int nb, nbody;
static float thr, brk, steer, dmg, gfeat, gdist, engA = 10, engV = 38, latG = .25f;
static int mapId;                                      // 0 circuit, 1 road with ramps
// ---- customisation tables
typedef struct { const char *nm; float sx, sy, sz, im, pw, gr; uint8_t r, g, b; } Veh;
static const Veh VEH[4] = {{"Berline",1,1,1,1,1,1,200,35,30},{"Sport",1.05f,.8f,1.12f,1.1f,1.15f,1.12f,40,90,210},
  {"Pick-up",1.12f,1.25f,1.15f,.8f,.95f,.85f,230,170,30},{"Buggy",.8f,.85f,.7f,1.3f,1.1f,1.15f,60,180,70}};
typedef struct { const char *nm; float r, gr, im; uint8_t cr, cg, cb; } Whl;
static const Whl WHL[4] = {{"Route",.36f,.25f,.4f,170,170,175},{"Sport",.34f,.34f,.45f,230,200,60},{"Tout-terrain",.43f,.2f,.33f,90,90,95},{"Mini",.27f,.3f,.5f,220,60,60}};
typedef struct { const char *nm; float k, c; } Sus;
static const Sus SUS[4] = {{"Souple",3500,45},{"Normale",5000,60},{"Ferme",8000,80},{"Course",12000,100}};
typedef struct { const char *nm; float a, v; uint8_t r, g, b; } Eng;
static const Eng ENG[4] = {{"4 cyl",7.5f,30,150,150,155},{"V6",10,38,60,110,220},{"V8",13,46,230,190,60},{"Turbo",17,55,230,60,60}};
typedef struct { const char *nm; float k, yl, bk, im; } Cha;
static const Cha CHA[3] = {{"Acier",14000,.08f,.6f,1.f},{"Renforce",20000,.12f,.9f,.8f},{"Leger",9000,.05f,.45f,1.25f}};
static int cv, cw, cs = 1, ce = 1, cc;                 // chosen vehicle, wheels, suspension, engine, chassis
static const float P[NN][3] = {
  {-.9,.35,-2},{.9,.35,-2},{-.9,.35,2},{.9,.35,2},{-.9,.35,0},{.9,.35,0},
  {-.9,.85,-2},{.9,.85,-2},{-.9,.85,2},{.9,.85,2},
  {-.8,1.4,-.9},{.8,1.4,-.9},{-.8,1.4,.7},{.8,1.4,.7},
  {-1.05,.35,-1.3},{1.05,.35,-1.3},{-1.05,.35,1.3},{1.05,.35,1.3},
  {-.9,.85,.8},{.9,.85,.8}};
static float fsin(float x) {
  x *= 0.15915494f; x -= (float)(int)x; if (x < 0) x += 1.f;
  x = x * 6.2831853f - 3.1415927f;
  float y = 1.2732395f * x - 0.4052847f * x * (x < 0 ? -x : x);
  y = 0.225f * (y * (y < 0 ? -y : y) - y) + y; return -y;
}
static float fsqrt(float v) { return __builtin_sqrtf(v); }
static float bump(float x, float c, float wl, float wr, float h) {
  float t = x < c ? (c - x) / wl : (x - c) / wr; if (t >= 1.f) return 0; t = 1.f - t * t; return h * t * t;
}
#define TA 130.f
#define TB 85.f
static float gh(float x, float z) {
  float h, ft = 0;
  if (mapId == 0) {                                    // oval circuit with barriers and hills around
    float rho = fsqrt(x * x / (TA * TA) + z * z / (TB * TB)), g2 = fsqrt(x * x / (TA * TA * TA * TA) + z * z / (TB * TB * TB * TB)) + 1e-6f;
    float d = rho < .15f ? -60.f : (rho - 1.f) * rho / g2, ad = d < 0 ? -d : d;
    gdist = d;
    float side = (ad - 12.f) / 25.f; side = side < 0 ? 0 : (side > 1 ? 1 : side);
    h = side * (d > 0 ? 1.f : .35f) * (6.f * fsin(x * .07f + z * .04f) + 4.f * fsin(z * .09f - x * .03f));
    if (d > 0 && (((int)((x + z) * .035f + 1000)) & 7) != 0) ft = bump(d, 8.f, .6f, .6f, 1.3f);
  } else {                                             // straight road along z with ramps, walls, hills
    float ax = x < 0 ? -x : x, side = (ax - 7.f) / 20.f; side = side < 0 ? 0 : (side > 1 ? 1 : side);
    h = side * (5.f * fsin(x * .09f + z * .03f) + 3.f * fsin(z * .11f + x * .05f));
    float wx = (8.f - ax) / 3.f; wx = wx < 0 ? 0 : (wx > 1 ? 1 : wx);
    float f = z / 300.f; f = z - 300.f * (float)(int)f;
    if (z > 40.f && wx > 0) ft = wx * (bump(f, 80, 18, 2.5f, 2.2f) + bump(f, 150, 2.5f, 2.5f, 1.7f) + bump(f, 230, 30, 30, 2.5f));
    gdist = ax;
  }
  gfeat = ft; return h + ft;
}
static void car_init(float x0, float z0, float hx, float hz, float lift) {
  const Veh *V = &VEH[cv]; const Whl *W = &WHL[cw]; const Sus *S = &SUS[cs]; const Cha *H = &CHA[cc];
  float rx = hz, rz = -hx, oy = gh(x0, z0) + lift;
  engA = ENG[ce].a * V->pw; engV = ENG[ce].v; latG = W->gr * V->gr; if (latG > .45f) latG = .45f;
  for (int i = 0; i < NN; i++) {
    int w = i >= 14 && i < 18;
    float qx = P[i][0] * V->sx, qy = w ? W->r : P[i][1] * V->sy, qz = P[i][2] * V->sz;
    n[i] = (Node){x0 + rx * qx + hx * qz, oy + qy, z0 + rz * qx + hz * qz, 0, 0, 0, w ? W->im : V->im * H->im, w ? W->r : .15f, 0};
  }
  nb = 0; nbody = 0; dmg = 0;
  for (int i = 0; i < NN; i++) for (int j = i + 1; j < NN; j++) {
    float dx = P[j][0] - P[i][0], dy = P[j][1] - P[i][1], dz = P[j][2] - P[i][2], d = fsqrt(dx * dx + dy * dy + dz * dz);
    int w = (i >= 14 && i < 18) + (j >= 14 && j < 18);
    if (w == 2 || d > (w ? 1.9f : 2.35f) || nb >= MB) continue;
    float ex = n[j].x - n[i].x, ey = n[j].y - n[i].y, ez = n[j].z - n[i].z, L = fsqrt(ex * ex + ey * ey + ez * ez);
    bm[nb++] = (Beam){i, j, w ? 1 : 0, L, L, w ? S->k : H->k, w ? S->c : 40.f, H->yl, H->bk};
    if (!w) nbody++;
  }
}
static void step(float dt) {
  float hx = (n[16].x + n[17].x - n[14].x - n[15].x), hz = (n[16].z + n[17].z - n[14].z - n[15].z), hl = fsqrt(hx * hx + hz * hz) + 1e-4f;
  hx /= hl; hz /= hl;
  float s = steer, c = fsin(s + 1.5708f);
  for (int i = 0; i < NN; i++) n[i].vy -= G * dt;
  float tot = 0;
  for (int i = 0; i < nb; i++) {
    Beam *b = &bm[i]; if (b->f == 2) { tot += .45f; continue; }
    Node *p = &n[b->a], *q = &n[b->b];
    float dx = q->x - p->x, dy = q->y - p->y, dz = q->z - p->z, L = fsqrt(dx * dx + dy * dy + dz * dz) + 1e-4f;
    float ux = dx / L, uy = dy / L, uz = dz / L;
    float F = (b->k * (L - b->l0) + b->c * ((q->vx - p->vx) * ux + (q->vy - p->vy) * uy + (q->vz - p->vz) * uz)) * dt;
    p->vx += F * ux * p->im; p->vy += F * uy * p->im; p->vz += F * uz * p->im;
    q->vx -= F * ux * q->im; q->vy -= F * uy * q->im; q->vz -= F * uz * q->im;
    float e = (L - b->l0) / b->l0;
    if (b->f == 0) {
      if (e > b->yl) b->l0 += (e - b->yl) * b->l0 * .5f; else if (e < -b->yl) b->l0 += (e + b->yl) * b->l0 * .5f;
      if (e > b->bk) b->f = 2;
      float d = (b->l0 - b->lr) / b->lr; tot += d < 0 ? -d : d;
    } else if (e > 1.f) b->f = 2;
  }
  { float nd = tot * 100.f / (nbody * .21f); if (nd > dmg) dmg = nd; if (dmg > 100) dmg = 100; }
  for (int i = 0; i < NN; i++) {
    Node *p = &n[i];
    p->vx *= .9997f; p->vy *= .9997f; p->vz *= .9997f;
    p->x += p->vx * dt; p->y += p->vy * dt; p->z += p->vz * dt;
    p->gnd = 0;
    float g = gh(p->x, p->z), pen = g + p->r - p->y;
    if (pen > 0) {
      float gx = (gh(p->x + .3f, p->z) - gh(p->x - .3f, p->z)) / .6f, gz = (gh(p->x, p->z + .3f) - gh(p->x, p->z - .3f)) / .6f;
      float il = 1.f / fsqrt(1 + gx * gx + gz * gz), nx = -gx * il, ny = il, nz = -gz * il, d = pen * il; if (d > .25f) d = .25f;
      p->x += nx * d; p->y += ny * d; p->z += nz * d; p->gnd = 1;
      float vn = p->vx * nx + p->vy * ny + p->vz * nz;
      if (vn < 0) { p->vx -= vn * nx * 1.1f; p->vy -= vn * ny * 1.1f; p->vz -= vn * nz * 1.1f; }
      if (i >= 14 && i < 18) {
        float fx = hx, fz = hz;
        if (i >= 16) { fx = hx * c + hz * s; fz = hz * c - hx * s; }
        float fn = fx * nx + fz * nz, tx = fx - nx * fn, ty = -ny * fn, tz = fz - nz * fn;
        float lx = ny * tz - nz * ty, ly = nz * tx - nx * tz, lz = nx * ty - ny * tx;
        float ll = fsqrt(lx * lx + ly * ly + lz * lz) + 1e-4f; lx /= ll; ly /= ll; lz /= ll;
        float vl = p->vx * lx + p->vy * ly + p->vz * lz; p->vx -= lx * vl * latG; p->vy -= ly * vl * latG; p->vz -= lz * vl * latG;
        float vf = p->vx * tx + p->vy * ty + p->vz * tz;
        if (thr != 0 && !(thr > 0 && vf > engV) && !(thr < 0 && vf < -10)) { float a = thr * engA * dt; p->vx += tx * a; p->vy += ty * a; p->vz += tz * a; }
        if (brk > 0) { p->vx *= 1.f - .05f * brk; p->vy *= 1.f - .05f * brk; p->vz *= 1.f - .05f * brk; }
      } else { p->vx *= .96f; p->vz *= .96f; }
    }
  }
}

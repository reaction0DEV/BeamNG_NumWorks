// NumBeam 3D v3 physics. x = right, y = up, z = forward.
// Car nodes 0-19 structure, 20-23 hood, 24-27 front bumper, 28-31 rear bumper; then trailer (11), then objects (8 nodes each).
#include <stdint.h>
#define NC 60
#define NMAX 124
#define MB 500
#define NG 10
#define MAX_AI 64
static float G = 14.f, gripF = 1.f; static int turbo;
typedef struct { float x, y, z, vx, vy, vz, im, r, rc; uint8_t gnd, t; } Node;   // t: 0 body, 1 car wheel, 2 trailer wheel
typedef struct { uint8_t a, b, f, t0, o, g; float l0, lr, k, c, yl, bk; } Beam;  // f: 0 body 1 susp 2 broken 3 hitch 4 panel attach; o: 0 car 1 trailer 2 object
static Node n[NMAX]; static Beam bm[MB];
static int nn, nb, nbody, nbc, ntr = -1, nob0, nobj, pAtt[NG], pAtt0[NG], wAtt[4], trl, solid = 1, mapId;
typedef struct { float x, z, phase, speed, hx, hz; } TrafficCar;
static TrafficCar traffic[MAX_AI]; static int trafficEnabled, trafficCount = 2, trafficBehavior, trafficSpeed = 2;
static TrafficCar policeCar; static int pursuitEnabled, cityPlan, weatherMode;
static TrafficCar policeCar; static int pursuitEnabled, cityPlan, weatherMode;
static float tunePower = 1.f, tuneGrip = 1.f, tuneSusp = 1.f, tuneBrake = 1.f, signalClock;
static const uint8_t PB[NG] = {22, 26, 30, 34, 38, 42, 46, 50, 54, 57}, PN[NG] = {4, 4, 4, 4, 4, 4, 4, 4, 3, 3};   // hood, front bumper, rear bumper, trunk lid, doors L/R, windshield, engine, seats L/R
static const char *PNAME[NG] = {"Capot", "Pare-ch.AV", "Pare-ch.AR", "Coffre", "Porte G", "Porte D", "Pare-brise", "Moteur", "Siege G", "Siege D"};
static float thr, brk, steer, dmg, gfeat, gdist, engA = 10, engV = 38, engineRpm = 900, engineOutput = .45f, latG = .25f, gsx = 1, gsz = 1;
static int gearNow = 1;
static const float SOLF[3] = {1.6f, 1.f, .65f};
typedef struct { const char *nm; float sx, sy, sz, im, pw, gr; uint8_t r, g, b; } Veh;
#define NV 5
static char genName[16] = "Gen #1";
static Veh VEH[NV] = {{"Berline",1,1,1,1,1,1,200,35,30},{"Sport",1.05f,.8f,1.12f,1.1f,1.15f,1.12f,40,90,210},
  {"Pick-up",1.12f,1.25f,1.15f,.8f,.95f,.85f,230,170,30},{"Buggy",.8f,.85f,.7f,1.3f,1.1f,1.15f,60,180,70},{genName,1,1,1,1,1,1,150,150,150}};
static unsigned genSeed = 1, genState;
static float gen_random(void) { genState = genState * 1664525u + 1013904223u; return (genState >> 8) / 16777215.f; }
static void gen_car(unsigned seed) {
  Veh *v = &VEH[4]; genSeed = seed ? seed : 1; genState = genSeed * 2654435761u + 12345u;
  v->sx = .82f + gen_random() * .42f; v->sy = .78f + gen_random() * .52f; v->sz = .76f + gen_random() * .5f;
  float mass = v->sx * v->sy * v->sz; v->im = 1.f / mass; if (v->im < .65f) v->im = .65f; if (v->im > 1.45f) v->im = 1.45f;
  v->pw = .82f + gen_random() * .48f; v->gr = .82f + gen_random() * .38f;
  unsigned hue = (unsigned)(gen_random() * 1536.f), sector = hue / 256, f = hue & 255;
  unsigned q = 255u * (255u - f) / 255u, t = 255u * f / 255u;
  switch (sector % 6) { case 0: v->r=230; v->g=t; v->b=35; break; case 1: v->r=q; v->g=230; v->b=35; break; case 2: v->r=35; v->g=230; v->b=t; break; case 3: v->r=35; v->g=q; v->b=230; break; case 4: v->r=t; v->g=35; v->b=230; break; default: v->r=230; v->g=35; v->b=q; }
  genName[0] = 'G'; genName[1] = 'e'; genName[2] = 'n'; genName[3] = ' '; genName[4] = '#';
  char digits[6]; int count = 0; unsigned nseed = genSeed;
  do { digits[count++] = (char)('0' + nseed % 10); nseed /= 10; } while (nseed && count < 5);
  int out = 5; while (count) genName[out++] = digits[--count]; genName[out] = 0;
  v->nm = genName;
}
typedef struct { const char *nm; float r, gr, im; uint8_t cr, cg, cb; } Whl;
static const Whl WHL[4] = {{"Route",.36f,.25f,.4f,170,170,175},{"Sport",.34f,.34f,.45f,230,200,60},{"Tout-terrain",.43f,.2f,.33f,90,90,95},{"Mini",.27f,.3f,.5f,220,60,60}};
typedef struct { const char *nm; float k, c; } Sus;
static const Sus SUS[4] = {{"Souple",3500,45},{"Normale",5000,60},{"Ferme",8000,80},{"Course",12000,100}};
typedef struct { const char *nm; float a, v; uint8_t r, g, b; } Eng;
static const Eng ENG[4] = {{"4 cyl",7.5f,30,150,150,155},{"V6",10,38,60,110,220},{"V8",13,46,230,190,60},{"Turbo",17,55,230,60,60}};
typedef struct { const char *nm; float k, yl, bk, im; } Cha;
static const Cha CHA[3] = {{"Acier",14000,.08f,.6f,1.f},{"Renforce",20000,.12f,.9f,.8f},{"Leger",9000,.05f,.45f,1.25f}};
// objects: size, node inverse mass, stiffness, yield, break, car-collision radius, colour
typedef struct { const char *nm; float sx, sy, sz, im, k, yl, bk, rc; uint8_t r, g, b; } Obj;
static const Obj OBJ[4] = {{"Cone",.5f,.8f,.5f,4.f,1500,.25f,1.5f,.4f,240,110,20},{"Caisse bois",1,1,1,1.6f,5000,.06f,.35f,.6f,170,120,60},
  {"Caisse acier",1,1,1,.9f,12000,.12f,.8f,.6f,110,130,160},{"Plot beton",1.2f,.9f,.6f,.25f,20000,.2f,1.2f,.5f,185,185,180}};
static uint8_t okind[6];
static int cv, cw, cs = 1, ce = 1, cc;
static const float P[NC][3] = {
  {-.9,.35,-2},{.9,.35,-2},{-.9,.35,2},{.9,.35,2},{-.9,.35,0},{.9,.35,0},
  {-.9,.85,-2},{.9,.85,-2},{-.9,.85,2},{.9,.85,2},
  {-.8,1.4,-.9},{.8,1.4,-.9},{-.8,1.4,.7},{.8,1.4,.7},
  {-1.05,.35,-1.3},{1.05,.35,-1.3},{-1.05,.35,1.3},{1.05,.35,1.3},
  {-.9,.85,.8},{.9,.85,.8},
  {-.85,.95,-1.3},{.85,.95,-1.3},                                      // 20-21 rear deck line
  {-.9,.9,2},{.9,.9,2},{.9,.9,.8},{-.9,.9,.8},                         // 22 hood: front-left, front-right, back-right, back-left
  {-.88,.28,2.3},{.88,.28,2.3},{.88,.62,2.3},{-.88,.62,2.3},           // 26 front bumper
  {-.88,.28,-2.3},{.88,.28,-2.3},{.88,.62,-2.3},{-.88,.62,-2.3},       // 30 rear bumper
  {-.9,.86,-2},{.9,.86,-2},{.9,.96,-1.3},{-.9,.96,-1.3},               // 34 trunk lid: rear-left, rear-right, front-right, front-left
  {-.91,.36,-.85},{-.91,.36,.75},{-.91,1.32,.62},{-.91,1.32,-.8},     // 38 door L: rear-bottom, front-bottom, front-top, rear-top
  {.91,.36,-.85},{.91,.36,.75},{.91,1.32,.62},{.91,1.32,-.8},          // 42 door R
  {-.84,.9,.85},{.84,.9,.85},{.78,1.37,.68},{-.78,1.37,.68},           // 46 windshield
  {-.35,.4,1.05},{.35,.4,1.05},{-.35,.4,1.85},{-.35,.84,1.05},         // 50 engine: origin, +x, +z (front), +y
  {-.7,.5,-.45},{-.2,.5,-.45},{-.7,.5,.05},                            // 54 seat L: rear-left, rear-right, front-left
  {.2,.5,-.45},{.7,.5,-.45},{.2,.5,.05}};                              // 57 seat R
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
#define WALLZ 60.f
#define CITY_X_LIMIT 56.f
#define CITY_Z_LIMIT 72.f
static float city_height(int ix, int iz) { return 9.f + ((ix * 7 + iz * 11 + 200) & 3) * 4.f; }
static int city_round(float v) { return (int)(v + (v >= 0 ? .5f : -.5f)); }
static void city_center(int ix, int iz, float *x, float *z) {
  *x = ix * 28.f + 14.f; *z = iz * 36.f + 18.f;
  if (cityPlan == 1) *x += (iz & 1) ? 5.f : -5.f;
  else if (cityPlan == 2) *z += (ix & 1) ? 5.f : -5.f;
}
static void city_nearest(float x, float z, int *ix, int *iz) {
  float best = 1e9f; *ix = city_round((x - 14.f) / 28.f); *iz = city_round((z - 18.f) / 36.f);
  for (int a = -3; a <= 3; a++) for (int b = -3; b <= 3; b++) {
    float cx, cz; city_center(a, b, &cx, &cz); float dx = x - cx, dz = z - cz, d = dx * dx + dz * dz;
    if (d < best) { best = d; *ix = a; *iz = b; }
  }
}
static float gh(float x, float z) {
  float h, ft = 0, ax = x < 0 ? -x : x;
  if (mapId == 3) { gdist = ax; gfeat = 0; return 0; }
  if (mapId == 0) {
    float rho = fsqrt(x * x / (TA * TA) + z * z / (TB * TB)), g2 = fsqrt(x * x / (TA * TA * TA * TA) + z * z / (TB * TB * TB * TB)) + 1e-6f;
    float d = rho < .15f ? -60.f : (rho - 1.f) * rho / g2, ad = d < 0 ? -d : d;
    gdist = d;
    float side = (ad - 12.f) / 25.f; side = side < 0 ? 0 : (side > 1 ? 1 : side);
    h = side * (d > 0 ? 1.f : .35f) * (6.f * fsin(x * .07f + z * .04f) + 4.f * fsin(z * .09f - x * .03f));
    if (d > 0 && (((int)((x + z) * .035f + 1000)) & 7) != 0) ft = bump(d, 8.f, .6f, .6f, 1.3f);
  } else {
    float lim = mapId == 2 ? 22.f : 7.f, side = (ax - lim) / 20.f; side = side < 0 ? 0 : (side > 1 ? 1 : side);
    h = side * (5.f * fsin(x * .09f + z * .03f) + 3.f * fsin(z * .11f + x * .05f));
    float wx = ((mapId == 2 ? 13.f : 8.f) - ax) / 3.f; wx = wx < 0 ? 0 : (wx > 1 ? 1 : wx);
    if (mapId == 2) ft = wx * bump(z, WALLZ, .45f, 3.f, 5.5f);
    else { float f = z / 300.f; f = z - 300.f * (float)(int)f;
      if (z > 40.f && wx > 0) ft = wx * (bump(f, 80, 18, 2.5f, 2.2f) + bump(f, 150, 2.5f, 2.5f, 1.7f) + bump(f, 230, 30, 30, 2.5f)); }
    gdist = ax;
  }
  gfeat = ft; return h + ft;
}
static void heading(float *hx, float *hz);
static void traffic_update(float dt);
static void traffic_reset(void) {
  static const float SPEED[5] = {4.5f, 7.f, 9.f, 12.f, 15.f};
  for (int i = 0; i < MAX_AI; i++) { traffic[i].phase = i * (200.f / (trafficCount > 0 ? trafficCount : 1)); traffic[i].speed = SPEED[trafficSpeed] * (.9f + (i % 5) * .05f); traffic[i].x = traffic[i].z = traffic[i].hx = traffic[i].hz = 0; }
  policeCar.x = (n[4].x + n[5].x) * .5f; policeCar.z = (n[4].z + n[5].z) * .5f - 12.f; policeCar.hx = 0; policeCar.hz = 1;
  traffic_update(0.f);
}
static void traffic_update(float dt) {
  if (mapId != 3) return;
  float px = (n[4].x + n[5].x) * .5f, pz = (n[4].z + n[5].z) * .5f, hx, hz; heading(&hx, &hz);
  if (trafficEnabled) for (int i = 0; i < trafficCount; i++) {
    TrafficCar *car = &traffic[i];
    if (trafficBehavior == 1) {
      float distance = 5.f + (i / 4) * 2.5f, side = ((i % 4) - 1.5f) * 2.f, rx = hz, rz = -hx;
      float tx = px - hx * distance + rx * side, tz = pz - hz * distance + rz * side;
      if (tx < -CITY_X_LIMIT + 3.f) tx = -CITY_X_LIMIT + 3.f; if (tx > CITY_X_LIMIT - 3.f) tx = CITY_X_LIMIT - 3.f;
      if (tz < -CITY_Z_LIMIT + 3.f) tz = -CITY_Z_LIMIT + 3.f; if (tz > CITY_Z_LIMIT - 3.f) tz = CITY_Z_LIMIT - 3.f;
      float blend = dt <= 0 ? 1.f : dt * 1.8f; if (blend > .12f) blend = .12f;
      car->x += (tx - car->x) * blend; car->z += (tz - car->z) * blend; car->hx = hx; car->hz = hz; continue;
    }
    float loop = trafficBehavior == 2 ? 148.f : 408.f;
    car->phase += car->speed * (trafficBehavior == 2 ? .7f : 1.f) * dt; while (car->phase >= loop) car->phase -= loop;
    float p = car->phase;
    if (trafficBehavior == 2) {
      if (p < 60.f) { car->x = -7.f; car->z = -30.f + p; car->hx = 0; car->hz = 1; }
      else if (p < 74.f) { car->x = -7.f + p - 60.f; car->z = 30.f; car->hx = 1; car->hz = 0; }
      else if (p < 134.f) { car->x = 7.f; car->z = 30.f - (p - 74.f); car->hx = 0; car->hz = -1; }
      else { car->x = 7.f - (p - 134.f); car->z = -30.f; car->hx = -1; car->hz = 0; }
    } else if (p < 120.f) { car->x = -42.f; car->z = -60.f + p; car->hx = 0; car->hz = 1; }
    else if (p < 204.f) { car->x = -42.f + p - 120.f; car->z = 60.f; car->hx = 1; car->hz = 0; }
    else if (p < 324.f) { car->x = 42.f; car->z = 60.f - (p - 204.f); car->hx = 0; car->hz = -1; }
    else { car->x = 42.f - (p - 324.f); car->z = -60.f; car->hx = -1; car->hz = 0; }
  }
  if (pursuitEnabled) {
    float dx = px - policeCar.x, dz = pz - policeCar.z, distance = fsqrt(dx * dx + dz * dz) + .001f;
    float tx = dx / distance, tz = dz / distance, rate = (dt <= 0.f) ? 1.f : dt * 1.6f; if (rate > .08f) rate = .08f;
    policeCar.hx += (tx - policeCar.hx) * rate; policeCar.hz += (tz - policeCar.hz) * rate;
    float len = fsqrt(policeCar.hx * policeCar.hx + policeCar.hz * policeCar.hz) + .001f; policeCar.hx /= len; policeCar.hz /= len;
    float speed = distance > 8.f ? 11.f : 5.f; policeCar.x += policeCar.hx * speed * dt; policeCar.z += policeCar.hz * speed * dt;
    if (policeCar.x < -CITY_X_LIMIT + 3.f) policeCar.x = -CITY_X_LIMIT + 3.f; if (policeCar.x > CITY_X_LIMIT - 3.f) policeCar.x = CITY_X_LIMIT - 3.f;
    if (policeCar.z < -CITY_Z_LIMIT + 3.f) policeCar.z = -CITY_Z_LIMIT + 3.f; if (policeCar.z > CITY_Z_LIMIT - 3.f) policeCar.z = CITY_Z_LIMIT - 3.f;
  }
  signalClock += dt;
}
static void add_beam(int a, int b, int f, int o, int g, float k, float c, float yl, float bk) {
  if (nb >= MB) return;
  float dx = n[b].x - n[a].x, dy = n[b].y - n[a].y, dz = n[b].z - n[a].z, L = fsqrt(dx * dx + dy * dy + dz * dz);
  bm[nb++] = (Beam){a, b, f, f, o, g, L, L, k, c, yl, bk};
}
static void heading(float *hx, float *hz) {
  static const uint8_t FR_[8] = {2, 3, 8, 9, 12, 13, 18, 19}, RR_[6] = {0, 1, 6, 7, 10, 11};
  float x = 0, z = 0; for (int i = 0; i < 8; i++) { x += n[FR_[i]].x / 8; z += n[FR_[i]].z / 8; } for (int i = 0; i < 6; i++) { x -= n[RR_[i]].x / 6; z -= n[RR_[i]].z / 6; }
  float l = fsqrt(x * x + z * z) + 1e-4f; *hx = x / l; *hz = z / l;
}
static void add_box(int kind, float x, float z) {
  if (nn + 8 > NMAX || nobj >= 6) return;
  const Obj *O = &OBJ[kind]; float SF = SOLF[solid], g = gh(x, z); int base = nn;
  for (int i = 0; i < 8; i++)
    n[nn++] = (Node){x + ((i & 1) - .5f) * O->sx, g + .1f + ((i >> 1) & 1) * O->sy, z + ((i >> 2) - .5f) * O->sz, 0, 0, 0, O->im, .1f, O->rc, 0, 3};
  for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) add_beam(base + i, base + j, 0, 2, 0, O->k, 25.f, O->yl * SF, O->bk * SF);
  okind[nobj++] = kind;
}
static void ep(float th, float off, float *x, float *z) { *x = TA * fsin(th + 1.5708f) + off; *z = TB * fsin(th); }
static void spawn_objects(void) {
  float x, z; nob0 = nn; nobj = 0;
  if (mapId == 0) {
    for (int i = 0; i < 3; i++) { ep(.15f + .15f * i, i & 1 ? 2.5f : -2.5f, &x, &z); add_box(0, x, z); }
    ep(.95f, 1.f, &x, &z); add_box(1, x, z); ep(.95f, 3.2f, &x, &z); add_box(2, x, z); ep(1.25f, -2.f, &x, &z); add_box(3, x, z);
  } else if (mapId == 1) {
    for (int i = 0; i < 3; i++) add_box(0, i & 1 ? 2.5f : -2.5f, 40.f + i * 16.f);
    add_box(1, -1.2f, 125.f); add_box(2, 1.2f, 125.f); add_box(3, 0, 170.f);
  }
}
static void car_init(float x0, float z0, float hx, float hz, float lift, int objs) {
  const Veh *V = &VEH[cv]; const Whl *W = &WHL[cw]; const Sus *S = &SUS[cs]; const Cha *H = &CHA[cc]; float SF = SOLF[solid];
  float rx = hz, rz = -hx, oy = gh(x0, z0) + lift;
  engA = ENG[ce].a * V->pw * tunePower; engV = ENG[ce].v; gearNow = 1; engineRpm = 900.f; engineOutput = .45f; latG = W->gr * V->gr * tuneGrip; if (latG > .45f) latG = .45f; gsx = V->sx; gsz = V->sz;
  for (int i = 0; i < NC; i++) {
    int w = i >= 14 && i < 18, pn = i >= 22;
    float qx = P[i][0] * V->sx, qy = w ? W->r : P[i][1] * V->sy, qz = P[i][2] * V->sz;
    n[i] = (Node){x0 + rx * qx + hx * qz, oy + qy, z0 + rz * qx + hz * qz, 0, 0, 0, w ? W->im : (pn ? (i >= 50 && i < 54 ? .5f : (i >= 54 ? 1.2f : 2.f)) : V->im * H->im), w ? W->r : (pn ? .08f : .15f), 0, 0, (uint8_t)w};
  }
  nn = NC; nb = 0; nbody = 0; dmg = 0; ntr = -1; nobj = 0;
  for (int i = 0; i < 22; i++) for (int j = i + 1; j < 22; j++) {
    float dx = P[j][0] - P[i][0], dy = P[j][1] - P[i][1], dz = P[j][2] - P[i][2], d = fsqrt(dx * dx + dy * dy + dz * dz);
    int w = (i >= 14 && i < 18) + (j >= 14 && j < 18);
    if (w == 2 || d > (w ? 1.9f : 2.35f)) continue;
    add_beam(i, j, w ? 1 : 0, 0, w ? (i >= 14 && i < 18 ? i - 14 : j - 14) : 0, w ? S->k * tuneSusp : H->k, w ? S->c * tuneSusp : 95.f, H->yl * SF, w ? .4f * SF : H->bk * SF);
    if (!w) nbody++;
  }
  static const float BK[NG] = {.09f, .07f, .07f, .09f, .10f, .10f, .08f, .20f, .22f, .22f};
  for (int g = 0; g < NG; g++) {                      // detachable parts: internal beams + weak attachments to the structure
    int b0 = PB[g], cnt = PN[g]; pAtt[g] = 0;
    for (int i = 0; i < cnt; i++) for (int j = i + 1; j < cnt; j++) add_beam(b0 + i, b0 + j, 0, 0, 0, g == 7 ? 12000 : 6000, 30, .1f * SF, .5f);
    for (int i = 0; i < cnt; i++) { uint32_t used = 0;
      for (int r = 0; r < 3; r++) {
        int bi = -1; float bd = 1e9f;
        for (int q = 0; q < 22; q++) { if ((used >> q) & 1) continue;
          float dx = P[q][0] - P[b0 + i][0], dy = P[q][1] - P[b0 + i][1], dz = P[q][2] - P[b0 + i][2], d = dx * dx + dy * dy + dz * dz;
          if (d > .09f && d < bd && d < 1.9f) { bd = d; bi = q; } }
        if (bi < 0) break; used |= 1u << bi;
        add_beam(b0 + i, bi, 4, 0, g, 2500, 30, 0, BK[g] * SF); pAtt[g]++;
      } }
    pAtt0[g] = pAtt[g];
  }
  for (int w = 0; w < 4; w++) { wAtt[w] = 0; }
  for (int i = 0; i < nb; i++) if (bm[i].f == 1) wAtt[bm[i].g]++;
  if (trl) {                                          // trailer: box + 2 wheels + tongue, hitched to the car's rear
    ntr = nn;
    for (int i = 0; i < 8; i++) {
      float qx = ((i & 1) - .5f) * 1.5f, qy = .3f + ((i >> 1) & 1) * .7f, qz = -4.1f + ((i >> 2) - .5f) * 1.8f;
      n[nn++] = (Node){x0 + rx * qx + hx * qz, gh(x0 + hx * qz, z0 + hz * qz) + qy, z0 + rz * qx + hz * qz, 0, 0, 0, 1.f, .12f, 0, 0, 0};
    }
    for (int s = -1; s <= 1; s += 2) { float qx = s * .85f, qz = -4.1f;
      n[nn++] = (Node){x0 + rx * qx + hx * qz, gh(x0 + hx * qz, z0 + hz * qz) + .3f, z0 + rz * qx + hz * qz, 0, 0, 0, .4f, .3f, 0, 0, 2}; }
    n[nn++] = (Node){x0 - hx * 3.f, gh(x0 - hx * 3.f, z0 - hz * 3.f) + .6f, z0 - hz * 3.f, 0, 0, 0, 1.f, .1f, 0, 0, 0};
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) add_beam(ntr + i, ntr + j, 0, 1, 0, 8000, 35, .08f * SF, .6f * SF);
    for (int w = 8; w < 10; w++) for (int j = 0; j < 8; j++) { float dx = n[ntr + w].x - n[ntr + j].x, dz = n[ntr + w].z - n[ntr + j].z, dy = n[ntr + w].y - n[ntr + j].y; if (dx * dx + dy * dy + dz * dz < 1.2f) add_beam(ntr + w, ntr + j, 1, 1, 0, 5000, 60, 0, .6f * SF); }
    for (int j = 4; j < 8; j++) add_beam(ntr + 10, ntr + j, 0, 1, 0, 8000, 35, .08f * SF, .6f * SF);
    add_beam(ntr + 10, 0, 3, 1, 0, 3500, 40, 0, .8f); add_beam(ntr + 10, 1, 3, 1, 0, 3500, 40, 0, .8f);
  }
  nbc = nb;
  if (objs) spawn_objects();
}
static void car_pose(float x0, float z0, float hx, float hz, float oy) {
  const Veh *V = &VEH[cv]; const Whl *W = &WHL[cw]; float rx = hz, rz = -hx;
  for (int i = 0; i < NC; i++) {
    int w = i >= 14 && i < 18; float qx = P[i][0] * V->sx, qy = w ? W->r : P[i][1] * V->sy, qz = P[i][2] * V->sz;
    n[i].x = x0 + rx * qx + hx * qz; n[i].y = oy + qy; n[i].z = z0 + rz * qx + hz * qz; n[i].vx = n[i].vy = n[i].vz = 0;
  }
  if (ntr >= 0) {
    for (int i = 0; i < 8; i++) { float qx = ((i & 1) - .5f) * 1.5f, qy = .3f + ((i >> 1) & 1) * .7f, qz = -4.1f + ((i >> 2) - .5f) * 1.8f;
      Node *p = &n[ntr + i]; p->x = x0 + rx * qx + hx * qz; p->y = gh(x0 + hx * qz, z0 + hz * qz) + qy; p->z = z0 + rz * qx + hz * qz; p->vx = p->vy = p->vz = 0; }
    for (int s = -1; s <= 1; s += 2) { float qx = s * .85f, qz = -4.1f; Node *p = &n[ntr + (s < 0 ? 8 : 9)];
      p->x = x0 + rx * qx + hx * qz; p->y = gh(x0 + hx * qz, z0 + hz * qz) + .3f; p->z = z0 + rz * qx + hz * qz; p->vx = p->vy = p->vz = 0; }
    Node *p = &n[ntr + 10]; p->x = x0 - hx * 3.f; p->y = gh(p->x, z0 - hz * 3.f) + .6f; p->z = z0 - hz * 3.f; p->vx = p->vy = p->vz = 0;
  }
}
static void repair(float x0, float z0, float hx, float hz) {   // like BeamNG's repair: every beam, panel and wheel back, car put upright
  for (int i = 0; i < nbc; i++) { bm[i].f = bm[i].t0; bm[i].l0 = bm[i].lr; }
  for (int g = 0; g < NG; g++) pAtt[g] = 0; for (int w = 0; w < 4; w++) wAtt[w] = 0;
  for (int i = 0; i < nbc; i++) { if (bm[i].f == 4) pAtt[bm[i].g]++; else if (bm[i].f == 1 && bm[i].o == 0) wAtt[bm[i].g]++; }
  car_pose(x0, z0, hx, hz, gh(x0, z0) + .5f); dmg = 0;
}
static float thrustAcc, latX, latY, latZ;
static void engine_update(float speed) {
  static const float GEAR_TOP[5] = {.18f, .34f, .54f, .77f, 1.f};
  static const float GEAR_PULL[5] = {1.f, .79f, .65f, .55f, .48f};
  float vmax = engV * (turbo ? 1.5f : 1.f), limit = vmax * GEAR_TOP[gearNow - 1];
  engineRpm = 900.f + speed / (limit + .001f) * 5600.f;
  if (engineRpm > 7000.f) engineRpm = 7000.f;
  if (thr > .05f && engineRpm > 6100.f && gearNow < 5) gearNow++;
  else if (gearNow > 1 && engineRpm < 1700.f) gearNow--;
  limit = vmax * GEAR_TOP[gearNow - 1]; engineRpm = 900.f + speed / (limit + .001f) * 5600.f;
  if (engineRpm > 7000.f) engineRpm = 7000.f;
  float torque;
  if (engineRpm < 1200.f) torque = .45f + (engineRpm - 900.f) * .0006f;
  else if (engineRpm < 3200.f) torque = .63f + (engineRpm - 1200.f) * .00018f;
  else if (engineRpm < 4600.f) torque = .99f;
  else torque = .99f - (engineRpm - 4600.f) * .00013f;
  if (torque < .55f) torque = .55f;
  engineOutput = torque * GEAR_PULL[gearNow - 1];
}
static void tire(Node *p, float fx, float fz, float nx, float ny, float nz, int drive, float dt) {
  float fn = fx * nx + fz * nz, tx = fx - nx * fn, ty = -ny * fn, tz = fz - nz * fn;
  float lx = ny * tz - nz * ty, ly = nz * tx - nx * tz, lz = nx * ty - ny * tx;
  float ll = fsqrt(lx * lx + ly * ly + lz * lz) + 1e-4f; lx /= ll; ly /= ll; lz /= ll;
  float weatherGrip = weatherMode == 1 ? .72f : (weatherMode == 3 ? .48f : 1.f);
  float vl = p->vx * lx + p->vy * ly + p->vz * lz, lg = latG * gripF * weatherGrip;
  float dvm = lg * 3.2f * G * dt * 3.f, d = vl * .5f; if (d > dvm) d = dvm; if (d < -dvm) d = -dvm;   // grip is limited: the tire slides instead of tipping the car over
  p->vx -= lx * d * .3f; p->vy -= ly * d * .3f; p->vz -= lz * d * .3f;           // part of the grip acts at the tire...
  { float J = d / p->im * .7f; latX -= lx * J; latY -= ly * J; latZ -= lz * J; }  // ...the rest through the whole body (no tipping over)
  if (!drive) return;
  float vf = p->vx * tx + p->vy * ty + p->vz * tz;
  float eV = engV * (turbo ? 1.5f : 1.f), eA = engA * (turbo ? 2.2f : 1.f) * engineOutput * (.3f + .7f * gripF);
  if (thr != 0 && !(thr > 0 && vf > eV) && !(thr < 0 && vf < -10)) {
#ifdef THRUSTBODY
    thrustAcc += thr * eA * dt * .25f;
#else
    float a2 = thr * eA * dt; p->vx += tx * a2; p->vy += ty * a2; p->vz += tz * a2;
#endif
  } else if (thr == 0.f && (vf > .5f || vf < -.5f)) { float drag = engA * .018f * dt * (vf > 0 ? 1.f : -1.f); p->vx -= tx * drag; p->vy -= ty * drag; p->vz -= tz * drag; }
  if (brk > 0) { float bf = 1.f - .05f * brk * tuneBrake * weatherGrip * (.4f + .6f * gripF); p->vx *= bf; p->vy *= bf; p->vz *= bf; }
}
static void step(float dt) {
  float hx, hz, thx, thz; heading(&hx, &hz); thx = hx; thz = hz;
  if (ntr >= 0) { float x = n[ntr+4].x + n[ntr+5].x + n[ntr+6].x + n[ntr+7].x - n[ntr].x - n[ntr+1].x - n[ntr+2].x - n[ntr+3].x, z = n[ntr+4].z + n[ntr+5].z + n[ntr+6].z + n[ntr+7].z - n[ntr].z - n[ntr+1].z - n[ntr+2].z - n[ntr+3].z, l = fsqrt(x * x + z * z) + 1e-4f; thx = x / l; thz = z / l; }
  float forward = 0; for (int i = 0; i < 20; i++) forward += (n[i].vx * hx + n[i].vz * hz) / 20.f;
  engine_update(forward < 0 ? -forward : forward);
  float s = steer, c = fsin(s + 1.5708f), tot = 0;
  for (int i = 0; i < nn; i++) n[i].vy -= G * dt;
  for (int i = 0; i < nb; i++) {
    Beam *b = &bm[i];
    if (b->f == 2) { if (b->o == 0) tot += b->t0 == 0 ? .45f : (b->t0 == 4 ? .08f : 0); continue; }
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
      if (b->o == 0) { float d = (b->l0 - b->lr) / b->lr; tot += d < 0 ? -d : d; }
    } else if (b->f == 1) {
      if (e > b->bk) { b->f = 2; if (b->o == 0) wAtt[b->g]--; }
    } else if (b->f == 3) { if (L - b->l0 > b->bk) b->f = 2; }
    else if (L - b->l0 > b->bk || b->l0 - L > b->bk * 1.4f) {   // panel attachment tears or is crushed
      b->f = 2;
      if (--pAtt[b->g] * 100 <= pAtt0[b->g] * 45 && pAtt[b->g] > 0) {   // enough attachments gone: the part comes off
        int gg = b->g; for (int q = 0; q < nb; q++) if (bm[q].f == 4 && bm[q].g == gg) bm[q].f = 2;
        pAtt[gg] = 0; for (int j = 0; j < PN[gg]; j++) { Node *pn = &n[PB[gg] + j]; pn->vy += 3.5f; pn->vx += (j & 1 ? .8f : -.8f); }
      }
    }
  }
  { float nd = tot * 100.f / (nbody * .055f); if (nd > dmg) dmg = nd; if (dmg > 100) dmg = 100; }
  thrustAcc = 0; latX = latY = latZ = 0;
  for (int i = 0; i < nn; i++) {
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
      { float rb = p->t == 1 ? 1.f : 1.1f; if (vn < 0) { p->vx -= vn * nx * rb; p->vy -= vn * ny * rb; p->vz -= vn * nz * rb; } }
      if (p->t == 1 && wAtt[i - 14] > 0) { if (i >= 16) tire(p, hx * c + hz * s, hz * c - hx * s, nx, ny, nz, 1, dt); else tire(p, hx, hz, nx, ny, nz, 1, dt); }
      else if (p->t == 2) tire(p, thx, thz, nx, ny, nz, 0, dt);
      else if (p->t == 1) { p->vx *= .995f; p->vz *= .995f; }
      else { p->vx *= .96f; p->vz *= .96f; }
    }
  }
  if (mapId == 3) for (int i = 0; i < 50; i++) {
    Node *p = &n[i]; if (p->y + p->r <= 0) continue;
    if (p->x < -CITY_X_LIMIT + p->r) { p->x = -CITY_X_LIMIT + p->r; if (p->vx < 0) p->vx = -p->vx * .15f; }
    if (p->x > CITY_X_LIMIT - p->r) { p->x = CITY_X_LIMIT - p->r; if (p->vx > 0) p->vx = -p->vx * .15f; }
    if (p->z < -CITY_Z_LIMIT + p->r) { p->z = -CITY_Z_LIMIT + p->r; if (p->vz < 0) p->vz = -p->vz * .15f; }
    if (p->z > CITY_Z_LIMIT - p->r) { p->z = CITY_Z_LIMIT - p->r; if (p->vz > 0) p->vz = -p->vz * .15f; }
    int ix0, iz0; city_nearest(p->x, p->z, &ix0, &iz0);
    for (int ix = ix0 - 1; ix <= ix0 + 1; ix++) for (int iz = iz0 - 1; iz <= iz0 + 1; iz++) {
      float bx, bz; city_center(ix, iz, &bx, &bz); float height = city_height(ix, iz);
      if (p->y - p->r >= height) continue;
      float px = 8.5f + p->r - (p->x > bx ? p->x - bx : bx - p->x), pz = 11.f + p->r - (p->z > bz ? p->z - bz : bz - p->z);
      if (px <= 0 || pz <= 0) continue;
      if (px < pz) {
        float sign = p->x >= bx ? 1.f : -1.f; p->x = bx + sign * (8.5f + p->r);
        float vn = p->vx * sign; if (vn < 0) p->vx -= vn * 1.2f; p->vz *= .88f;
      } else {
        float sign = p->z >= bz ? 1.f : -1.f; p->z = bz + sign * (11.f + p->r);
        float vn = p->vz * sign; if (vn < 0) p->vz -= vn * 1.2f; p->vx *= .88f;
      }
    }
  }
  if (thrustAcc != 0) for (int i = 0; i < NC; i++) { n[i].vx += hx * thrustAcc; n[i].vz += hz * thrustAcc; }
  if (latX != 0 || latY != 0 || latZ != 0) {
    float Ms = 0; for (int i = 0; i < 22; i++) if (i < 14 || i > 17) Ms += 1.f / n[i].im;
    for (int i = 0; i < 22; i++) if (i < 14 || i > 17) { n[i].vx += latX / Ms; n[i].vy += latY / Ms; n[i].vz += latZ / Ms; }
  }
  if (nobj) {                                           // car/trailer beams vs object corner spheres
    float cx = (n[4].x + n[5].x) * .5f, cz = (n[4].z + n[5].z) * .5f;
    for (int j = nob0; j < nn; j++) {
      Node *p = &n[j]; float ddx = p->x - cx, ddz = p->z - cz; if (ddx > 18 || ddx < -18 || ddz > 18 || ddz < -18) continue;
      for (int i = 0; i < nbc; i++) {
        Beam *b = &bm[i]; if (b->f > 1) continue;
        Node *a = &n[b->a], *q = &n[b->b];
        float ex = q->x - a->x, ey = q->y - a->y, ez = q->z - a->z, el = ex * ex + ey * ey + ez * ez + 1e-6f;
        float t = ((p->x - a->x) * ex + (p->y - a->y) * ey + (p->z - a->z) * ez) / el; t = t < 0 ? 0 : (t > 1 ? 1 : t);
        float dx = p->x - (a->x + t * ex), dy = p->y - (a->y + t * ey), dz = p->z - (a->z + t * ez), d2 = dx * dx + dy * dy + dz * dz, R = p->rc + .15f;
        if (d2 >= R * R) continue;
        float d = fsqrt(d2) + 1e-5f, nx = dx / d, ny = dy / d, nz = dz / d, pen = R - d; if (pen > .3f) pen = .3f;
        float imq = a->im * (1 - t) * (1 - t) + q->im * t * t, it = p->im + imq, cp = pen / it;
        p->x += nx * cp * p->im; p->y += ny * cp * p->im; p->z += nz * cp * p->im;
        a->x -= nx * cp * a->im * (1 - t); a->y -= ny * cp * a->im * (1 - t); a->z -= nz * cp * a->im * (1 - t);
        q->x -= nx * cp * q->im * t; q->y -= ny * cp * q->im * t; q->z -= nz * cp * q->im * t;
        float vqx = a->vx * (1 - t) + q->vx * t, vqy = a->vy * (1 - t) + q->vy * t, vqz = a->vz * (1 - t) + q->vz * t;
        float vn = (p->vx - vqx) * nx + (p->vy - vqy) * ny + (p->vz - vqz) * nz;
        if (vn < 0) { float jj = -1.15f * vn / it;
          p->vx += nx * jj * p->im; p->vy += ny * jj * p->im; p->vz += nz * jj * p->im;
          a->vx -= nx * jj * a->im * (1 - t); a->vy -= ny * jj * a->im * (1 - t); a->vz -= nz * jj * a->im * (1 - t);
          q->vx -= nx * jj * q->im * t; q->vy -= ny * jj * q->im * t; q->vz -= nz * jj * q->im * t; }
      }
    }
  }
}

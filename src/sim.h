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
static Node nPl[NMAX]; static Beam bmPl[MB]; static Node *n = nPl; static Beam *bm = bmPl;   // active vehicle: the player's by default, bots swap these pointers
static int nbCap = MB;
static int nn, nb, nbody, nbc, ntr = -1, nob0, nobj, pAtt[NG], pAtt0[NG], wAtt[4], trl, solid = 1, mapId;
typedef struct { float x, z, phase, speed, hx, hz; uint8_t bot; } TrafficCar;   // bot: 0 = kinematic rail car, else physics-bot slot + 1
static TrafficCar traffic[MAX_AI]; static int trafficEnabled = 1, trafficCount = 6, trafficBehavior, trafficSpeed = 2;
static TrafficCar policeCar; static int pursuitEnabled, cityPlan, weatherMode;
static float tunePower = 1.f, tuneGrip = 1.f, tuneSusp = 1.f, tuneBrake = 1.f, signalClock;
static const uint8_t PB[NG] = {22, 26, 30, 34, 38, 42, 46, 50, 54, 57}, PN[NG] = {4, 4, 4, 4, 4, 4, 4, 4, 3, 3};   // hood, front bumper, rear bumper, trunk lid, doors L/R, windshield, engine, seats L/R
static const char *PNAME[NG] = {"Capot", "Pare-ch.AV", "Pare-ch.AR", "Coffre", "Porte G", "Porte D", "Pare-brise", "Moteur", "Siege G", "Siege D"};
static float thr, brk, steer, dmg, gfeat, gdist, engA = 10, engV = 38, engineRpm = 900, engineOutput = .45f, latG = .25f, gsx = 1, gsz = 1;
static int gearNow = 1;
// ---- vehicle systems: radiator (front), fuel tank (rear), driveshaft (underbody). Pierced tank => leak, fire or explosion.
static float fuel = 1.f, coolant = 1.f, engTemp = .3f, radHp = 1.f, tankHp = 1.f, shaftHp = 1.f, driveEff = 1.f;
static float fireI, fireT, boomAt = 1e9f, boomT = 99.f, flashT, jolt, vcmx, vcmz, steamI, fxClock;
static int burning, exploded, fxPrimed; static unsigned fxSeed = 777u;
static float fx_rand(void) { fxSeed = fxSeed * 1664525u + 1013904223u; return (fxSeed >> 8) / 16777215.f; }
static void fx_reset(void) {
  fuel = 1.f; coolant = 1.f; engTemp = .3f; radHp = tankHp = shaftHp = driveEff = 1.f; fireI = fireT = 0.f; boomAt = 1e9f; boomT = 99.f;
  flashT = 0.f; jolt = vcmx = vcmz = steamI = 0.f; burning = exploded = fxPrimed = 0;
}
static const float SOLF[3] = {1.6f, 1.f, .65f};
typedef struct { const char *nm; float sx, sy, sz, im, pw, gr; uint8_t r, g, b; } Veh;
#define NV 16   // 0-3 de base, 4 = généré, 5+ = modèles ajoutés (VEH[NV] reste réservé au bot actif)
static char genName[16] = "Gen #1";
static Veh VEH[NV + 1] = {{"Berline",1,1,1,1,1,1,200,35,30},{"Sport",1.05f,.8f,1.12f,1.1f,1.15f,1.12f,40,90,210},
  {"Pick-up",1.12f,1.25f,1.15f,.8f,.95f,.85f,230,170,30},{"Buggy",.8f,.85f,.7f,1.3f,1.1f,1.15f,60,180,70},{genName,1,1,1,1,1,1,150,150,150},
  // ---- modèles ajoutés : nom, largeur, hauteur, longueur, 1/masse, puissance, grip, couleur ----
  {"Citadine",.85f,.95f,.8f,1.3f,.8f,1.f,245,245,245},
  {"4x4",1.1f,1.3f,1.1f,.75f,1.05f,.95f,40,90,50},
  {"SUV",1.12f,1.2f,1.15f,.78f,1.f,.9f,60,60,70},
  {"Muscle",1.1f,.9f,1.2f,.9f,1.3f,.95f,20,20,25},
  {"Van",1.15f,1.5f,1.35f,.65f,.85f,.8f,230,230,235},
  {"Camion",1.25f,1.7f,1.6f,.65f,.8f,.75f,200,90,30},
  {"Kart",.7f,.75f,.7f,1.45f,1.f,1.2f,240,60,60},
  {"Rallye",1.f,.95f,1.f,1.05f,1.2f,1.2f,250,120,20},
  {"Limousine",1.f,.95f,1.4f,.7f,.9f,.85f,15,15,18},
  {"Dragster",.85f,.75f,1.35f,1.15f,1.35f,.7f,200,10,10},
  {"Taxi",1.f,1.02f,1.05f,.95f,.95f,1.f,255,200,0}};
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
#define CITY_X_LIMIT 112.f
#define CITY_Z_LIMIT 144.f
#define CITY_NX 8
#define CITY_NZ 8
// 8 x 8 blocks, varied: 0 office, 1 tower (downtown), 2 low-rise, 3 park (no building)
static unsigned city_hash(int ix, int iz) { unsigned v = (unsigned)(ix + 50) * 73856093u ^ (unsigned)(iz + 50) * 19349663u; v ^= v >> 13; v *= 0x5bd1e995u; v ^= v >> 15; return v; }
static int city_kind(int ix, int iz) {
  unsigned v = city_hash(ix, iz); int k = (int)(v % 10), central = ix >= -2 && ix <= 1 && iz >= -2 && iz <= 1;
  if (k == 0) return 3; if (k <= 2) return 2; if (central && k <= 6) return 1; return 0;
}
static float city_height(int ix, int iz) {
  unsigned v = city_hash(ix, iz) >> 8; int k = city_kind(ix, iz);
  if (k == 3) return 0.f; if (k == 2) return 5.f + (v & 3) * 1.5f; if (k == 1) return 30.f + (v % 5) * 6.f; return 12.f + (v & 3) * 4.f;
}
static int city_round(float v) { return (int)(v + (v >= 0 ? .5f : -.5f)); }
static void city_center(int ix, int iz, float *x, float *z) {
  *x = ix * 28.f + 14.f; *z = iz * 36.f + 18.f;
  if (cityPlan == 1) *x += (iz & 1) ? 5.f : -5.f;
  else if (cityPlan == 2) *z += (ix & 1) ? 5.f : -5.f;
}
static void city_nearest(float x, float z, int *ix, int *iz) {
  float best = 1e9f; int ea = city_round((x - 14.f) / 28.f), eb = city_round((z - 18.f) / 36.f); *ix = ea; *iz = eb;
  for (int a = ea - 1; a <= ea + 1; a++) for (int b = eb - 1; b <= eb + 1; b++) {
    float cx, cz; city_center(a, b, &cx, &cz); float dx = x - cx, dz = z - cz, d = dx * dx + dz * dz;
    if (d < best) { best = d; *ix = a; *iz = b; }
  }
}
static int city_push_out(float *x, float *z, float r) {   // projects a ground point out of every building footprint
  int ix0, iz0, moved = 0; city_nearest(*x, *z, &ix0, &iz0);
  for (int ix = ix0 - 1; ix <= ix0 + 1; ix++) for (int iz = iz0 - 1; iz <= iz0 + 1; iz++) {
    float bx, bz; if (city_height(ix, iz) <= 0.f) continue; city_center(ix, iz, &bx, &bz);
    float dx = *x - bx, dz = *z - bz, px = 8.5f + r - (dx < 0 ? -dx : dx), pz = 11.f + r - (dz < 0 ? -dz : dz);
    if (px <= 0 || pz <= 0) continue; moved = 1;
    if (px < pz) *x = bx + (dx >= 0 ? 8.5f + r : -(8.5f + r)); else *z = bz + (dz >= 0 ? 11.f + r : -(11.f + r));
  }
  return moved;
}
static float gh(float x, float z) {
  float h, ft = 0, ax = x < 0 ? -x : x;
  if (mapId == 3 || mapId == 4) { gdist = ax; gfeat = 0; return 0; }   // 4 = carte vide (sol plat)
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
static void bots_clear(void);
static void rail_box(int mode, float *x0, float *x1, float *z0, float *z1) {   // 0 inner ring, 2 patrol block, 3 big outer ring
  if (mode == 2) { *x0 = -26.3f; *x1 = -1.7f; *z0 = 1.7f; *z1 = 34.3f; }
  else if (mode == 3) { *x0 = -82.3f; *x1 = 82.3f; *z0 = -106.3f; *z1 = 106.3f; }
  else { *x0 = -26.3f; *x1 = 26.3f; *z0 = -34.3f; *z1 = 34.3f; }
}
static int rail_mode_of(int i) { return trafficBehavior == 0 ? ((i & 1) ? 3 : 0) : trafficBehavior; }
static void traffic_reset(void) {
  bots_clear(); policeCar.bot = 0;
  static const float SPEED[5] = {4.5f, 7.f, 9.f, 12.f, 15.f};
  for (int i = 0; i < MAX_AI; i++) { { float rx0, rx1, rz0, rz1; rail_box(rail_mode_of(i), &rx0, &rx1, &rz0, &rz1); float loop = 2.f * ((rx1 - rx0) + (rz1 - rz0)); int per = trafficBehavior == 0 ? (trafficCount + 1) / 2 : trafficCount, kk = trafficBehavior == 0 ? i / 2 : i; traffic[i].phase = kk * loop / (per > 0 ? per : 1); } traffic[i].speed = SPEED[trafficSpeed] * (.9f + (i % 5) * .05f); traffic[i].x = traffic[i].z = traffic[i].hx = traffic[i].hz = 0; traffic[i].bot = 0; }
  policeCar.x = (n[4].x + n[5].x) * .5f; policeCar.z = (n[4].z + n[5].z) * .5f - 12.f; policeCar.hx = 0; policeCar.hz = 1;
  traffic_update(0.f);
}
static void traffic_update(float dt) {
  if (mapId != 3) return;
  float px = (n[4].x + n[5].x) * .5f, pz = (n[4].z + n[5].z) * .5f, hx, hz; heading(&hx, &hz);
  if (trafficEnabled) for (int i = 0; i < trafficCount; i++) {
    TrafficCar *car = &traffic[i];
    if (car->bot) continue;                                  // driven by the physics engine
    if (trafficBehavior == 1) {
      float distance = 5.f + (i / 4) * 2.5f, side = ((i % 4) - 1.5f) * 2.f, rx = hz, rz = -hx;
      float tx = px - hx * distance + rx * side, tz = pz - hz * distance + rz * side;
      if (tx < -CITY_X_LIMIT + 3.f) tx = -CITY_X_LIMIT + 3.f; if (tx > CITY_X_LIMIT - 3.f) tx = CITY_X_LIMIT - 3.f;
      if (tz < -CITY_Z_LIMIT + 3.f) tz = -CITY_Z_LIMIT + 3.f; if (tz > CITY_Z_LIMIT - 3.f) tz = CITY_Z_LIMIT - 3.f;
      float blend = dt <= 0 ? 1.f : dt * 1.8f; if (blend > .12f) blend = .12f;
      car->x += (tx - car->x) * blend; car->z += (tz - car->z) * blend; car->hx = hx; car->hz = hz; continue;
    }
    float x0, x1, z0, z1; rail_box(rail_mode_of(i), &x0, &x1, &z0, &z1);   // lane rectangle, inset from the road centre lines
    float W = x1 - x0, Hh = z1 - z0, loop = 2.f * (W + Hh);
    car->phase += car->speed * (trafficBehavior == 2 ? .7f : 1.f) * dt; while (car->phase >= loop) car->phase -= loop;
    float p = car->phase;
    if (p < Hh) { car->x = x0; car->z = z0 + p; car->hx = 0; car->hz = 1; }
    else if (p < Hh + W) { car->x = x0 + p - Hh; car->z = z1; car->hx = 1; car->hz = 0; }
    else if (p < 2.f * Hh + W) { car->x = x1; car->z = z1 - (p - Hh - W); car->hx = 0; car->hz = -1; }
    else { car->x = x1 - (p - 2.f * Hh - W); car->z = z0; car->hx = -1; car->hz = 0; }
  }
  for (int i = 0; i < trafficCount && trafficEnabled; i++) if (!traffic[i].bot) city_push_out(&traffic[i].x, &traffic[i].z, 1.2f);   // other city plans move the buildings
  if (pursuitEnabled && !policeCar.bot) {
    float dx = px - policeCar.x, dz = pz - policeCar.z, distance = fsqrt(dx * dx + dz * dz) + .001f;
    float tx = dx / distance, tz = dz / distance, rate = (dt <= 0.f) ? 1.f : dt * 1.6f; if (rate > .08f) rate = .08f;
    policeCar.hx += (tx - policeCar.hx) * rate; policeCar.hz += (tz - policeCar.hz) * rate;
    float len = fsqrt(policeCar.hx * policeCar.hx + policeCar.hz * policeCar.hz) + .001f; policeCar.hx /= len; policeCar.hz /= len;
    float speed = distance > 8.f ? 11.f : 5.f; policeCar.x += policeCar.hx * speed * dt; policeCar.z += policeCar.hz * speed * dt;
    city_push_out(&policeCar.x, &policeCar.z, 1.4f);
    if (policeCar.x < -CITY_X_LIMIT + 3.f) policeCar.x = -CITY_X_LIMIT + 3.f; if (policeCar.x > CITY_X_LIMIT - 3.f) policeCar.x = CITY_X_LIMIT - 3.f;
    if (policeCar.z < -CITY_Z_LIMIT + 3.f) policeCar.z = -CITY_Z_LIMIT + 3.f; if (policeCar.z > CITY_Z_LIMIT - 3.f) policeCar.z = CITY_Z_LIMIT - 3.f;
  }
  signalClock += dt;
}
static void add_beam(int a, int b, int f, int o, int g, float k, float c, float yl, float bk) {
  if (nb >= nbCap) return;
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
  if (mapId == 3) city_push_out(&x0, &z0, 3.2f);          // never spawn inside a building
  fx_reset();
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
  car_pose(x0, z0, hx, hz, gh(x0, z0) + .5f); dmg = 0; fx_reset();
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
  { float spd = fsqrt(p->vx * p->vx + p->vz * p->vz), gf = 1.f;   // drift: the faster you go, the less the tyres hold (rear first)
    if (spd > 16.f) { gf = 1.f / (1.f + (spd - 16.f) * .045f); if (gf < .22f) gf = .22f; }
    if (p->t == 1 && (int)(p - n) < 16) gf *= .8f;
    lg *= gf; }
  float dvm = lg * 3.2f * G * dt * 3.f, d = vl * .5f; if (d > dvm) d = dvm; if (d < -dvm) d = -dvm;   // grip is limited: the tire slides instead of tipping the car over
  p->vx -= lx * d * .3f; p->vy -= ly * d * .3f; p->vz -= lz * d * .3f;           // part of the grip acts at the tire...
  { float J = d / p->im * .7f; latX -= lx * J; latY -= ly * J; latZ -= lz * J; }  // ...the rest through the whole body (no tipping over)
  if (!drive) return;
  float vf = p->vx * tx + p->vy * ty + p->vz * tz;
  float eV = engV * (turbo ? 1.5f : 1.f), eA = engA * 1.8f * (turbo ? 2.2f : 1.f) * engineOutput * (.3f + .7f * gripF) * driveEff;
  if (thr != 0 && !(thr < 0 && vf < -10)) {   // no top-speed cut-off
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
      if (height <= 0.f || p->y - p->r >= height) continue;
      float px = 8.5f + p->r - (p->x > bx ? p->x - bx : bx - p->x), pz = 11.f + p->r - (p->z > bz ? p->z - bz : bz - p->z);
      if (px <= 0 || pz <= 0) continue;
      // soft contact: the node is pushed out a few centimetres per step (never teleported), so the beams
      // never see a sudden stretch. Axis choice has hysteresis-free min-penetration but the push is capped.
      float vn, push;
      if (px < pz) {
        float sign = p->x >= bx ? 1.f : -1.f; push = px < .025f ? px : .025f; p->x += sign * push;
        vn = p->vx * sign; if (vn < 0) p->vx -= vn * sign * 1.1f; p->vz *= .97f;   // normal is (sign,0): the impulse must carry the sign
      } else {
        float sign = p->z >= bz ? 1.f : -1.f; push = pz < .025f ? pz : .025f; p->z += sign * push;
        vn = p->vz * sign; if (vn < 0) p->vz -= vn * sign * 1.1f; p->vx *= .97f;
      }
    }
  }
  for (int i = 0; i < nn; i++) {                         // safety net: a node can never leave the physically possible range
    Node *p = &n[i]; float v2 = p->vx * p->vx + p->vy * p->vy + p->vz * p->vz;
    if (!(v2 == v2) || v2 > 1000.f * 1000.f) { float k = v2 == v2 ? 1000.f / fsqrt(v2) : 0.f; p->vx *= k; p->vy *= k; p->vz *= k; if (!(p->x == p->x) || !(p->y == p->y) || !(p->z == p->z)) { p->x = n[4].x; p->y = n[4].y + 1.f; p->z = n[4].z; } }
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

// ---- systems damage: looks at the crushed / broken body beams around each component
static float rawR, rawT, rawS;                                        // raw damage sums (kept for tuning)
static void comp_damage(void) {
  float sr = 0, st = 0, ss = 0;
  for (int i = 0; i < nbc; i++) {
    Beam *b = &bm[i]; if (b->o != 0 || (b->t0 != 0 && b->t0 != 4) || b->a >= 50 || b->b >= 50) continue;   // body, bumpers and their attachments
    float sev;
    if (b->t0 == 4) sev = b->f == 2 ? .1f : 0.f;                                         // a torn-off attachment
    else { float def = (b->l0 - b->lr) / b->lr; if (def < 0) def = -def; sev = b->f == 2 ? 1.f : def * 5.f; if (sev > 1.f) sev = 1.f; }
    if (sev < .05f) continue;
    float za = P[b->a][2], zb = P[b->b][2], ya = P[b->a][1], yb = P[b->b][1], mx = (P[b->a][0] + P[b->b][0]) * .5f, mz = (za + zb) * .5f;
    if (za >= 1.9f || zb >= 1.9f) sr += sev;                                              // reaches the nose: radiator
    if (za <= -1.9f || zb <= -1.9f) st += sev;                                            // reaches the tail: fuel tank
    if (b->t0 == 0 && ya < .9f && yb < .9f && mx > -.6f && mx < .6f && mz > -1.4f && mz < 1.4f) ss += sev * 1.6f;   // central underbody: driveshaft
  }
  for (int w = 0; w < 4; w++) if (wAtt[w] <= 0) ss += .9f;                                    // a torn-off wheel takes the axle line with it
  if (dmg > 75.f) ss += (dmg - 75.f) * .12f;                                                  // a totalled car has a broken driveline
  rawR = sr; rawT = st; rawS = ss;
  float h;
  h = 1.f - sr / 3.8f; if (h < radHp) radHp = h < 0 ? 0 : h;
  h = 1.f - st / 6.f; if (h < tankHp) tankHp = h < 0 ? 0 : h;
  h = 1.f - ss / 2.6f; if (h < shaftHp) shaftHp = h < 0 ? 0 : h;
}
static void explode(void) {
  float hx, hz, cx = 0, cy = 0, cz = 0; heading(&hx, &hz);
  for (int i = 0; i < 20; i++) { cx += n[i].x * .05f; cy += n[i].y * .05f; cz += n[i].z * .05f; }
  float bx = cx - hx * 1.6f * gsz, bz = cz - hz * 1.6f * gsz, by = cy - .3f;                 // blast centre = the tank
  for (int i = 0; i < nn; i++) { Node *p = &n[i]; float dx = p->x - bx, dy = p->y - by + .6f, dz = p->z - bz, d = fsqrt(dx * dx + dy * dy + dz * dz) + .4f, k = 26.f / (1.f + d * .6f) / d;
    p->vx += dx * k + (fx_rand() - .5f) * 3.f; p->vy += dy * k + 4.f; p->vz += dz * k + (fx_rand() - .5f) * 3.f; }
  for (int i = 0; i < nbc; i++) { Beam *b = &bm[i]; if (b->o != 0) continue;
    if (b->f == 0 && b->t0 == 0 && fx_rand() < .3f) b->f = 2;
    else if (b->f == 1 && fx_rand() < .25f) { b->f = 2; wAtt[b->g]--; } }
  for (int g = 0; g < NG; g++) if (pAtt[g] > 0 && fx_rand() < (g < 6 ? .8f : .35f)) {       // panels are blown off
    for (int q = 0; q < nbc; q++) if (bm[q].o == 0 && bm[q].f == 4 && bm[q].g == g) bm[q].f = 2;
    pAtt[g] = 0; for (int j = 0; j < PN[g]; j++) { Node *pn = &n[PB[g] + j]; pn->vy += 6.f + fx_rand() * 4.f; pn->vx += (fx_rand() - .5f) * 8.f; } }
  exploded = 1; burning = 1; fireI = 1.f; fireT = 0.f; boomT = 0.f; flashT = 1.f; fuel = .2f; dmg = 100.f; tankHp = 0.f;
  if (radHp > .2f) radHp = .2f; if (shaftHp > .3f) shaftHp = .3f;
}
static void fx_update(float dt) {                 // once per frame
  if (dt <= 0.f) return; if (dt > .1f) dt = .1f;
  fxClock += dt;
  float vx = 0, vz = 0; for (int i = 0; i < 20; i++) { vx += n[i].vx * .05f; vz += n[i].vz * .05f; }
  if (!fxPrimed) { vcmx = vx; vcmz = vz; fxPrimed = 1; }
  float dvx = vx - vcmx, dvz = vz - vcmz; jolt = fsqrt(dvx * dvx + dvz * dvz); vcmx = vx; vcmz = vz;   // speed lost in this frame
  float spd = fsqrt(vx * vx + vz * vz), ta = thr < 0 ? -thr : thr;
  comp_damage();
  if (radHp < .6f && coolant > 0.f) { coolant -= dt * (.6f - radHp) * .16f; if (coolant < 0.f) coolant = 0.f; }    // coolant leak
  float target = .35f + .3f * ta; if (coolant < .5f) target += (.5f - coolant) * 2.6f;
  engTemp += (target - engTemp) * dt * .12f;
  steamI = (radHp < .6f && engTemp > .6f) ? (engTemp - .6f) * 1.5f : 0.f; if (steamI > 1.f) steamI = 1.f;
  float pw = engTemp > 1.f ? 1.f - (engTemp - 1.f) * 1.8f : 1.f; if (pw < 0.f) pw = 0.f;
  float sh = shaftHp > .65f ? 1.f : (shaftHp < .3f ? 0.f : (shaftHp - .3f) / .35f);
  fuel -= dt * (.0006f + .0035f * ta) * (turbo ? 2.f : 1.f);
  if (tankHp < .6f && fuel > 0.f) fuel -= dt * (.6f - tankHp) * .12f;                                       // fuel leak
  if (fuel < 0.f) fuel = 0.f;
  if (!burning && fuel > .03f && tankHp < .6f) {                                                             // ignition of a leaking tank
    float w = .8f - tankHp, risk = 0.f;
    if (jolt > 2.5f) risk += (jolt - 2.5f) * .24f * w;                                                       // sparks from an impact
    if (spd > 8.f) risk += dt * .05f * w;                                                                    // scraping sparks
    if (engTemp > 1.2f) risk += dt * .12f;                                                                   // hot engine
    if (fx_rand() < risk) { burning = 1; fireT = 0.f; fireI = .1f; boomAt = fx_rand() < .5f ? 3.f + fx_rand() * 4.f : 1e9f; }   // 50%: it will blow up later
  }
  if (!burning && fuel > .03f && engTemp > 1.5f && fx_rand() < dt * .05f) { burning = 1; fireT = 0.f; fireI = .1f; boomAt = fx_rand() < .35f ? 10.f + fx_rand() * 6.f : 1e9f; }   // boiling engine catches fire
  if (!exploded && fuel > .3f && tankHp < .35f && jolt > 12.f && fx_rand() < .35f) explode();               // very violent hit on a full, broken tank
  if (burning) {
    fireT += dt; fireI += (1.f - fireI) * dt * .8f; fuel -= dt * (exploded ? .02f : .025f); if (fuel < 0.f) fuel = 0.f;
    dmg += dt * 3.f * fireI; if (dmg > 100.f) dmg = 100.f;
    if (!exploded && fireT > boomAt && fuel > .25f) explode();
    if (fuel < .03f || (exploded && fireT > 25.f)) { fireI -= dt * .3f; if (fireI < .04f) { fireI = 0.f; burning = 0; } }
    if (fireT > 8.f) pw = 0.f;                                                                              // the engine bay burnt out
  }
  if (exploded) boomT += dt;
  if (flashT > 0.f) flashT -= dt * 3.5f;
  driveEff = sh * pw * (fuel > .005f ? 1.f : 0.f) * (exploded ? 0.f : 1.f);
}

// =====================================================================================================
// ---- Physical bots. A traffic / police car close to the player is promoted from a kinematic "rail" car
// to a complete soft-body car: same nodes, beams, panels, wheels, engine, radiator, tank, fire and
// explosion as the player's car, running through the very same step() / fx_update() code.
// Far away it is handed back to the rails (wrecks stay until they are far enough).
// ---- Technique: the physics code works on the globals n, bm, nn, nb, fuel... Each vehicle owns a Ctx;
// "entering" a bot swaps the array pointers and copies a few dozen scalars (zero-copy for the big arrays).
#ifndef MAX_PHYS
#define MAX_PHYS 3            // simultaneous full-physics bots: about 12 KB RAM and one more car of CPU each
#endif
#define BOT_MB 300            // a car without trailer uses 277 beams
#ifndef PHYS_NEAR
#define PHYS_NEAR 26.f        // a rail car closer than this to the player becomes a physical bot
#endif
#ifndef PHYS_FAR
#define PHYS_FAR 55.f         // a healthy bot farther than this goes back on the rails
#endif
#ifndef PHYS_WRECK_FAR
#define PHYS_WRECK_FAR 80.f   // a wreck is only removed when it is this far
#endif
#define PSRC 200              // source id of the police car
#define CAR_R .3f             // node-vs-beam radius for car/car contacts
typedef struct {
  Node *n; Beam *bm;
  int nn, nb, nbCap, nbody, nbc, ntr, nobj, nob0, trl, turbo, gearNow, burning, exploded, fxPrimed, cv, cw, cs, ce, cc;
  int pAtt[NG], pAtt0[NG], wAtt[4];
  float dmg, thr, brk, steer, engA, engV, engineRpm, engineOutput, latG, gsx, gsz;
  float fuel, coolant, engTemp, radHp, tankHp, shaftHp, driveEff, fireI, fireT, boomAt, boomT, flashT, jolt, vcmx, vcmz, steamI;
} Ctx;
#define CTX_I(X) X(nn) X(nb) X(nbCap) X(nbody) X(nbc) X(ntr) X(nobj) X(nob0) X(trl) X(turbo) X(gearNow) X(burning) X(exploded) X(fxPrimed) X(cv) X(cw) X(cs) X(ce) X(cc)
#define CTX_F(X) X(dmg) X(thr) X(brk) X(steer) X(engA) X(engV) X(engineRpm) X(engineOutput) X(latG) X(gsx) X(gsz) \
  X(fuel) X(coolant) X(engTemp) X(radHp) X(tankHp) X(shaftHp) X(driveEff) X(fireI) X(fireT) X(boomAt) X(boomT) X(flashT) X(jolt) X(vcmx) X(vcmz) X(steamI)
static void ctx_save(Ctx *c) {
  c->n = n; c->bm = bm;
#define S_(v) c->v = v;
  CTX_I(S_) CTX_F(S_)
#undef S_
  for (int i = 0; i < NG; i++) { c->pAtt[i] = pAtt[i]; c->pAtt0[i] = pAtt0[i]; } for (int i = 0; i < 4; i++) c->wAtt[i] = wAtt[i];
}
static void ctx_load(const Ctx *c) {
  n = c->n; bm = c->bm;
#define L_(v) v = c->v;
  CTX_I(L_) CTX_F(L_)
#undef L_
  for (int i = 0; i < NG; i++) { pAtt[i] = c->pAtt[i]; pAtt0[i] = c->pAtt0[i]; } for (int i = 0; i < 4; i++) wAtt[i] = c->wAtt[i];
}
typedef struct { Node nd[NC]; Beam bd[BOT_MB]; Ctx c; Veh veh; float spin, stuckT, steerS; uint8_t used, src; } Bot;
static Bot bots[MAX_PHYS]; static Ctx pctx;
static void bot_enter(Bot *b) { ctx_save(&pctx); VEH[NV] = b->veh; ctx_load(&b->c); }   // makes the bot the active vehicle
static void bot_leave(Bot *b) { ctx_save(&b->c); ctx_load(&pctx); }                      // and gives the player back
static void bots_clear(void) { for (int i = 0; i < MAX_PHYS; i++) bots[i].used = 0; }
static int bots_active(void) { for (int i = 0; i < MAX_PHYS; i++) if (bots[i].used) return 1; return 0; }
static float absf_(float v) { return v < 0 ? -v : v; }

// rail geometry shared with traffic_update(): 0 = circuit (inner ring road), 2 = patrol (block around the start)
static void rail_point(int mode, float p, float *x, float *z) {
  float x0, x1, z0, z1; rail_box(mode, &x0, &x1, &z0, &z1); float W = x1 - x0, H = z1 - z0, loop = 2.f * (W + H);
  while (p >= loop) p -= loop; while (p < 0.f) p += loop;
  if (p < H) { *x = x0; *z = z0 + p; } else if (p < H + W) { *x = x0 + p - H; *z = z1; }
  else if (p < 2.f * H + W) { *x = x1; *z = z1 - (p - H - W); } else { *x = x1 - (p - 2.f * H - W); *z = z0; }
}
static float rail_phase(int mode, float x, float z) {       // closest point of the rails, as a phase
  float x0, x1, z0, z1; rail_box(mode, &x0, &x1, &z0, &z1); float W = x1 - x0, H = z1 - z0, best = 1e9f, ph = 0, c, d;
  c = z < z0 ? z0 : (z > z1 ? z1 : z); d = (x - x0) * (x - x0) + (z - c) * (z - c); if (d < best) { best = d; ph = c - z0; }
  c = x < x0 ? x0 : (x > x1 ? x1 : x); d = (x - c) * (x - c) + (z - z1) * (z - z1); if (d < best) { best = d; ph = H + c - x0; }
  c = z < z0 ? z0 : (z > z1 ? z1 : z); d = (x - x1) * (x - x1) + (z - c) * (z - c); if (d < best) { best = d; ph = H + W + z1 - c; }
  c = x < x0 ? x0 : (x > x1 ? x1 : x); d = (x - c) * (x - c) + (z - z0) * (z - z0); if (d < best) { best = d; ph = 2.f * H + W + x1 - c; }
  return ph;
}

// ---- driver: pure pursuit towards the rail / formation / player, with speed control and simple obstacle braking.
// Must be called while the bot is the active vehicle. Only sets thr, brk and steer.
static void bot_ai(Bot *b, float dt, float plx, float plz, float plhx, float plhz) {
  float hx, hz; heading(&hx, &hz);
  float x = (n[4].x + n[5].x) * .5f, z = (n[4].z + n[5].z) * .5f, rx = hz, rz = -hx, vf = 0;
  for (int i = 0; i < 20; i++) vf += (n[i].vx * hx + n[i].vz * hz) * .05f;
  int police = b->src == PSRC; float tx, tz, vt;
  if (police) { tx = plx; tz = plz; float dx = tx - x, dz = tz - z, d = fsqrt(dx * dx + dz * dz); vt = d > 8.f ? 12.f : 6.f; }
  else if (trafficBehavior == 1) {
    int i = b->src; float distance = 5.f + (i / 4) * 2.5f, side = ((i % 4) - 1.5f) * 2.f;
    tx = plx - plhx * distance + plhz * side; tz = plz - plhz * distance - plhx * side;
    if (tx < -CITY_X_LIMIT + 3.f) tx = -CITY_X_LIMIT + 3.f; if (tx > CITY_X_LIMIT - 3.f) tx = CITY_X_LIMIT - 3.f;
    if (tz < -CITY_Z_LIMIT + 3.f) tz = -CITY_Z_LIMIT + 3.f; if (tz > CITY_Z_LIMIT - 3.f) tz = CITY_Z_LIMIT - 3.f;
    float dx = tx - x, dz = tz - z, d = fsqrt(dx * dx + dz * dz); vt = d * 1.3f; if (vt > 16.f) vt = 16.f; if (d < 2.f) vt = 0.f;
  } else {
    int mode = rail_mode_of(b->src); float ph = rail_phase(mode, x, z), look = 6.f + (vf > 0 ? vf : 0) * .6f;
    rail_point(mode, ph + look, &tx, &tz); vt = traffic[b->src].speed * (mode == 2 ? .7f : 1.f);
    float fx2, fz2; rail_point(mode, ph + look + 9.f, &fx2, &fz2);          // what is coming after: brake before the bend, not in it
    float ax = fx2 - x, az = fz2 - z, al = fsqrt(ax * ax + az * az) + 1e-3f, ar = (ax * rx + az * rz) / al;
    if (absf_(ar) > .35f && vt > 4.f) vt = 4.f;
  }
  float vt0 = vt, dx = tx - x, dz = tz - z, dl = fsqrt(dx * dx + dz * dz) + 1e-3f; dx /= dl; dz /= dl;
  float er = dx * rx + dz * rz, ef = dx * hx + dz * hz;                 // target direction: right / forward components
  float st = er > 1.f ? 1.f : (er < -1.f ? -1.f : er); if (ef < 0.f) st = er >= 0.f ? 1.f : -1.f;
  if ((absf_(er) > .3f || ef < .3f) && vt > 4.f) vt = 4.f;               // slow down for corners
  if ((absf_(er) > .7f || ef < 0.f) && vt > 3.f) vt = 3.f;
  if (b->stuckT < 4.f) {                                                  // brake for what is in front (ignored when stuck so it can push through)
    for (int k = -1; k < MAX_PHYS; k++) {
      float ox, oz;
      if (k < 0) { if (police || trafficBehavior == 1) continue; ox = plx; oz = plz; }
      else { if (!bots[k].used || &bots[k] == b) continue; ox = (bots[k].nd[4].x + bots[k].nd[5].x) * .5f; oz = (bots[k].nd[4].z + bots[k].nd[5].z) * .5f; }
      float ex = ox - x, ez = oz - z, f = ex * hx + ez * hz, l = ex * rx + ez * rz;
      if (f > 0.f && f < 11.f && l < 2.4f && l > -2.4f) { float a = (f - 5.8f) * 1.2f; if (a < 0.f) a = 0.f; if (a < vt) vt = a; }
    }
  }
  if (absf_(vf) < .4f && vt0 > 2.f) b->stuckT += dt; else if (absf_(vf) > 1.f) b->stuckT = 0.f;
  int reversing = b->stuckT > 7.f && b->stuckT < 8.5f; if (b->stuckT >= 8.5f) b->stuckT = 0.f;
  if (driveEff < .05f || exploded || fuel <= .005f) { thr = 0.f; brk = .15f; }   // wreck: dead engine, parked
  else if (reversing) { thr = -.6f; brk = 0.f; st = -st; }
  else {
    float e = vt - vf; thr = e > 0.f ? (e * .6f > 1.f ? 1.f : e * .6f) : 0.f;
    brk = e < -1.f ? ((-e - 1.f) * .15f > 1.f ? 1.f : (-e - 1.f) * .15f) : 0.f;
  }
  float smax = .5f / (1.f + absf_(vf) * .05f), k = dt * 6.f > .5f ? .5f : dt * 6.f;
  b->steerS += (st * smax - b->steerS) * k; steer = b->steerS;
}

static int bot_clear_at(float x, float z, float *plc) {     // free space to spawn a car? plc = player center
  float dx = x - plc[0], dz = z - plc[1], lim = ntr >= 0 ? 10.f : 6.5f;
  if (dx * dx + dz * dz < lim * lim) return 0;
  if (ntr >= 0) { dx = x - n[ntr + 4].x; dz = z - n[ntr + 4].z; if (dx * dx + dz * dz < 36.f) return 0; }
  for (int i = 0; i < MAX_PHYS; i++) if (bots[i].used) { dx = x - (bots[i].nd[4].x + bots[i].nd[5].x) * .5f; dz = z - (bots[i].nd[4].z + bots[i].nd[5].z) * .5f; if (dx * dx + dz * dz < 36.f) return 0; }
  return 1;
}
static void bot_spawn(int slot, int src, float x, float z, float hx, float hz, float speed) {
  static const uint8_t CR[8] = {200, 40, 215, 185, 65, 230, 110, 175}, CG[8] = {48, 135, 170, 90, 165, 180, 90, 140}, CB[8] = {40, 55, 45, 40, 70, 65, 180, 60};
  Bot *b = &bots[slot]; Ctx *c = &b->c; int police = src == PSRC;
  b->used = 1; b->src = (uint8_t)src; b->spin = b->stuckT = b->steerS = 0.f;
  b->veh = VEH[police ? 1 : src & 3];                                    // berline / sport / pick-up / buggy
  if (police) { b->veh.r = 235; b->veh.g = 235; b->veh.b = 228; } else { b->veh.r = CR[src & 7]; b->veh.g = CG[src & 7]; b->veh.b = CB[src & 7]; }
  for (unsigned i = 0; i < sizeof(Ctx); i++) ((char *)c)[i] = 0;
  c->n = b->nd; c->bm = b->bd; c->nbCap = BOT_MB; c->cv = NV; c->cw = 0; c->cs = 1; c->ce = police ? 2 : (src & 1); c->cc = 0; c->ntr = -1;
  bot_enter(b);
  car_init(x, z, hx, hz, .12f, 0);
  for (int i = 0; i < nn; i++) { n[i].vx = hx * speed; n[i].vz = hz * speed; }
  bot_leave(b);
}
static void bot_release(Bot *b) {
  TrafficCar *car = b->src == PSRC ? &policeCar : &traffic[b->src];
  if (b->src != PSRC && trafficBehavior != 1) car->phase = rail_phase(rail_mode_of(b->src), car->x, car->z);   // back on the nearest rail point
  car->bot = 0; b->used = 0;
}

// once per frame: release far bots, drive and update the active ones, promote at most one new rail car
static void bots_frame(float dt) {
  if (mapId != 3) return;
  float hx0, hz0; heading(&hx0, &hz0);
  float plc[2] = {(n[4].x + n[5].x) * .5f, (n[4].z + n[5].z) * .5f};
  for (int s = 0; s < MAX_PHYS; s++) {
    Bot *b = &bots[s]; if (!b->used) continue;
    int police = b->src == PSRC; TrafficCar *car = police ? &policeCar : &traffic[b->src];
    if (police ? !pursuitEnabled : (!trafficEnabled || b->src >= trafficCount)) { bot_release(b); continue; }
    bot_enter(b);
    bot_ai(b, dt, plc[0], plc[1], hx0, hz0);
    fx_update(dt);
    float hx, hz; heading(&hx, &hz);
    float cx = (n[4].x + n[5].x) * .5f, cz = (n[4].z + n[5].z) * .5f, cy = n[4].y, vf = 0;
    for (int i = 0; i < 20; i++) vf += (n[i].vx * hx + n[i].vz * hz) * .05f;
    int wreck = dmg > 55.f || burning || exploded || driveEff < .1f;
    b->spin += vf * dt / .35f;
    bot_leave(b);
    car->x = cx; car->z = cz; car->hx = hx; car->hz = hz;                // mirror for the rest of the game (culling, sorting)
    float dx = cx - plc[0], dz = cz - plc[1], lim = wreck ? PHYS_WRECK_FAR : PHYS_FAR;
    if (dx * dx + dz * dz > lim * lim || cy < -20.f) bot_release(b);
  }
  if (dt <= 0.f) return;
  int slot = -1; for (int s = 0; s < MAX_PHYS; s++) if (!bots[s].used) { slot = s; break; }
  if (slot < 0) return;
  if (trafficEnabled) for (int i = 0; i < trafficCount; i++) {
    TrafficCar *car = &traffic[i]; if (car->bot) continue;
    float dx = car->x - plc[0], dz = car->z - plc[1], hl = car->hx * car->hx + car->hz * car->hz;
    if (dx * dx + dz * dz > PHYS_NEAR * PHYS_NEAR || hl < .5f || !bot_clear_at(car->x, car->z, plc)) continue;
    float v = trafficBehavior == 1 ? 0.f : car->speed * (trafficBehavior == 2 ? .7f : 1.f);
    bot_spawn(slot, i, car->x, car->z, car->hx, car->hz, v); car->bot = (uint8_t)(slot + 1); return;
  }
  if (pursuitEnabled && !policeCar.bot) {
    float dx = policeCar.x - plc[0], dz = policeCar.z - plc[1];
    if (dx * dx + dz * dz < (PHYS_NEAR + 4.f) * (PHYS_NEAR + 4.f) && bot_clear_at(policeCar.x, policeCar.z, plc)) {
      bot_spawn(slot, PSRC, policeCar.x, policeCar.z, policeCar.hx, policeCar.hz, 6.f); policeCar.bot = (uint8_t)(slot + 1);
    }
  }
}

// ---- car / car contacts: the nodes of one car against the beams of the other (same scheme as objects vs car in step())
typedef struct { Node *n; Beam *bm; int nbc, ntr; float cx, cz, rad; } VRef;
static void node_vs_car(Node *p, const VRef *B) {
  float dcx = p->x - B->cx, dcz = p->z - B->cz; if (dcx * dcx + dcz * dcz > B->rad * B->rad) return;
  for (int i = 0; i < B->nbc; i++) {
    Beam *b = &B->bm[i]; if (b->f > 1) continue;
    Node *a = &B->n[b->a], *q = &B->n[b->b];
    float ex = q->x - a->x, ey = q->y - a->y, ez = q->z - a->z, el = ex * ex + ey * ey + ez * ez + 1e-6f;
    float t = ((p->x - a->x) * ex + (p->y - a->y) * ey + (p->z - a->z) * ez) / el; t = t < 0 ? 0 : (t > 1 ? 1 : t);
    float dx = p->x - (a->x + t * ex), dy = p->y - (a->y + t * ey), dz = p->z - (a->z + t * ez), d2 = dx * dx + dy * dy + dz * dz;
    if (d2 >= CAR_R * CAR_R) continue;
    float d = fsqrt(d2) + 1e-5f, nx = dx / d, ny = dy / d, nz = dz / d, pen = CAR_R - d; if (pen > .15f) pen = .15f;
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
static void car_hits(const VRef *A, const VRef *B) {
  for (int i = 0; i < 50; i++) node_vs_car(&A->n[i], B);                // structure, panels, wheels (not the engine / seats inside)
  if (A->ntr >= 0) for (int i = A->ntr; i < A->ntr + 11; i++) node_vs_car(&A->n[i], B);
}
static void cars_collide(void) {
  VRef v[MAX_PHYS + 1]; int m = 0;
  v[m++] = (VRef){n, bm, nbc, ntr, (n[4].x + n[5].x) * .5f, (n[4].z + n[5].z) * .5f, ntr >= 0 ? 10.f : 3.8f};
  for (int s = 0; s < MAX_PHYS; s++) if (bots[s].used) v[m++] = (VRef){bots[s].nd, bots[s].bd, bots[s].c.nbc, -1, (bots[s].nd[4].x + bots[s].nd[5].x) * .5f, (bots[s].nd[4].z + bots[s].nd[5].z) * .5f, 3.8f};
  for (int a = 0; a < m; a++) for (int c = a + 1; c < m; c++) {
    float dx = v[a].cx - v[c].cx, dz = v[a].cz - v[c].cz, lim = v[a].rad + v[c].rad + .5f;
    if (dx * dx + dz * dz > lim * lim) continue;
    car_hits(&v[a], &v[c]); car_hits(&v[c], &v[a]);
  }
}
// one physics sub-step for the whole scene: the player, every active bot, then the contacts between cars
static void world_step(float dt) {
  step(dt);
  if (!bots_active()) return;
  ctx_save(&pctx);
  for (int s = 0; s < MAX_PHYS; s++) if (bots[s].used) { Bot *b = &bots[s]; ctx_load(&b->c); step(dt); ctx_save(&b->c); }
  ctx_load(&pctx);
  cars_collide();
}

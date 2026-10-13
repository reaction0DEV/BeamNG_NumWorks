> 🌍 **Version Française disponible !** Pour lire la documentation en français, [cliquez ici](./NumBeam3D_FR/README.md).

---

# NumBeam3D

Driving and crash simulator in 3D with **beam-based deformation physics** (BeamNG style), written in C for the **NumWorks** calculator (external application, `.nwa`).
Everything runs in real time on the calculator: software 3D rendering, deformable body physics, traffic, weather, challenges and record saving.

> This is the **English version**: the in-game text is in English and ASCII only (the calculator font has no accents). The French version is in the `NumBeam3D_FR` folder.

# Download

📥 **Download NumBeam 3D :** [Click here to download the latest version(`EN.NumBeam3D.nwa`)](https://github.com/reaction0DEV/BeamNG_NumWorks/releases/download/V1.1/EN.NumBeam3D.nwa)


## Highlights

- **Deformable body**: the car is a mesh of nodes and beams. It bends, breaks, and loses its windows, mirrors and bumpers.
- **Mechanical systems**: radiator, fuel tank, gearbox, driveshaft, tyres (punctures), fuel, fire and explosion.
- **6 maps**: Circuit, Road, Crash-test, City, Empty (ramps and loops) and **Italy** (endless country road, villages, Tuscan hills).
- **16 vehicles**, 4 engines, 4 wheel types, 4 suspensions, 3 chassis, 4 trailers.
- **Challenges**: timed deliveries, crash tests, tutorial, 29 achievements, lifetime statistics.
- **Python mods**: add your own vehicles, maps, powers and game modes with simple scripts, no recompiling needed (see [`MODS_MANUAL.md`](modding/MODS_MANUAL.md)).
- **Developer mode**: 10 adjustable physics variables (gravity, grip, max speed, turbo...), with an optional **safety unlock** to type any value.
- **Minimap**: in the City and now also in Italy (scrolling map centred on the car).
- **Road objects option**: cones, crates, barrels and tyre walls can be removed (Options -> World, or the quick menu).

The full list of what the program contains is in [`CONTENT.md`](info/CONTENT.md).

## Repository contents

| File | Role |
|---|---|
| `src/main.c` | Main program: rendering, interface, menus, challenges, mods |
| `src/sim.h` | Simulation engine: physics, maps, traffic, events, bots |
| `src/epsilon_app.h` | Calculator protection (Home key) for an external application |
| `src/epsilon_files.h` | Reading/writing files in Epsilon's file system (saves) |
| `icon.png` | Application icon |
| `Makefile` | Build and install |
| `info/BUILD.md` | Step-by-step guide to build and install |
| `modding/MODS_MANUAL.md` | Full manual for Python mods |
| `modding/example_mods.py` | Example mod script |
| `info/CONTENT.md` | Inventory of everything in the game |

## Building

Requirements: `arm-none-eabi-gcc`, `arm-none-eabi-binutils` and Node.js (for `nwlink`, downloaded automatically by `npx`).

```sh
make          # builds output/numbeam3d.nwa
make check    # checks the links and prints the installed size
make run      # installs on the calculator plugged in via USB
make clean    # removes output/
```

Detailed guide (Windows, macOS, Linux, troubleshooting): [`BUILD.md`](info/BUILD.md).

The Makefile expects the sources in `src/` (`main.c`, `sim.h`, `epsilon_app.h`, `epsilon_files.h`) and `icon.png` at the root, next to the `Makefile`.

## Installation

1. Build (`make`) or get `numbeam3d.nwa`.
2. Plug the NumWorks in via USB, then `make run` (or send the `.nwa` from [my.numworks.com](https://my.numworks.com)).
3. Launch the **NumBeam3D** application from the home screen.

Records, statistics, achievements, favorites and settings are saved in the calculator's file system.

## Default controls

All keys can be remapped (Options -> Driving -> Controls).

| Action | Key |
|---|---|
| Accelerate / Brake | Up / Down |
| Left / Right | Left / Right |
| Reset / replay | OK |
| Hood / Doors / Trunk | Var / x,n,t / Exp |
| Open or close everything | Shift |
| Beams (wireframe) | Toolbox |
| Top view / Damage | Ln / Log |
| Camera left, right, up, down | 4, 6, 8, 2 |
| Zoom + / - | 7 / 9 |
| Camera reset | 5 |
| Quick menu (favorites) | EXE |
| Turbo (hold) | Clear |
| Cockpit view | 0 |
| Gear up / down | 3 / 1 |
| Auto / manual gearbox | - |
| Clutch / Handbrake (hold) | Alpha / Power |
| Minimap | Sin |
| Fuel (hold) | Cos |
| Time: stop / slow / speed up | Sqrt / ( / ) |
| Repeat the last quick option | + |
| Favorites (direct access) | Comma |
| Tutorial: next step | Dot |

## Python mods

Write a script in the calculator's **Python** application, start it with `# numbeam`, and the game reads it at startup. Options can be written in French or in English:

```python
# numbeam
vehicle("Rocket", 0.9, 0.8, 1.3, 0.8, 2.0, 1.1, color=(255, 40, 120))
map("Arena", ground=(90, 170, 90))
ramp(0, 8, 30, 45, 0, 6)
power("Moon", gravity=3, duration=15)
mode("Moon race", map="Arena", vehicle="Rocket", gravity=6)
```

Limits: 8 vehicles, 4 maps, 8 powers, 8 modes. Details in [`MODS_MANUAL.md`](modding/MODS_MANUAL.md).

## Technical notes

- C11, `-Os`, single-precision maths, no dynamic allocation (no `malloc`).
- Software 3D rendering into a 16-bit buffer (RGB565), depth sorted, with weather (rain, fog, black ice).
- The Italy map is generated on the fly in 30 m cells (deterministic and cached): the road can go on forever at no memory cost.
- Custom mod maps use the Empty map engine.

## Licence

To be defined by the author (no licence is provided with the sources).

# NumBeam3D — Mods manual (Python scripts)

You can add **your own vehicles, maps, powers and game modes** by writing small scripts in the NumWorks **Python** application. The game reads them at startup: no recompiling or reinstalling needed.

> **Important:** the game does *not run* your script (the NumWorks has no Python inside external apps). It **reads** it as a list of declarations. So you write "configuration" Python: function calls, variables and a little arithmetic. No `for`, `if`, `def`, or complex lists.

---

## 1. Installing a mod

1. Open the calculator's **Python** application -> **Add a script** (or send a file from a computer with [my.numworks.com/python](https://my.numworks.com/python)).
2. Give it any name you like (e.g. `mymap`). The `.py` extension is automatic.
3. **The first line must be `# numbeam`** (case does not matter). Without this line the file is ignored (your other Python scripts are never touched).
4. Write your declarations (see below), save, then launch the game.
5. In the game: **Challenges -> Custom modes** (or **Options -> Custom modes**). The first line shows what was loaded, for example `Scripts: 2  V3 M1 P4 G2` (2 files, 3 vehicles, 1 map, 4 powers, 2 game modes).
6. Edited a script while the game is open? Go back to **Custom modes** and choose **Reload scripts**. (Quitting the game to go to Python restarts the game anyway.)

A full example is supplied: `example_mods.py`.

**If an error is detected**, the first line of the page shows `! file l.12: message` (file name, line number). Press OK on that line to read it again. Only the *first* error is reported: fix it, reload, and the next one appears.

---

## 2. Writing a script

```python
# numbeam
road_width = 8             # a variable (number)
height = 2 * 3             # calculations allowed: + - * / ** and parentheses

vehicle("Rocket", 0.9, 0.8, 1.3, 0.8, 2.0, 1.1, color=(255, 40, 120))
map("Arena", ground=(90, 170, 90))
ramp(0, road_width, 30, 45, 0, height)
```

Rules:

- One statement = `function(arguments)` or `name = number`. A call can span several lines as long as the parenthesis is not closed.
- Arguments **in order** (positional) or **by name**: `ramp(0, 8, 30, 45, 0, 6)` and `ramp(x=0, width=8, z0=30, z1=45, h0=0, h1=6)` are equivalent. You can mix them (positional first).
- Text between `"quotes"` or `'apostrophes'`. **No accents** in function and option names. Avoid them in displayed text too (the game's font does not support them).
- Colors and positions are tuples: `(255, 0, 0)`, `(x, z)`.
- `True`, `False`, `None` and `pi` exist. `#` starts a comment. `import ...` / `from ... import ...` lines are ignored.
- **Function and option names can be written in English or in French**, and you can mix both. They are not case sensitive. **Unknown option names are reported** (typo detection).
- Out-of-range values are **brought back to the limit** without an error.
- Names of equipment (engines, wheels, objects, maps, vehicles) are the ones shown **in the game**; this English version of the game uses English names (`"Off-road"`, `"barrel"`...).

### English and French option names

| English | French | English | French |
|---|---|---|---|
| `name` | `nom` | `width` | `largeur` |
| `height` | `hauteur` | `length` | `longueur` |
| `mass` | `masse` | `power` | `puissance` |
| `color` / `colour` | `couleur` | `thruster` | `propulseur` |
| `ground` | `sol` | `start` | `depart` |
| `hills` | `collines` | `gravity` | `gravite` |
| `traction` | `adherence` | `radius` | `rayon` |
| `jump` | `saut` | `repair` | `reparer` |
| `fuel` | `essence` | `duration` | `duree` |
| `map` | `carte` | `vehicle` | `vehicule` |
| `engine` | `moteur` | `wheels` | `roues` |
| `trailer` | `remorque` | `weather` | `meteo` |
| `traffic` | `trafic` | `max_speed` | `vitesse_max` |
| `lateral_grip` | `adherence_laterale` | `turbo_speed` | `turbo_vitesse` |
| `braking` | `freinage` | `steering` | `braquage` |
| `engine_braking` | `frein_moteur` | | |

Options that are the same in both languages: `grip`, `boost`, `stop`, `tp`, `turbo`, `turbo_acceleration`, `acceleration`, `amplitude`, `type`, `x`, `z`, `z0`, `z1`, `h0`, `h1`.

### Limits

| Item | Maximum |
|---|---|
| Custom vehicles | 8 |
| Custom maps | 4 |
| Ramps per map | 16 |
| Loops per map | 4 |
| Objects per map | 12 |
| Powers | 8 |
| Modes | 8 |
| Variables | 16 |
| Displayed name | 15 characters |

---

## 3. The functions

### `vehicle(name, width, height, length, mass, power, grip, color=(r,g,b), thruster=0)`

Adds a vehicle to the **Garage** (*Car* line) and to the *Car* quick option. Everything is a **coefficient** relative to the Sedan (1 = identical).

| Option | Default | Range | Effect |
|---|---|---|---|
| `name` | `"Mod"` | 15 chars | displayed name |
| `width`, `height`, `length` | 1 | 0.3 – 3 | body dimensions |
| `mass` | 1 | 0.3 – 4 | higher = heavier |
| `power` | 1 | 0.3 – 3 | multiplies the engine's acceleration |
| `grip` | 1 | 0.3 – 2 | tyre grip |
| `color` | grey | 0 – 255 | body color |
| `thruster` | 0 | 0 or 1 | 1 = thrusters in turbo (like the Dragster) |

The engine, wheels, suspension and chassis are chosen as usual in the Garage (or through a `mode`).

### `map(name, ground=(r,g,b), start=(x,z), hills=(height,length), gravity=, traction=)`

Creates a map. It is chosen like the others (*Map* line of the main menu or the *Map* quick option) and is built on the flat terrain of the **Empty** map. The functions `ramp`, `hills`, `loop`, `object`, `start` that follow apply to **this map** (until the next `map`).

| Option | Default | Effect |
|---|---|---|
| `ground` | white checkerboard | ground color (checkerboard of two close shades) |
| `start` | `(0, 0)` | where the car appears (x = side, z = forward) |
| `hills` | none | rolling relief: max height (0 – 8 m) and wavelength (3 – 200 m) |
| `gravity` | the options' value | map gravity (1 – 80); 0 or absent = normal setting |
| `traction` | the options' value | road grip (0 – 3) |

The car faces **increasing z**. Custom maps keep the scenery of the Empty map (nothing more), with no city or circuit.

### `ramp(x, width, z0, z1, h0, h1)`

A ramp (or jump, sloped wall, bridge...): centred on `x`, `width` metres wide, running from `z0` to `z1` forward, and whose height goes linearly from `h0` to `h1` (0 – 40 m). Two ramps end to end make a jump; `h0 > h1` makes a descent. Ramps are orange and black.

### `hills(amplitude, length)`

Variant of the `hills` option of `map`, written on a separate line.

### `loop(x, z, radius)`

A vertical loop (radius 3 to 15 m) at the given position. It works like those of the Empty map (*Ramps* option enabled in the options). Without `loop`, your map has none.

### `object(type, x, z)`

Places a physical object at the start: `"cone"`, `"wood crate"`, `"steel crate"`, `"concrete block"`, `"barrel"` (explosive), `"tyres"`, `"pylon"` (or the number 0 – 6). You can write only the beginning of the name (`"wood"` gives the wooden crate). The *Road objects* option must be enabled.

### `start(x, z)`

Same effect as the `start` option of `map`.

### `power(name, ...)`

A **power** is activated **in game**: open the menu (quick-options key / pause) -> **Options -> Custom modes**, choose the power and press OK. You can also add it to your **favorites** with Shift to have it in the quick menu.

Instant effects (can be combined):

| Option | Range | Effect |
|---|---|---|
| `jump=` | 0 – 40 | upward impulse |
| `boost=` | 0 – 60 | forward impulse |
| `stop=1` | 0 / 1 | stops the car |
| `repair=1` | 0 / 1 | repairs the car |
| `fuel=1` | 0 / 1 | fills the tank |
| `tp=(x, z)` | | teleports the car to that spot |

**Temporary** effects: `duration=` (seconds, 0 – 120) during which these apply:

- `turbo=1`: forced turbo,
- any **setting** from the list in chapter 4 (e.g. `gravity=3`, `max_speed=250`).

If you give a setting or `turbo=1` without `duration`, the duration is **10 s**. A temporary power that is already active is replaced by the new one.

### `mode(name, ...)`

A **mode** groups a map, a vehicle, equipment and settings, and **starts the game directly** (menu **Challenges -> Custom modes**, or a favorite). Everything is optional:

| Option | Value | Effect |
|---|---|---|
| `map=` | name (`"Arena"`, `"City"`, `"Circuit"`...) or number | map to use |
| `vehicle=` | name (`"Rocket"`, `"Truck"`...) or number | vehicle |
| `engine=` | `"4 cyl"`, `"V6"`, `"V8"`, `"Turbo"` | engine |
| `wheels=` | `"Road"`, `"Sport"`, `"Off-road"`, `"Mini"` | wheels |
| `suspension=` | `"Soft"`, `"Normal"`, `"Firm"`, `"Race"` | suspension |
| `chassis=` | `"Steel"`, `"Reinforced"`, `"Light"` | chassis |
| `trailer=` | `"none"`, `"Simple"`, `"Tanker"`, `"Flatbed"`, `"Caravan"` | trailer |
| `weather=` | 0 – 3 | weather (like the game option) |
| `traffic=` | 0 – 64 | number of traffic cars (0 = none) |
| + settings | see chapter 4 | imposed for the whole mode |

Names do not have to be complete (`"off"` is enough for `"Off-road"`), and case does not matter. The map and the vehicle can be defined in the same script **or in another one**.

The mode stays active until you return to the **main menu**. The mode's settings are **also reset** at that point.

---

## 4. The settings (max speed, gravity, etc.)

These are the same variables as the **Options -> System -> Dev variables** page. They are used in `power(...)` and `mode(...)`:

| Option | Unit | Range | Role |
|---|---|---|---|
| `max_speed` | km/h | 3 – 150 | the engine's maximum speed |
| `acceleration` | – | 1 – 100 | acceleration power |
| `lateral_grip` | – | 0.02 – 2 | road holding in corners |
| `gravity` | – | 0 – 80 | gravity (14 = normal) |
| `traction` | – | 0.05 – 3 | road grip |
| `turbo_speed` | × | 1 – 5 | turbo speed multiplier |
| `turbo_acceleration` | × | 1 – 10 | turbo acceleration multiplier |
| `braking` | × | 0 – 10 | brake force |
| `steering` | – | 0.05 – 1 | base steering angle |
| `engine_braking` | – | 0 – 0.2 | engine braking when releasing the throttle |

These ranges apply to scripts even when the **safety unlock** of the developer mode is on (the unlock only concerns values typed by hand in the Dev variables page).

**Priority** if several sources act at the same time: a **mode**'s settings < **developer mode** (your manual settings) < active **power**.

---

## 5. Quick examples

**A fast, light vehicle**

```python
# numbeam
vehicle("Lightning", 0.9, 0.8, 1.2, 0.7, 2.5, 1.2, color=(255, 220, 0))
```

**A jump map**

```python
# numbeam
map("Jumps", ground=(200, 170, 110), hills=(1, 50))
h = 5
ramp(0, 8, 30, 45, 0, h)        # up
ramp(0, 8, 45, 46, h, h)        # plateau
ramp(0, 8, 46, 60, h, 0)        # down
loop(0, 120, 8)
```

**A "moon gravity" power and a mode that uses it**

```python
# numbeam
power("Moon", gravity=3, duration=15)
mode("Moon race", map="Jumps", vehicle="Lightning", gravity=6, max_speed=200)
```

---

## 6. Troubleshooting

| Symptom | Likely cause |
|---|---|
| The script is not picked up | the first line is not `# numbeam` |
| `option xxx` | option name misspelled (see the tables above) |
| `unknown name` | variable used before it is defined, or misspelled function |
| `missing quote` | a text is not closed on its line |
| `comma expected` | comma forgotten between two arguments, or unclosed parenthesis |
| `ramp without map` | you need a `map(...)` before `ramp`, `loop`, `object`, `start` |
| `max 8 vehicles` (etc.) | limit reached (see chapter 2) |
| A vehicle / map does not appear | `Reload scripts`, then check the `Scripts: ...` line |
| The game runs out of memory | reduce the number of ramps / objects, or use fewer mods |

## 7. What is not possible (for now)

- `for` loops, `if` conditions, `def` functions and lists: the script is a list of declarations, not a program.
- New 3D body shapes, textures, sounds, new engines/wheels/chassis (you choose among the game's).
- Custom cities, circuits or roads: custom maps rely on the flat terrain of the Empty map, with ramps, hills, loops and objects.
- "On the fly" power effects other than those listed (jump, boost, stop, repair, fuel, teleport, turbo and temporary settings).

# NumBeam3D — everything the program contains

Inventory drawn up from the source code (`main.c`, `sim.h`). The names are those shown in the game.

## 1. Maps (6 built in + custom maps)

| Map | Description |
|---|---|
| **Circuit** | Track with a chequered start/finish line and a timer: lap time and best lap |
| **Road** | Straight road with centre and edge markings, speed bumps |
| **Crash-test** | Car launched into an obstacle at a chosen speed (30 / 50 / 80 / 110 / 140 km/h), impact score |
| **City** | City of blocks (regular, offset or channeled layout), buildings, traffic lights, speed bumps, petrol stations, traffic, police, random events, deliveries |
| **Empty** | Flat test terrain: ramps, jumps and vertical loops (*Ramps* option), physical objects |
| **Italy** | Endless winding country road, Tuscan hills, a village every 360 m (houses, church with bell tower, fountain), cypresses, umbrella pines, farms, villas with towers, Roman ruins, olive groves, vineyards, hay bales, chapels, low walls, distant mountains; cones, crates, barrels and a tyre wall on the road at the start |
| **Custom maps** | Up to 4 maps created with a Python script (on the Empty map terrain) |

## 2. Vehicles (16)

Sedan, Sport, Pickup, Buggy, **Gen #** (randomly generated car from a seed), City car, 4x4, SUV, Muscle, Van, Truck, Kart, Rally, Limo, Dragster (thrusters in turbo), Taxi.
Plus up to 8 custom vehicles through mods.

### Equipment
- **Engines (4)**: 4 cyl, V6, V8, Turbo
- **Wheels (4)**: Road, Sport, Off-road, Mini
- **Suspensions (4)**: Soft, Normal, Firm, Race
- **Chassis (3)**: Steel, Reinforced, Light
- **Trailers (5)**: none, Simple, Tanker (sloshing liquid), Flatbed (loose crates), Caravan
- **Ground/obstacle sturdiness**: Sturdy, Normal, Fragile

## 3. Physics and damage

- Body made of deformable **nodes and beams** (plastic deformation, breaking)
- Windows that crack then shatter, mirrors that hang then tear off, bumpers that detach
- Hood, doors and trunk that open (in the garage and in game)
- **Systems**: radiator (leak, steam), fuel tank (leak, fire, explosion), driveshaft, gearbox, differential, axles, exhaust
- **Tyres**: puncture after a hard landing, sagging wheel, loss of grip
- Fire, smoke, explosion fireball, sparks, dust, debris
- Tyre marks (braking, skidding, burn-out)
- **Airbags** (front, side) and **seat belt**, risk for the occupants
- **Automatic or manual** gearbox, clutch, handbrake, engine braking, turbo
- Fuel and consumption, filling at petrol stations (10 s from empty to full)
- Road grip (Dry / Wet / Ice) and gravity (Normal / Moon / Strong)
- Weather: Clear, Rain, Fog, Ice
- Drifting, reversing, jump, boost

## 4. World and AI

- **AI traffic**: traffic cars on rails that become physical (bots) near the player; count, behavior (Regular / Follows player / Patrol) and speed (from very slow to very fast)
- **Police chase** (option)
- **Traffic lights** (green 6 s, orange 2 s, red 6 s, offset per junction)
- **Random events** in the city: crashed cars, ambulance and police, barriers, roadworks, works truck, convoy on the ring roads
- **Physical objects**: Cone, Wood crate, Steel crate, Concrete block, Barrel (explosive), Tyres, Pylon. They can be removed with the **Road objects** option
- **Ramps and loops** on the Empty map

## 5. Game modes and challenges

- Free **Play** on all maps
- Timed **Deliveries** in the city (several parcels, one countdown per parcel), 3 levels: Easy, Normal, Expert, with stars and best scores
- **Crash test** at an adjustable speed
- **Circuit lap** with lap time
- Guided **Tutorial** in 13 steps (accelerate, steer, brake, camera, hood/doors/trunk, damage, repair, turbo)
- **Custom modes** (Python scripts)
- **Time control**: STOP, x0.1, x0.25, x0.5, x1, x2

## 6. Achievements (29) and statistics

First crash, Little ride, Courier, Express reverse, Drifter, First lap, Pump attendant, Boom!, Pedal to the metal, Road tripper, Wrecker, Reliable courier, Drift king, Smoking tyres, Daredevil, Elite driver, Marathon, Globetrotter, Light speed, Extreme reverse, Demolisher, Drift master, Endless drift, Legendary courier, Perfect expert, Loop, Zen driving, Collector, Platinum.

**Lifetime statistics**: distance, crashes, top speed, drift time (total and best), deliveries, max reverse speed, fill-ups, explosions, loops, laps, vehicles driven. Saved with the records (`numbeam.sav`).

## 7. Views and display

- Free camera (left/right/up/down, zoom, reset), **cockpit** view, **top** view with damage
- **Wireframe** display of the beams
- **Minimap** of the city (corner or full screen, small/normal/large icons): car, destination, police, events; on the **Italy** map, a scrolling minimap centred on the car: road, villages, objects
- HUD: speed, gear, revs, fuel, system indicators
- Quality (Low / Normal / High), field of view (Narrow / Normal / Wide), view distance (Short / Normal / Long), adjustable tyre marks and particles
- 3D garage with a car to inspect

## 8. Menus

- **Main menu**: Play, Map, Garage, Challenges, Profile, Options, Quit
- **Options**: Mode, Vehicle, World, Driving, Display, Tools (in game), System, Custom modes, Favorites
- **Simple / Advanced mode** (Advanced mode shows extra settings)
- **Quick menu** in game with 38 actions: Repair all, Turbo, Boost, Car, Engine, Trailer, Road, Gravity, Time, Back to start, AI traffic, AI count, AI behavior, AI speed, City layout, Weather, Power, Grip, Suspension, Braking, Police chase, Test (puncture > fire > boom), Jump, Stop (emergency brake), Wheels, Map, Gearbox, Flat tyre, Clutch, Seat belt, Airbags, Events, Minimap, Map icons, Breakable body, Fuel use, Fill tank, Road objects
- **Favorites**: 8 customizable slots (Shift to add a line)
- **Remappable controls**: 35 actions bound to the calculator's keys
- **Challenges** and **Profile** pages (Statistics, Achievements)

## 9. Developer mode (dev variables)

Max speed, Acceleration, Lateral grip, Gravity, Overall grip, Turbo speed, Turbo acceleration, Braking, Steering, Engine brake. Saved in a config file separate from the records.

**Safety unlock** (Options -> System -> Dev variables): removes the min/max limits. OK on a variable opens free keyboard entry (0-9, dot, minus sign; Clear fixes a typo, empty field + Clear = default value). The only remaining limit is ±100000, to avoid infinite values. Warning: extreme values can make the physics blow up.

**Unbreakable** (Options -> System -> Dev variables, developer mode on): the player's car cannot break whatever the impact or power. No beam breaks or bends, no punctures, leaks, fire or explosion, and the engine never fails. Part speed is capped at 200 m/s and beam force is bounded so the simulation cannot blow up. If the car was already damaged, use "Repair all" (quick menu). The setting is saved.

## 10. Python script mods

Functions: `vehicle`, `map`, `ramp`, `hills`, `loop`, `object`, `start`, `power`, `mode`, plus variables and simple calculations (the French names `vehicule`, `carte`, `rampe`, `colline`, `boucle`, `objet`, `depart`, `pouvoir` also work).
- **Powers**: jump, boost, stop, repair, fuel, teleport, temporary turbo, temporary settings
- **Modes**: map, vehicle, engine, wheels, suspension, chassis, trailer, weather, traffic and imposed dev settings
- Limits: 8 vehicles, 4 maps (16 ramps, 4 loops, 12 objects each), 8 powers, 8 modes, 16 variables
- Hot reload, errors reported with file name and line number
- Details: `MODS_MANUAL.md`, example: `example_mods.py`

## 11. Saving and system

- Records, statistics and achievements: `numbeam.sav` (with checksum)
- Favorites, Simple/Advanced mode, developer mode: separate config file
- **Home** key protection (the application receives it as a normal key and quits cleanly)
- Compatible with Upsilon; also works in the NumPlay launcher

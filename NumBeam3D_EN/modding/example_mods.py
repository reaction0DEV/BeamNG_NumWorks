# numbeam
# NumBeam3D mods example: vehicles, map, powers and modes.
# This file is read by the game (nothing is executed). The file name is free.

# ---------- Vehicles ----------
# vehicle(name, width, height, length, mass, power, grip, color=(r,g,b))
vehicle("Rocket", 0.9, 0.8, 1.3, 0.8, 2.0, 1.1, color=(255, 40, 120))
vehicle("Tank", 1.4, 1.5, 1.6, 3.0, 1.5, 0.7, color=(70, 90, 50))
vehicle("Jet", 0.85, 0.75, 1.35, 1.0, 2.5, 0.8, color=(250, 250, 0), thruster=1)

# ---------- Map ----------
# map(name, ground=(r,g,b), start=(x,z), hills=(height,length), gravity=, traction=)
map("Arena", ground=(90, 170, 90), start=(0, 0), hills=(1.5, 40), gravity=14)
# ramp(x, width, z_start, z_end, height_start, height_end)
jump = 6
ramp(0, 8, 30, 45, 0, jump)
ramp(0, 8, 70, 90, jump, 0)
ramp(-20, 6, 40, 60, 0, 2 * jump / 3)
loop(12, 120, 7)
object("barrel", 5, 25)
object("cone", -3, 20)
object("tyres", 10, 60)

# ---------- Powers ----------
power("Super jump", jump=14)
power("Rocket!", boost=35, duration=4, turbo=1, turbo_acceleration=4)
power("Moon", gravity=3, duration=8)
power("Return", tp=(0, 0), stop=1, repair=1, fuel=1)

# ---------- Modes (menu: Challenges > Custom modes) ----------
mode("Moon", map="Arena", vehicle="Rocket", gravity=3, max_speed=200)
mode("Tank 4x4", map="Arena", vehicle="Tank", engine="V8", wheels="Off-road", traffic=0, weather=1)

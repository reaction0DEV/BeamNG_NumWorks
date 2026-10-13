# Build and install NumBeam3D

This guide explains how to turn the sources (`main.c`, `sim.h`...) into the `numbeam3d.nwa` application and install it on a NumWorks calculator.

> The commands in this guide come from the supplied `Makefile`. They could not be tested on a real ARM toolchain when the project was merged: if a step fails, see the *Common problems* section.

## 1. What you need

| Tool | Why |
|---|---|
| `arm-none-eabi-gcc` (+ `binutils`, `newlib`) | compiles for the NumWorks processor |
| `make` | runs the build |
| **Node.js** (with `npm`/`npx`) | the Makefile automatically downloads `nwlink@1.0.0` |
| Internet access (the first time) | downloading `nwlink` |
| USB cable | only to install with `make run` |

Expected layout:

```
Makefile
icon.png
src/
  main.c
  sim.h
  epsilon_app.h
  epsilon_files.h
```

## 2. Install the tools

### Windows
1. Install [Node.js](https://nodejs.org) (LTS version).
2. Install the ARM toolchain: [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (choose `arm-none-eabi`) and tick "Add to PATH".
3. Install `make`: the easiest is [MSYS2](https://www.msys2.org) then `pacman -S make`, or `choco install make`.
4. Use a terminal where `arm-none-eabi-gcc` and `make` are recognised (MSYS2 or Git Bash).

### macOS
```sh
brew install node make
brew install --cask gcc-arm-embedded
```

### Linux (Debian / Ubuntu)
```sh
sudo apt install build-essential nodejs npm gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi
```

### Check
```sh
arm-none-eabi-gcc --version
make --version
node --version
```
All three commands must answer.

## 3. Build

In the project folder:

```sh
make
```

Result: `output/numbeam3d.nwa`. The steps shown are `CC` (compilation), `ICON` (icon) then `LD` (linking), followed by the program size.

### Other commands

| Command | Effect |
|---|---|
| `make` | builds `output/numbeam3d.nwa` |
| `make check` | checks the links as on the calculator and prints the installed size (`output/numbeam3d.bin`) |
| `make run` | builds then installs on the calculator plugged in via USB |
| `make clean` | removes the `output/` folder |

## 4. Install on the calculator

### Option A: command line
1. Plug the NumWorks in via USB.
2. Run `make run`.

### Option B: from the browser
1. Build with `make`.
2. Open [my.numworks.com](https://my.numworks.com), go to the section for sending an external application, and send `output/numbeam3d.nwa` (WebUSB-compatible browser, such as Chrome or Edge).

Then launch **NumBeam3D** from the calculator's home screen.

> An external `.nwa` application needs a calculator that accepts this kind of application (compatible firmware, for example Upsilon, or the suitable mode for your model). Check your firmware's compatibility before installing.

## 5. Common problems

| Symptom | Lead |
|---|---|
| `arm-none-eabi-gcc: command not found` | the ARM toolchain is not installed or not in the `PATH` |
| `npx: command not found` | install Node.js |
| Error downloading `nwlink` | check your Internet connection / proxy, then run `make` again |
| `eadk.h: No such file` | `nwlink` could not provide the EADK API: same cause as above, retry with Internet |
| `make: *** No rule to make target` | you are not at the project root (where the `Makefile` is), or a file is missing (see the layout in section 1) |
| `make run` cannot find the calculator | change the cable or USB port, unlock the calculator, close tabs/applications that use it |
| `cannot move location counter backwards` (linker.ld) | the application exceeds the available RAM. Build with `make EXTRA=-DMAX_PHYS=1` (a single physics bot, about 12 KB less), or reduce `PART_MAX` / `SK_MAX` in `src/main.c` |
| The program is too big | `make check` prints the size; reduce the number of mods or added content |
| Compiler warnings | normal (`-Wall -Wextra`): only `error` messages block the build |

## 6. Compile a test on PC (syntax check)

Without a calculator you can only check that the code is syntactically correct, if you have an `eadk.h` (provided by `nwlink`):

```sh
gcc -std=c11 -I<folder_containing_eadk.h> -fsyntax-only src/main.c
```

This does not run the game: running it needs the calculator or its simulator.

## 7. Makefile options

- `NWLINK`: change the linking tool, for example `make NWLINK="nwlink"` if you installed it globally.
- `Q=`: show every command that is run, for example `make Q=`.
- The API level and the application name are defined in `main.c`.

# Compiler et installer NumBeam3D

Ce guide explique comment transformer les sources (`main.c`, `sim.h`…) en application `numbeam3d.nwa` et l'installer sur une calculatrice NumWorks.

> Les commandes de ce guide reprennent celles du `Makefile` fourni. Elles n'ont pas pu être testées sur une vraie chaîne ARM lors de la fusion du projet : si une étape échoue, regarde la section *Problèmes courants*.

## 1. Ce qu'il te faut

| Outil | Pourquoi |
|---|---|
| `arm-none-eabi-gcc` (+ `binutils`, `newlib`) | compile pour le processeur de la NumWorks |
| `make` | lance la compilation |
| **Node.js** (avec `npm`/`npx`) | le Makefile télécharge automatiquement `nwlink@1.0.0` |
| Accès Internet (la première fois) | téléchargement de `nwlink` |
| Câble USB | seulement pour installer avec `make run` |

Arborescence attendue :

```
Makefile
icon.png
src/
  main.c
  sim.h
  epsilon_app.h
  epsilon_files.h
```

## 2. Installer les outils

### Windows
1. Installe [Node.js](https://nodejs.org) (version LTS).
2. Installe la chaîne ARM : [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (choisis `arm-none-eabi`) et coche « Add to PATH ».
3. Installe `make` : le plus simple est [MSYS2](https://www.msys2.org) puis `pacman -S make`, ou `choco install make`.
4. Utilise un terminal où `arm-none-eabi-gcc` et `make` sont reconnus (MSYS2 ou Git Bash).

### macOS
```sh
brew install node make
brew install --cask gcc-arm-embedded
```

### Linux (Debian / Ubuntu)
```sh
sudo apt install build-essential nodejs npm gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi
```

### Vérifier
```sh
arm-none-eabi-gcc --version
make --version
node --version
```
Les trois commandes doivent répondre.

## 3. Compiler

Dans le dossier du projet :

```sh
make
```

Résultat : `output/numbeam3d.nwa`. Les étapes affichées sont `CC` (compilation), `ICON` (icône) puis `LD` (édition de liens), suivies de la taille du programme.

### Autres commandes

| Commande | Effet |
|---|---|
| `make` | compile `output/numbeam3d.nwa` |
| `make check` | contrôle les liens comme sur la calculatrice et affiche la taille installée (`output/numbeam3d.bin`) |
| `make run` | compile puis installe sur la calculatrice branchée en USB |
| `make clean` | supprime le dossier `output/` |

## 4. Installer sur la calculatrice

### Option A : en ligne de commande
1. Branche la NumWorks en USB.
2. Lance `make run`.

### Option B : depuis le navigateur
1. Compile avec `make`.
2. Ouvre [my.numworks.com](https://my.numworks.com), section pour envoyer une application externe, et envoie `output/numbeam3d.nwa` (navigateur compatible WebUSB, comme Chrome ou Edge).

Ensuite, lance **NumBeam3D** depuis l'écran d'accueil de la calculatrice.

> Une application externe `.nwa` nécessite une calculatrice qui accepte ce type d'applications (firmware compatible, par exemple Upsilon, ou le mode adapté de ton modèle). Vérifie la compatibilité de ton firmware avant d'installer.

## 5. Problèmes courants

| Symptôme | Piste |
|---|---|
| `arm-none-eabi-gcc: command not found` | la chaîne ARM n'est pas installée ou pas dans le `PATH` |
| `npx: command not found` | installe Node.js |
| Erreur au téléchargement de `nwlink` | vérifie la connexion Internet / le proxy, puis relance `make` |
| `eadk.h: No such file` | `nwlink` n'a pas pu fournir l'API EADK : même cause que ci-dessus, relance avec Internet |
| `make: *** No rule to make target` | tu n'es pas à la racine du projet (là où est le `Makefile`), ou un fichier manque (voir l'arborescence en section 1) |
| `make run` ne trouve pas la calculatrice | change de câble ou de port USB, déverrouille la calculatrice, ferme les onglets/applications qui l'utilisent |
| `cannot move location counter backwards` (linker.ld) | l'application dépasse la RAM disponible. Compile avec `make EXTRA=-DMAX_PHYS=1` (un seul bot physique, ~12 Ko de moins), ou réduis `PART_MAX` / `SK_MAX` dans `src/main.c` |
| Le programme est trop gros | `make check` affiche la taille ; réduis le nombre de mods ou de contenus ajoutés |
| Avertissements du compilateur | normaux (`-Wall -Wextra`) : seuls les messages `error` bloquent |

## 6. Compiler un test sur PC (vérification de syntaxe)

Sans calculatrice, tu peux seulement vérifier que le code est syntaxiquement correct si tu disposes d'un `eadk.h` (fourni par `nwlink`) :

```sh
gcc -std=c11 -I<dossier_contenant_eadk.h> -fsyntax-only src/main.c
```

Cela ne lance pas le jeu : l'exécution demande la calculatrice ou son simulateur.

## 7. Options du Makefile

- `NWLINK` : changer l'outil de liaison, par exemple `make NWLINK="nwlink"` si tu l'as installé globalement.
- `Q=` : afficher toutes les commandes exécutées, par exemple `make Q=`.
- Le niveau d'API et le nom de l'application sont définis dans `main.c`.

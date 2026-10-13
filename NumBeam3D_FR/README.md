# NumBeam3D

Simulateur de conduite et de crash en 3D avec **physique à déformation par poutres** (style BeamNG), écrit en C pour la calculatrice **NumWorks** (application externe `.nwa`).
Tout tourne en temps réel sur la calculatrice : rendu 3D logiciel, physique de carrosserie déformable, trafic, météo, défis et sauvegarde des records.

> Voici la **version française** : le texte du jeu est en français et en ASCII (la police de la calculatrice ne gère pas les accents). La version anglaise est dans le dossier `NumBeam3D_EN`.

# Télécharger

📥 **Télécharger NumBeam 3D :** [Cliquez ici pour obtenir directement le fichier(`FR.NumBeam3D.nwa`)](https://github.com/reaction0DEV/BeamNG_NumWorks/releases/download/V1.1/FR.NumBeam3D.nwa))


## Points forts

- **Carrosserie déformable** : la voiture est un maillage de nœuds et de poutres. Elle se plie, se casse, perd ses vitres, ses rétroviseurs et ses pare-chocs.
- **Systèmes mécaniques** : radiateur, réservoir, boîte, transmission, pneus (crevaisons), carburant, incendie et explosion.
- **6 cartes** : Circuit, Route, Crash-test, Ville, Vide (rampes et loopings) et **Italie** (route de campagne sans fin, villages, collines toscanes).
- **16 véhicules**, 4 moteurs, 4 types de roues, 4 suspensions, 3 châssis, 4 remorques.
- **Défis** : livraisons chronométrées, crash-tests, tutoriel, 29 succès, statistiques à vie.
- **Mods en Python** : ajoute tes véhicules, cartes, pouvoirs et modes de jeu avec de simples scripts, sans recompiler (voir [`MANUEL_MODS.md`](modding/MANUEL_MODS.md)).
- **Mode développeur** : 10 variables physiques réglables (gravité, adhérence, vitesse max, turbo…), avec un **débridage de sécurité** optionnel pour saisir n'importe quelle valeur.
- **Mini-carte** : en Ville et maintenant aussi en Italie (carte défilante centrée sur la voiture).
- **Option « Objets sur la route »** : cônes, caisses, barils et mur de pneus peuvent être retirés (Options → Monde, ou menu rapide).

La liste complète de ce que contient le programme est dans [`CONTENU.md`](info/CONTENU.md).

## Contenu du dépôt

| Fichier | Rôle |
|---|---|
| `src/main.c` | Programme principal : rendu, interface, menus, défis, mods |
| `src/sim.h` | Moteur de simulation : physique, cartes, trafic, événements, bots |
| `src/epsilon_app.h` | Protection de la calculatrice (touche Home) pour une application externe |
| `src/epsilon_files.h` | Lecture/écriture de fichiers dans le système de fichiers d'Epsilon (sauvegardes) |
| `icon.png` | Icône de l'application |
| `Makefile` | Compilation et installation |
| `info/COMPILER.md` | Guide pas à pas pour compiler et installer |
| `modding/MANUEL_MODS.md` | Manuel complet des mods Python |
| `modding/exemple_mods.py` | Exemple de script de mods |
| `info/CONTENU.md` | Inventaire de tout ce que contient le jeu |

## Compilation

Prérequis : `arm-none-eabi-gcc`, `arm-none-eabi-binutils` et Node.js (pour `nwlink`, téléchargé automatiquement par `npx`).

```sh
make          # compile output/numbeam3d.nwa
make check    # vérifie les liens et affiche la taille installée
make run      # installe sur la calculatrice branchée en USB
make clean    # supprime output/
```

Guide détaillé (Windows, macOS, Linux, dépannage) : [`COMPILER.md`](info/COMPILER.md).

Le Makefile attend les sources dans `src/` (`main.c`, `sim.h`, `epsilon_app.h`, `epsilon_files.h`) et `icon.png` à la racine, à côté du `Makefile`.

## Installation

1. Compile (`make`) ou récupère `numbeam3d.nwa`.
2. Branche la NumWorks en USB, puis `make run` (ou envoie le `.nwa` depuis [my.numworks.com](https://my.numworks.com)).
3. Lance l'application **NumBeam3D** depuis l'écran d'accueil.

Les records, statistiques, succès, favoris et réglages sont sauvegardés dans le système de fichiers de la calculatrice.

## Commandes par défaut

Toutes les touches sont reconfigurables (Options → Conduite → Commandes).

| Action | Touche |
|---|---|
| Accélérer / Freiner | Haut / Bas |
| Gauche / Droite | Gauche / Droite |
| Remettre / rejouer | OK |
| Capot / Portes / Coffre | Var / x,n,t / Exp |
| Tout ouvrir ou fermer | Shift |
| Poutres (filaire) | Boîte à outils |
| Vue du dessus / Dégâts | Ln / Log |
| Caméra gauche, droite, haut, bas | 4, 6, 8, 2 |
| Zoom + / − | 7 / 9 |
| Caméra reset | 5 |
| Menu rapide (favoris) | EXE |
| Turbo (maintenir) | Effacer |
| Vue habitacle | 0 |
| Rapport sup. / inf. | 3 / 1 |
| Boîte auto / manuelle | − |
| Embrayage / Frein à main (maintenir) | Alpha / Puissance |
| Mini-carte | Sin |
| Essence (maintenir) | Cos |
| Temps : stop / ralentir / accélérer | √ / ( / ) |
| Refaire la dernière option rapide | + |
| Favoris (accès direct) | Virgule |
| Tutoriel : étape suivante | Point |

## Mods Python

Écris un script dans l'application **Python** de la calculatrice, commence-le par `# numbeam`, et le jeu le lit au démarrage (les options peuvent aussi s'écrire en anglais : `name`, `color`, `gravity`…) :

```python
# numbeam
vehicule("Fusee", 0.9, 0.8, 1.3, 0.8, 2.0, 1.1, couleur=(255, 40, 120))
carte("Arene", sol=(90, 170, 90))
rampe(0, 8, 30, 45, 0, 6)
pouvoir("Lune", gravite=3, duree=15)
mode("Course lunaire", carte="Arene", vehicule="Fusee", gravite=6)
```

Limites : 8 véhicules, 4 cartes, 8 pouvoirs, 8 modes. Détails dans [`MANUEL_MODS.md`](modding/MANUEL_MODS.md).

## Notes techniques

- C11, `-Os`, calculs en simple précision, aucune allocation dynamique (ni `malloc`).
- Rendu 3D logiciel dans un tampon 16 bits (RGB565), triés par profondeur, avec météo (pluie, brouillard, verglas).
- La carte Italie est générée à la volée par cellules de 30 m (déterministe et mise en cache) : la route peut se poursuivre indéfiniment sans coût mémoire.
- Les cartes perso des mods utilisent le moteur de la carte Vide.

## Licence

À définir par l'auteur (aucune licence n'est fournie avec les sources).

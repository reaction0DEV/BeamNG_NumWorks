# NumBeam3D

NumBeam3D est un simulateur de voiture 3D pour calculatrice NumWorks, inspiré du style BeamNG mais adapté aux contraintes techniques du matériel et du moteur de rendu de la calculatrice.

Le projet produit une application autonome, avec une ville simplifiée, un trafic routier, une chasse au police, plusieurs paramètres de conduite et un rendu pseudo-3D sur écran monochrome / couleur de la NumWorks.

## Sommaire

- Aperçu du projet
- Fonctionnalités
- Prérequis
- Installation des dépendances
- Compilation locale
- Transformation en paquet .nwa
- Structure du dépôt
- Contrôles de jeu
- Dépannage
- Limites connues

## Aperçu du projet

Ce projet est conçu pour la plateforme NumWorks et repose sur :

- un moteur de rendu 3D simple basé sur la projection de points / faces
- une ville maillée avec rues, batiments, panneaux et objets décoratifs
- une simulation de déplacement et d’accélération simplifiée
- des options rapides de réglage du véhicule et du monde
- un packaging pour la calculatrice via l’outil EADK / nwlink

## Fonctionnalités

- voiture jouable avec accélération, freinage et direction
- ville générée avec quartiers et plans de rue variés
- projet urbain avec batiments, parkings, station-service, garages et zones de travaux
- trafic IA avec véhicules de circulation
- police poursuivante qui suit le joueur
- météo légère : clair, pluie, brouillard et verglas
- réglages rapides : puissance, adhérence, suspension et freinage
- garage et réparation du véhicule
- tutoriel et options de jeu
- rendu 3D et affichage de scène sur l’écran de la NumWorks

## Prérequis

Avant de compiler, il faut avoir :

- un environnement Linux ou macOS
- `make`
- `gcc` standard du système
- `arm-none-eabi-gcc`
- le paquet NumWorks EADK accessible via `npx --yes -- nwlink@1.0.0`

Vérification rapide :

```bash
arm-none-eabi-gcc --version
make --version
npx --yes -- nwlink@1.0.0 --help
```

## Installation des dépendances

Le projet utilise l’outil `nwlink` pour récupérer les flags et les outils de packaging NumWorks.

La commande du `Makefile` est déjà configurée :

```bash
npx --yes -- nwlink@1.0.0 eadk-cflags-device
```

Si cette commande fonctionne, le projet est prêt à compiler.

## Compilation locale

Depuis la racine du dépôt :

```bash
make build
```

Cette commande compile le fichier principal et produit les artefacts dans le dossier `output/`.

### Commandes utiles

Build simple :

```bash
make build
```

Build + génération du fichier binaire empaqueté :

```bash
make check
```

Vérification du package et liste du contenu généré :

```bash
ls -l output/
```

## Transformer le projet en .nwa

Le packaging officiel NumWorks est produit automatiquement par le `Makefile`.

### Étape 1 : compiler le projet

```bash
make build
```

### Étape 2 : générer le paquet final

```bash
make check
```

Cette commande exécute :

```bash
npx --yes -- nwlink@1.0.0 nwa-bin output/numbeam3d.nwa output/numbeam3d.bin
```

et produit au minimum :

- `output/numbeam3d.nwa`
- `output/numbeam3d.bin`

### À quoi servent ces fichiers ?

- `.bin` : binaire brut de l’application
- `.nwa` : paquet d’application NumWorks prêt à être transféré sur la calculatrice

### Transfert sur la calculatrice

Pour installer l’application sur une NumWorks, il faut utiliser le canal officiel de distribution NumWorks compatible avec les fichiers `.nwa` / `.bin` :

- soit via le logiciel officiel NumWorks / l’outils de transfert associé
- soit via un outil de transfert compatible avec les paquets approuvés par NumWorks

Le fichier le plus important pour le transfert est :

```bash
output/numbeam3d.nwa
```

> En pratique, le `.nwa` est le format de paquet que la calculatrice accepte, tandis que le `.bin` est un artefact intermédiaire utile pour le debug ou le packaging.

## Structure du dépôt

```text
.
├── Makefile
├── README.md
├── src/
│   ├── main.c
│   └── sim.h
├── output/
│   ├── numbeam3d.nwa
│   ├── numbeam3d.bin
│   └── ...
└── ...
```

Détail des fichiers :

- `src/main.c` : rendu, interface utilisateur, moteur de scène, menus, ville et objets
- `src/sim.h` : simulation physique, IA, ville, conditions météo, réglages de conduite
- `Makefile` : règles de compilation et de packaging NumWorks
- `output/` : artefacts générés (`.bin`, `.nwa`)

## Screenshots

Les captures ci-dessous sont des exemples de ce que l’on peut attendre visuellement du jeu une fois compilé et exécuté sur la NumWorks.

> Les images peuvent varier selon la résolution écran, la qualité de rendu choisie et la version du paquet final.

### Vue de ville

```text
+-----------------------------------------------+
|  NumBeam3D                                    |
|  Quartiers / routes / batiments               |
|  Plan de ville / route / environnement         |
|                                               |
|      .----.    .----.   .----.                 |
|     /____/    /____/   /____/                 |
|                                               |
|  route  :   route  :   route                  |
+-----------------------------------------------+
```

### Vue de conduite

```text
+-----------------------------------------------+
|  voiture / route / panneaux / trafic           |
|  courbe de route / police / environnement      |
|                                               |
|          .---------------------------.          |
|         /   voiture jouable         \         |
|        /  _______   _______         \        |
|       /__/_____/__/_____/            \       |
+-----------------------------------------------+
```

### Écran de menu rapide

```text
+-----------------------------------------------+
| OPTIONS RAPIDES                                |
| Reparer tout                                   |
| Turbo                                           |
| Voiture                                         |
| Moteur                                          |
| Meteo                                           |
| Plan de ville                                   |
| Puissance                                       |
| Adherence                                       |
| Suspension                                      |
| Freinage                                        |
| Poursuite police                                |
+-----------------------------------------------+
```

## Installation sur NumWorks

### 1. Générer le paquet

Depuis la racine du dépôt :

```bash
make check
```

Cela produit au moins :

```bash
output/numbeam3d.nwa
output/numbeam3d.bin
```

### 2. Préparer le transfert

Le fichier `.nwa` est le paquet à transférer vers la NumWorks.

Le transfert se fait via le mécanisme officiel NumWorks compatible avec les paquets applicatifs.

### 3. Installer sur la calculatrice

1. copier le fichier `.nwa` obtenu dans `output/`
2. utiliser l’outil officiel de transfert / de package NumWorks
3. envoyer le paquet sur la calculatrice
4. installer puis lancer l’application

### 4. Vérifier le lancement

Après installation, lancer l’application directement depuis le menu des applications de la NumWorks.

> En cas d’erreur, vérifier que le paquet a bien été généré dans `output/`, que la calculatrice est compatible et que les outils NumWorks sont correctement installés.

## Contrôles de jeu

Les touches peuvent être modifiées dans les menus de configuration du jeu.

Par défaut / usages courants :

- Accélérer : flèche haut
- Freiner / reculer : flèche bas
- Gauche : flèche gauche
- Droite : flèche droite
- Menu rapide : touche dédiée
- Vue / caméra : options de menu
- Réparations / garage : options du menu et du menu rapide

## Menu rapide et réglages

Le projet expose un menu rapide avec plusieurs options :

- réparation du véhicule
- turbo
- changement de voiture
- changement de moteur
- remorque / accessoires
- météo
- gravité
- temps
- trafic IA
- nombre d’IA
- comportement IA
- vitesse IA
- plan de ville
- réglages de puissance, adhérence, suspension et freinage
- poursuite policière

## Dépannage

### Erreur de compilation ARM

Vérifier que le compilateur est bien présent :

```bash
which arm-none-eabi-gcc
arm-none-eabi-gcc --version
```

### Erreur sur `nwlink`

Vérifier que Node.js et `npx` sont installés :

```bash
node --version
npm --version
npx --yes -- nwlink@1.0.0 --help
```

### Fichier .nwa non généré

Relancer :

```bash
make clean
make check
```

Si le Makefile ne contient pas de cible `clean`, il faut supprimer le dossier `output/` puis relancer.

## Limites connues

- le projet est conçu pour la NumWorks et dépend fortement de ses ressources mémoire et écran
- le rendu est optimisé pour un style simplifié, pas un vrai moteur 3D complet
- les modèles urbains et la physique sont volontairement simplifiés pour tenir sur la calculatrice
- le suivi du portage / le packaging officiel NumWorks dépend des outils autorisés par NumWorks et de la compatibilité de la machine cible

## Commandes de référence

```bash
make build
make check
ls -l output/
```

## Conclusion

Le projet est prêt à être compilé localement puis transformé en paquet `.nwa` pour la NumWorks. La commande clef est :

```bash
make check
```

et le paquet final se trouve dans :

```bash
output/numbeam3d.nwa
```



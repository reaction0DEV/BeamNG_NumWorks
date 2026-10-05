# NumBeam 3D

La version **3D** de NumBeam : un jeu de voiture à **carrosserie déformable** pour la calculatrice **NumWorks**, inspiré de BeamNG. Il tient dans une app `.nwa` d'environ 19 Ko.

> Inspiré de BeamNG.drive, sans aucun lien avec BeamNG GmbH ni avec NumWorks SAS.
> La version 2D (vue de côté) est dans le dossier principal du dépôt.

## Comment ça marche

La voiture n'est pas un modèle figé : c'est un **squelette de 20 points reliés par 86 poutres-ressorts**, simulé en 3D en temps réel.

- Les poutres se **déforment de façon permanente** quand elles sont trop sollicitées, puis **cassent** si le choc est violent.
- Les 4 roues ont des **suspensions** (ressort et amortisseur) et un **grip latéral** : la voiture roule, tourne et dérape.
- La carrosserie **frotte** contre le sol quand elle le touche, et les faces de la voiture se déforment avec le squelette.
- Le terrain est une **route** avec ligne centrale, **rampes**, **murs en béton** et **collines**. Hors de la route, le relief devient sauvage et tu peux rouler dans la campagne.
- Le jeu affiche la vitesse, le pourcentage de dégâts et la distance. La carrosserie fonce quand les dégâts augmentent.

## Commandes

| Touche | Action |
|---|---|
| Haut | Accélérer |
| Bas | Freiner, puis marche arrière |
| Gauche / Droite | Tourner |
| OK | Remettre la voiture en route, là où tu es |
| Boîte à outils | Afficher le squelette de poutres (blanc, puis rouge quand elles sont déformées) |
| Retour / Home | Quitter |

## Installation

Il te faut ta calculatrice, un câble USB et un ordinateur avec **Chrome** ou **Edge**.

1. Télécharge `NumBeam3D.nwa` (section **Releases** du dépôt).
2. Branche la calculatrice et allume-la.
3. Va sur [my.numworks.com/apps](https://my.numworks.com/apps) et connecte-toi.
4. Clique sur **Connect**, choisis ta calculatrice, ajoute `NumBeam3D.nwa`, puis clique sur **Install**. Laisse la calculatrice branchée jusqu'à la fin.
5. Sur la calculatrice, appuie sur **Home** et ouvre **NumBeam3D** (à la fin de la liste des apps).

> Les apps externes demandent un firmware officiel NumWorks récent. Sur Omega ou Upsilon, le fonctionnement peut différer.
> Certains jeux (Celeste, Hollow Knight) remplissent tout l'espace d'applications et ne peuvent pas être installés en même temps.

## Compiler soi-même

Prérequis : `arm-none-eabi-gcc` (avec newlib), **Node.js** (pour l'outil `nwlink`) et `make`.

```
git clone https://github.com/reaction0DEV/BeamNG_NumWorks
cd BeamNG_NumWorks/3d
make build      # produit output/numbeam3d.nwa
make check      # vérifie l'édition de liens et affiche la taille
```

Pour envoyer le fichier sur la calculatrice, utilise ensuite le site my.numworks.com/apps, comme dans la section Installation.

## Structure du projet

```
src/
  sim.h      physique 3D : points, poutres, déformation, collisions, terrain
  main.c     rendu 3D, caméra, entrées clavier, boucle de jeu
  icon.png   icône de l'app (55x56)
Makefile
```

- **Physique** (`sim.h`) : intégration d'Euler semi-implicite à pas fixe de 3,5 ms, ressorts-amortisseurs sur chaque poutre, déformation plastique au-delà d'un seuil, rupture au-delà d'un autre. Axes : x vers la droite, y vers le haut, z vers l'avant.
- **Terrain** : rendu par la technique du *voxel space* (une colonne de hauteur par colonne d'écran, parcourue de près en loin). La voiture est ensuite dessinée par-dessus, face par face, des plus lointaines aux plus proches.
- **Écran** : l'image est calculée en 160x112 puis agrandie 2 fois, pour rester dans la mémoire vive de la calculatrice. La barre d'information en bas est dessinée à part.

## Modifier le jeu

Les réglages se trouvent dans `src/sim.h` :

- **Gravité** : `G`
- **Rigidité et amortissement** : la ligne qui crée les poutres dans `car_init` (valeurs `k` et `c`)
- **Solidité** : les seuils de déformation (`.08f`) et de rupture (`.6f`) dans `step`
- **Moteur et vitesse max** : l'accélération (`10.f`) et la limite (`38`) dans la partie « tire » de `step`
- **Grip des pneus** : le coefficient `.25f` du glissement latéral
- **Terrain** : la fonction `gh` (route, rampes, murs, collines, répétés tous les 300 m)
- **Forme de la voiture** : le tableau `P` (les poutres sont créées automatiquement entre points proches)

Dans `src/main.c` : `FOC` (champ de vision), `HOR` (hauteur de l'horizon) et la distance de vue (72) dans `terrain`. Si le jeu est trop lent, réduis cette distance.

## Limites connues

- La fluidité dépend de la calculatrice : si le rendu est trop lent, la physique passe au ralenti au lieu de planter.
- La caméra ne s'incline pas : elle suit la voiture à plat, sans rouler ni piquer.
- Les réglages (dégâts, puissance des rampes) sont à affiner en jouant.

## Idées pour la suite

- Particules et étincelles lors des chocs
- Plusieurs voitures et plusieurs circuits
- Obstacles déformables (caisses, barrières)
- Meilleure distance sauvegardée

## Crédits

- Structure d'app basée sur [epsilon-sample-app-c](https://github.com/numworks/epsilon-sample-app-c) de NumWorks (licence BSD-3-Clause).
- Outil de liaison [`nwlink`](https://www.npmjs.com/package/nwlink).
- Inspiré de BeamNG.drive.

## Licence

À compléter : ajoute un fichier `LICENSE` (par exemple MIT ou GPL-3.0).

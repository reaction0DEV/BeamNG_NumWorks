# NumBeam

Un jeu de voiture à **carrosserie déformable** pour la calculatrice **NumWorks**, inspiré de BeamNG. Il tient dans une app `.nwa` d'environ 17 Ko.

> Inspiré de BeamNG.drive, sans aucun lien avec BeamNG GmbH ni avec NumWorks SAS.

## Comment ça marche

La voiture n'est pas une image : c'est un **squelette de 11 points reliés par 28 poutres-ressorts**, simulé en temps réel.

- Les poutres se **déforment de façon permanente** quand elles sont trop sollicitées, puis **cassent** si le choc est violent.
- Les roues sont montées sur des **suspensions** (ressort et amortisseur) qui absorbent les sauts.
- Les roues roulent librement, tandis que la carrosserie **frotte** contre le sol quand elle le touche.
- Le terrain est une route infinie avec collines, rampes et murs.
- Le jeu affiche la vitesse, le pourcentage de dégâts et la distance parcourue. La carrosserie fonce quand les dégâts augmentent.

C'est en **2D, vue de côté** 

## Commandes

| Touche | Action |
|---|---|
| Droite | Accélérer |
| Gauche | Freiner, puis marche arrière |
| Haut / Bas | Cabrer ou piquer la voiture en l'air |
| OK | Remettre la voiture sur ses roues, là où tu es |
| Boîte à outils | Afficher le squelette de poutres (blanc, puis rouge quand elles sont déformées) |
| Retour / Home | Quitter |

## Installation

Il te faut ta calculatrice, un câble USB et un ordinateur avec **Chrome** ou **Edge**.

1. Télécharge `NumBeam.nwa` (section **Releases** du dépôt).
2. Branche la calculatrice et allume-la.
3. Va sur [my.numworks.com/apps](https://my.numworks.com/apps) et connecte-toi.
4. Clique sur **Connect**, choisis ta calculatrice, ajoute `NumBeam.nwa`, puis clique sur **Install**. Laisse la calculatrice branchée jusqu'à la fin.
5. Sur la calculatrice, appuie sur **Home** et ouvre **NumBeam** (à la fin de la liste des apps).

> Les apps externes demandent un firmware officiel NumWorks récent. Sur Omega ou Upsilon, le fonctionnement peut différer.
> Certains jeux (Celeste, Hollow Knight) remplissent tout l'espace d'applications et ne peuvent pas être installés en même temps.

## Compiler soi-même

Prérequis : `arm-none-eabi-gcc` (avec newlib), **Node.js** (pour l'outil `nwlink`) et `make`.

```
git clone https://github.com/reaction0DEV/BeamNG_NumWorks
cd BeamNG_NumWorks
make build      # produit output/numbeam.nwa
make check      # vérifie l'édition de liens et affiche la taille
```

Pour envoyer le fichier sur la calculatrice, utilise ensuite le site my.numworks.com/apps, comme dans la section Installation.

## Structure du projet

```
src/
  sim.h      physique : points, poutres, déformation, collisions, terrain
  main.c     affichage, entrées clavier, boucle de jeu
  icon.png   icône de l'app (55x56)
Makefile
```

- **Physique** (`sim.h`) : intégration d'Euler semi-implicite à pas fixe de 3,5 ms, ressorts-amortisseurs sur chaque poutre, déformation plastique au-delà d'un seuil, rupture au-delà d'un autre.
- **Affichage** (`main.c`) : l'écran est dessiné par bandes de 16 lignes, pour ne pas dépasser la mémoire vive de la calculatrice.

## Modifier le jeu

Les réglages se trouvent dans `src/sim.h` :

- **Gravité** : `G`
- **Rigidité et amortissement** : la ligne qui crée les poutres dans `car_init` (valeurs `k` et `c`)
- **Solidité** : le seuil de déformation `yl`, et le seuil de rupture dans `step`
- **Puissance moteur** et **vitesse max** : début de `step`
- **Terrain** : la fonction `gy` (collines, rampes et murs, répétés tous les 2400 pixels)
- **Forme de la voiture** : le tableau `P` (les poutres sont créées automatiquement entre points proches)

## Idées pour la suite

- Particules et étincelles lors des chocs
- Plusieurs voitures et plusieurs niveaux
- Meilleur score de distance sauvegardé
- Obstacles déformables (caisses, barrières)

## Crédits

- Structure d'app basée sur [epsilon-sample-app-c](https://github.com/numworks/epsilon-sample-app-c) de NumWorks (licence BSD-3-Clause).
- Outil de liaison [`nwlink`](https://www.npmjs.com/package/nwlink).
- Inspiré de BeamNG.drive.

## Licence

À compléter : ajoute un fichier `LICENSE` (par exemple MIT ou GPL-3.0).

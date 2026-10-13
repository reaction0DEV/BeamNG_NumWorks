# NumBeam3D — Manuel des mods (scripts Python)

Tu peux ajouter **tes propres véhicules, cartes, pouvoirs et modes de jeu** en écrivant de petits scripts dans l'application **Python** de la NumWorks. Le jeu les lit au démarrage : pas besoin de recompiler ni de réinstaller.

> **Important :** le jeu ne *fait pas tourner* ton script (la NumWorks n'a pas de Python dans les apps externes). Il le **lit** comme une liste de déclarations. Tu écris donc du Python « de configuration » : des appels de fonctions, des variables et un peu de calcul. Pas de `for`, `if`, `def`, ni de listes complexes.

---

## 1. Installer un mod

1. Ouvre l'application **Python** de la calculatrice → **Ajouter un script** (ou envoie un fichier depuis un ordinateur avec [my.numworks.com/python](https://my.numworks.com/python)).
2. Donne-lui le nom que tu veux (ex. `mamap`). L'extension `.py` est automatique.
3. **La première ligne doit être `# numbeam`** (majuscules sans importance). Sans cette ligne, le fichier est ignoré (tes autres scripts Python ne sont jamais touchés).
4. Écris tes déclarations (voir plus bas), enregistre, puis lance le jeu.
5. Dans le jeu : **Défis → Modes perso** (ou **Options → Modes perso**). La première ligne indique ce qui a été chargé, par exemple `Scripts: 2  V3 C1 P4 M2` (2 fichiers, 3 véhicules, 1 carte, 4 pouvoirs, 2 modes).
6. Tu as modifié un script pendant que le jeu est ouvert ? Reviens dans **Modes perso** et choisis **Recharger les scripts**. (Quitter le jeu pour aller dans Python fait redémarrer le jeu de toute façon.)

Un exemple complet est fourni : `exemple_mods.py`.

**Si une erreur est détectée**, la première ligne de la page affiche `! fichier l.12: message` (nom du fichier, numéro de ligne). Appuie sur OK sur cette ligne pour la relire. Seule la *première* erreur est signalée : corrige-la, recharge, et la suivante apparaît.

---

## 2. Écrire un script

```python
# numbeam
largeur_route = 8          # une variable (nombre)
hauteur = 2 * 3            # calculs permis : + - * / ** et parenthèses

vehicule("Fusee", 0.9, 0.8, 1.3, 0.8, 2.0, 1.1, couleur=(255, 40, 120))
carte("Arene", sol=(90, 170, 90))
rampe(0, largeur_route, 30, 45, 0, hauteur)
```

Règles :

- Une instruction = `fonction(arguments)` ou `nom = nombre`. Un appel peut tenir sur plusieurs lignes tant que la parenthèse n'est pas fermée.
- Arguments **dans l'ordre** (positionnels) ou **par nom** : `rampe(0, 8, 30, 45, 0, 6)` et `rampe(x=0, largeur=8, z0=30, z1=45, h0=0, h1=6)` sont équivalents. On peut mélanger (positionnels d'abord).
- Texte entre `"guillemets"` ou `'apostrophes'`. **Pas d'accents** dans les noms de fonctions et d'options. Dans les textes affichés évite-les aussi (la police du jeu ne les gère pas).
- Couleurs et positions sont des tuples : `(255, 0, 0)`, `(x, z)`.
- `True`, `False`, `None` et `pi` existent. `#` commence un commentaire. Les lignes `import ...` / `from ... import ...` sont ignorées.
- Les options peuvent aussi s'écrire **en anglais** (voir le tableau ci-dessous) ; on peut mélanger les deux langues. Les noms de fonctions et d'options ne tiennent pas compte des majuscules. Les **noms d'options inconnus sont signalés** (faute de frappe).
- Les valeurs hors limites sont **ramenées à la limite** sans erreur.

### Noms d'options en anglais

| Français | Anglais | Français | Anglais |
|---|---|---|---|
| `nom` | `name` | `largeur` | `width` |
| `hauteur` | `height` | `longueur` | `length` |
| `masse` | `mass` | `puissance` | `power` |
| `couleur` | `color` / `colour` | `propulseur` | `thruster` |
| `sol` | `ground` | `depart` | `start` |
| `collines` | `hills` | `gravite` | `gravity` |
| `adherence` | `traction` | `rayon` | `radius` |
| `saut` | `jump` | `reparer` | `repair` |
| `essence` | `fuel` | `duree` | `duration` |
| `carte` | `map` | `vehicule` | `vehicle` |
| `moteur` | `engine` | `roues` | `wheels` |
| `remorque` | `trailer` | `meteo` | `weather` |
| `trafic` | `traffic` | `vitesse_max` | `max_speed` |
| `adherence_laterale` | `lateral_grip` | `turbo_vitesse` | `turbo_speed` |
| `freinage` | `braking` | `braquage` | `steering` |
| `frein_moteur` | `engine_braking` | | |

Les fonctions `vehicle`, `map`, `ramp`, `hills`, `loop`, `object`, `start` et `power` existaient déjà. Les noms de moteurs, roues, objets, etc. restent ceux affichés par le jeu (version française : `"Tout-terrain"`, `"baril"`...).

### Limites

| Élément | Maximum |
|---|---|
| Véhicules perso | 8 |
| Cartes perso | 4 |
| Rampes par carte | 16 |
| Boucles (loopings) par carte | 4 |
| Objets par carte | 12 |
| Pouvoirs | 8 |
| Modes | 8 |
| Variables | 16 |
| Nom affiché | 15 caractères |

---

## 3. Les fonctions

### `vehicule(nom, largeur, hauteur, longueur, masse, puissance, grip, couleur=(r,g,b), propulseur=0)`

Ajoute un véhicule au **Garage** (ligne *Voiture*) et à l'option rapide *Voiture*. Tout est un **coefficient** par rapport à la Berline (1 = identique).

| Option | Défaut | Plage | Effet |
|---|---|---|---|
| `nom` | `"Mod"` | 15 car. | nom affiché |
| `largeur`, `hauteur`, `longueur` | 1 | 0.3 – 3 | dimensions de la carrosserie |
| `masse` | 1 | 0.3 – 4 | plus grand = plus lourd |
| `puissance` | 1 | 0.3 – 3 | multiplie l'accélération du moteur |
| `grip` | 1 | 0.3 – 2 | adhérence des pneus |
| `couleur` | gris | 0 – 255 | couleur de la carrosserie |
| `propulseur` | 0 | 0 ou 1 | 1 = propulseurs en turbo (comme le Dragster) |

Le moteur, les roues, la suspension et le châssis se choisissent comme d'habitude dans le Garage (ou via un `mode`).

### `carte(nom, sol=(r,g,b), depart=(x,z), collines=(hauteur,longueur), gravite=, adherence=)`

Crée une carte. Elle se choisit comme les autres (ligne *Carte* du menu principal ou option rapide *Carte*) et se construit sur le terrain plat de la carte **Vide**. Les fonctions `rampe`, `colline`, `boucle`, `objet`, `depart` qui suivent s'appliquent à **cette carte** (jusqu'à la prochaine `carte`).

| Option | Défaut | Effet |
|---|---|---|
| `sol` | damier blanc | couleur du sol (damier de deux teintes proches) |
| `depart` | `(0, 0)` | où apparaît la voiture (x = côté, z = avant) |
| `collines` | aucune | relief ondulé : hauteur max (0 – 8 m) et longueur d'onde (3 – 200 m) |
| `gravite` | celle des options | pesanteur de la carte (1 – 80) ; 0 ou absent = réglage normal |
| `adherence` | celle des options | adhérence de la route (0 – 3) |

La voiture regarde vers les **z croissants**. Les cartes perso gardent les décors de la carte Vide (rien de plus), sans ville ni circuit.

### `rampe(x, largeur, z0, z1, h0, h1)`

Une rampe (ou un tremplin, un mur incliné, un pont…) : centrée sur `x`, large de `largeur` mètres, qui s'étend de `z0` à `z1` en avant, et dont la hauteur passe linéairement de `h0` à `h1` (0 – 40 m). Deux rampes bout à bout font un saut ; `h0 > h1` fait une descente. Les rampes sont orange et noires.

### `colline(amplitude, longueur)`

Variante de l'option `collines` de `carte`, écrite en ligne séparée.

### `boucle(x, z, rayon)`

Un looping vertical (rayon de 3 à 15 m) à la position donnée. Il fonctionne comme ceux de la carte Vide (option *Rampes* active dans les options). Sans `boucle`, ta carte n'en a pas.

### `objet(type, x, z)`

Pose un objet physique au départ : `"cone"`, `"caisse bois"`, `"caisse acier"`, `"plot beton"`, `"baril"` (explosif), `"pneus"`, `"pylone"` (ou le numéro 0 – 6). On peut écrire seulement le début du nom (`"caisse"` donne la caisse en bois). Option *Objets* active requise dans les options.

### `depart(x, z)`

Même effet que l'option `depart` de `carte`.

### `pouvoir(nom, ...)`

Un **pouvoir** s'active **en jeu** : ouvre le menu (touche des options rapides / pause) → **Options → Modes perso**, choisis le pouvoir et appuie sur OK. Tu peux aussi l'ajouter à tes **favoris** avec Shift pour l'avoir dans le menu rapide.

Effets instantanés (combinables) :

| Option | Plage | Effet |
|---|---|---|
| `saut=` | 0 – 40 | impulsion vers le haut |
| `boost=` | 0 – 60 | impulsion vers l'avant |
| `stop=1` | 0 / 1 | arrête la voiture |
| `reparer=1` | 0 / 1 | répare la voiture |
| `essence=1` | 0 / 1 | plein de carburant |
| `tp=(x, z)` | | téléporte la voiture à cet endroit |

Effets **temporaires** : `duree=` (secondes, 0 – 120) pendant laquelle s'appliquent :

- `turbo=1` : turbo forcé,
- n'importe quel **réglage** de la liste du chapitre 4 (par ex. `gravite=3`, `vitesse_max=250`).

Si tu donnes un réglage ou `turbo=1` sans `duree`, la durée est de **10 s**. Un pouvoir temporaire déjà actif est remplacé par le nouveau.

### `mode(nom, ...)`

Un **mode** regroupe une carte, un véhicule, un équipement et des réglages, et **lance directement la partie** (menu **Défis → Modes perso**, ou favori). Tout est optionnel :

| Option | Valeur | Effet |
|---|---|---|
| `carte=` | nom (`"Arene"`, `"Ville"`, `"Circuit"`…) ou numéro | carte à utiliser |
| `vehicule=` | nom (`"Fusee"`, `"Camion"`…) ou numéro | véhicule |
| `moteur=` | `"4 cyl"`, `"V6"`, `"V8"`, `"Turbo"` | moteur |
| `roues=` | `"Route"`, `"Sport"`, `"Tout-terrain"`, `"Mini"` | roues |
| `suspension=` | `"Souple"`, `"Normale"`, `"Ferme"`, `"Course"` | suspension |
| `chassis=` | `"Acier"`, `"Renforce"`, `"Leger"` | châssis |
| `remorque=` | `"non"`, `"Simple"`, `"Citerne"`, `"Plateau"`, `"Caravane"` | remorque |
| `meteo=` | 0 – 3 | météo (comme l'option du jeu) |
| `trafic=` | 0 – 64 | nombre de voitures de trafic (0 = aucun) |
| + réglages | voir chapitre 4 | imposés pendant tout le mode |

Les noms n'ont pas besoin d'être complets (`"tout"` suffit pour `"Tout-terrain"`), et les majuscules ne comptent pas. La carte et le véhicule peuvent être définis dans le même script **ou dans un autre**.

Le mode reste actif jusqu'au retour au **menu principal**. Les réglages du mode **sont aussi remis à zéro** à ce moment-là.

---

## 4. Les réglages (vitesse max, gravité, etc.)

Ce sont les mêmes variables que la page **Options → Système → Variables dev**. On les utilise dans `pouvoir(...)` et `mode(...)` :

| Option | Unité | Plage | Rôle |
|---|---|---|---|
| `vitesse_max` | km/h | 3 – 150 | vitesse maximale du moteur |
| `acceleration` | – | 1 – 100 | puissance d'accélération |
| `adherence_laterale` | – | 0.02 – 2 | tenue de route dans les virages |
| `gravite` | – | 0 – 80 | pesanteur (14 = normale) |
| `adherence` | – | 0.05 – 3 | adhérence de la route |
| `turbo_vitesse` | × | 1 – 5 | multiplicateur de vitesse du turbo |
| `turbo_acceleration` | × | 1 – 10 | multiplicateur d'accélération du turbo |
| `freinage` | × | 0 – 10 | force des freins |
| `braquage` | – | 0.05 – 1 | angle de direction de base |
| `frein_moteur` | – | 0 – 0.2 | frein moteur au lâcher de l'accélérateur |

**Priorité** si plusieurs sources agissent en même temps : réglages d'un **mode** < **mode développeur** (tes réglages manuels) < **pouvoir** actif.

---

## 5. Exemples rapides

**Un véhicule rapide et léger**

```python
# numbeam
vehicule("Eclair", 0.9, 0.8, 1.2, 0.7, 2.5, 1.2, couleur=(255, 220, 0))
```

**Une carte de sauts**

```python
# numbeam
carte("Sauts", sol=(200, 170, 110), collines=(1, 50))
h = 5
rampe(0, 8, 30, 45, 0, h)       # montée
rampe(0, 8, 45, 46, h, h)       # plateau
rampe(0, 8, 46, 60, h, 0)       # descente
boucle(0, 120, 8)
```

**Un pouvoir « gravité lunaire » et un mode qui l'utilise**

```python
# numbeam
pouvoir("Lune", gravite=3, duree=15)
mode("Course lunaire", carte="Sauts", vehicule="Eclair", gravite=6, vitesse_max=200)
```

---

## 6. Dépannage

| Symptôme | Cause probable |
|---|---|
| Le script n'est pas pris en compte | la première ligne n'est pas `# numbeam` |
| `option xxx` | nom d'option mal écrit (voir les tableaux ci-dessus) |
| `nom inconnu` | variable utilisée avant d'être définie, ou fonction mal écrite |
| `guillemet manque` | un texte n'est pas fermé sur sa ligne |
| `virgule attendue` | virgule oubliée entre deux arguments, ou parenthèse non fermée |
| `rampe sans carte` | il faut d'abord un `carte(...)` avant `rampe`, `boucle`, `objet`, `depart` |
| `max 8 vehicules` (etc.) | limite atteinte (voir chapitre 2) |
| Un véhicule / une carte n'apparaît pas | `Recharger les scripts`, puis vérifie la ligne `Scripts: …` |
| Le jeu manque de mémoire | réduis le nombre de rampes / objets, ou mets moins de mods |

## 7. Ce qui n'est pas possible (pour l'instant)

- Boucles `for`, conditions `if`, fonctions `def` et listes : le script est une liste de déclarations, pas un programme.
- Nouvelles formes 3D de carrosserie, textures, sons, nouveaux moteurs/roues/châssis (on choisit parmi ceux du jeu).
- Villes, circuits ou routes sur mesure : les cartes perso reposent sur le terrain plat de la carte Vide, avec rampes, collines, boucles et objets.
- Effets de pouvoir « à la volée » autres que ceux listés (saut, boost, stop, réparation, essence, téléportation, turbo et réglages temporaires).

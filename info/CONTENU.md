# NumBeam3D — tout ce que contient le programme

Inventaire établi à partir du code source (`main.c`, `sim.h`). Les noms sont ceux affichés dans le jeu.

## 1. Cartes (6 intégrées + cartes perso)

| Carte | Description |
|---|---|
| **Circuit** | Piste avec ligne de départ/arrivée à damier, chronomètre : temps de tour et meilleur tour |
| **Route** | Route droite avec marquage central et bords, dos d'âne |
| **Crash-test** | Voiture lancée contre un obstacle à vitesse choisie (30 / 50 / 80 / 110 / 140 km/h), score d'impact |
| **Ville** | Ville en blocs (plan régulier, décalé ou canalisé), immeubles, feux tricolores, ralentisseurs, stations-service, trafic, police, événements aléatoires, livraisons |
| **Vide** | Terrain plat de test : rampes, tremplins et loopings verticaux (option *Rampes*), objets physiques |
| **Italie** | Route de campagne sinueuse sans fin, collines toscanes, villages tous les 360 m (maisons, église avec campanile, fontaine), cyprès, pins parasols, fermes, villas à tour, ruines romaines, oliveraies, vignes, meules, chapelles, murets, montagnes au loin ; cônes, caisses, barils et mur de pneus sur la route au départ |
| **Cartes perso** | Jusqu'à 4 cartes créées par script Python (sur le terrain de la carte Vide) |

## 2. Véhicules (16)

Berline, Sport, Pick-up, Buggy, **Gen #** (voiture générée aléatoirement à partir d'une graine), Citadine, 4x4, SUV, Muscle, Van, Camion, Kart, Rallye, Limousine, Dragster (propulseurs en turbo), Taxi.
Plus jusqu'à 8 véhicules perso via mods.

### Équipement
- **Moteurs (4)** : 4 cyl, V6, V8, Turbo
- **Roues (4)** : Route, Sport, Tout-terrain, Mini
- **Suspensions (4)** : Souple, Normale, Ferme, Course
- **Châssis (3)** : Acier, Renforcé, Léger
- **Remorques (5)** : aucune, Simple, Citerne (liquide qui clapote), Plateau (caisses libres), Caravane
- **Solidité du sol/obstacles** : Robuste, Normale, Fragile

## 3. Physique et dégâts

- Carrosserie en **nœuds et poutres** déformables (déformation plastique, rupture)
- Vitres qui se fissurent puis éclatent, rétroviseurs qui pendent puis s'arrachent, pare-chocs qui se détachent
- Capot, portes et coffre qui s'ouvrent (dans le garage et en jeu)
- **Systèmes** : radiateur (fuite, vapeur), réservoir (fuite, incendie, explosion), arbre de transmission, boîte, différentiel, essieux, échappement
- **Pneus** : crevaison après un atterrissage violent, roue qui s'affaisse, perte d'adhérence
- Feu, fumée, boule de feu d'explosion, étincelles, poussière, débris
- Traces de pneus (freinage, dérapage, burn-out)
- **Airbags** (avant, latéraux) et **ceinture**, risque pour les occupants
- Boîte de vitesses **auto ou manuelle**, embrayage, frein à main, frein moteur, turbo
- Carburant et consommation, plein aux stations-service (10 s de vide à plein)
- Adhérence de la route (Sèche / Mouillée / Glace) et gravité (Normale / Lune / Forte)
- Météo : Clair, Pluie, Brouillard, Glace
- Dérapage, marche arrière, saut, boost

## 4. Monde et IA

- **Trafic IA** : voitures de circulation sur rails, qui deviennent physiques (bots) près du joueur ; nombre, comportement (Circuit / Suit joueur / Patrouille) et vitesse (de très lent à très rapide)
- **Poursuite de police** (option)
- **Feux tricolores** (vert 6 s, orange 2 s, rouge 6 s, décalage par carrefour)
- **Événements aléatoires** en ville : voitures accidentées, ambulance et police, barrières, chantiers, camion de travaux, convoi sur les routes de ceinture
- **Objets physiques** : Cone, Caisse bois, Caisse acier, Plot béton, Baril (explosif), Pneus, Pylône
- **Rampes et loopings** sur la carte Vide

## 5. Modes de jeu et défis

- **Jouer** libre sur toutes les cartes
- **Livraisons** chronométrées en ville (plusieurs colis, un compte à rebours par colis), 3 niveaux : Facile, Normal, Expert, avec étoiles et meilleurs scores
- **Crash-test** à vitesse réglable
- **Tour de circuit** avec temps de tour
- **Tutoriel** guidé en 13 étapes (accélérer, tourner, freiner, caméra, capot/portes/coffre, dégâts, réparation, turbo)
- **Modes perso** (scripts Python)
- **Contrôle du temps** : STOP, x0.1, x0.25, x0.5, x1, x2

## 6. Succès (29) et statistiques

Premier crash, Petite balade, Livreur, Marche arrière express, Drifteur, Premier tour, Pompiste, Boum !, Pied au plancher, Routard, Casseur, Facteur fiable, Roi du drift, Pneus fumants, Casse-cou, Chauffeur d'élite, Marathon, Globe-trotter, Vitesse lumière, Marche arrière extrême, Démolisseur, Maître du drift, Drift sans fin, Livreur légendaire, Expert parfait, Looping, Conduite zen, Collectionneur, Platine.

**Statistiques à vie** : distance, crashs, vitesse max, temps de dérapage (total et meilleur), livraisons, marche arrière max, pleins, explosions, loopings, tours, véhicules conduits. Sauvegardées avec les records (`numbeam.sav`).

## 7. Vues et affichage

- Caméra libre (gauche/droite/haut/bas, zoom, reset), vue **habitacle**, vue **du dessus** avec dégâts
- Affichage **filaire des poutres**
- **Mini-carte** de la ville (coin ou plein écran, petites/normales/grandes icônes) : voiture, destination, police, événements
- HUD : vitesse, rapport, régime, carburant, indicateurs de systèmes
- Qualité (Basse / Normale / Haute), champ de vision (Étroit / Normal / Large), distance de vue (Courte / Normale / Longue), traces de pneus et particules réglables
- Garage 3D avec voiture à inspecter

## 8. Menus

- **Menu principal** : Jouer, Carte, Garage, Défis, Profil, Options, Quitter
- **Options** : Mode, Véhicule, Monde, Conduite, Affichage, Outils (en jeu), Système, Modes perso, Favoris
- **Mode Simple / Avancé** (le mode Avancé affiche des réglages supplémentaires)
- **Menu rapide** en jeu avec 37 actions : Réparer tout, Turbo, Boost, Voiture, Moteur, Remorque, Route, Gravité, Temps, Retour au départ, Trafic IA, Nombre IA, Comportement IA, Vitesse IA, Plan de ville, Météo, Puissance, Adhérence, Suspension, Freinage, Poursuite police, Test (percer > feu > boom), Saut, Stop (frein d'urgence), Roues, Carte, Boîte de vitesses, Crevaison, Embrayage, Ceinture, Airbags, Événements, Mini-carte, Icônes carte, Carrosserie cassable, Consommation, Plein d'essence
- **Favoris** : 8 emplacements personnalisables (Shift pour ajouter une ligne)
- **Commandes reconfigurables** : 35 actions liées aux touches de la calculatrice
- Pages **Défis**, **Profil** (Statistiques, Succès)

## 9. Mode développeur (variables dev)

Vitesse max, Accélération, Adhérence latérale, Gravité, Adhérence globale, Turbo vitesse, Turbo accélération, Freinage, Braquage, Frein moteur. Enregistrés dans un fichier de config séparé des records.

## 10. Mods par scripts Python

Fonctions : `vehicule`, `carte`, `rampe`, `colline`, `boucle`, `objet`, `depart`, `pouvoir`, `mode`, plus variables et calculs simples.
- **Pouvoirs** : saut, boost, stop, réparation, essence, téléportation, turbo temporaire, réglages temporaires
- **Modes** : carte, véhicule, moteur, roues, suspension, châssis, remorque, météo, trafic et réglages dev imposés
- Limites : 8 véhicules, 4 cartes (16 rampes, 4 loopings, 12 objets chacune), 8 pouvoirs, 8 modes, 16 variables
- Rechargement à chaud, erreurs signalées avec nom de fichier et numéro de ligne
- Détails : `MANUEL_MODS.md`, exemple : `exemple_mods.py`

## 11. Sauvegarde et système

- Records, statistiques et succès : `numbeam.sav` (avec somme de contrôle)
- Favoris, mode Simple/Avancé, mode développeur : fichier de config séparé
- Protection de la touche **Home** (l'application la reçoit comme une touche et quitte proprement)
- Compatible Upsilon ; fonctionne aussi dans le lanceur NumPlay

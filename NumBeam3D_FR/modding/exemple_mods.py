# numbeam
# Exemple de mods NumBeam3D: vehicule, carte, pouvoirs et modes.
# Ce fichier est lu par le jeu (rien n'est execute). Le nom du fichier est libre.

# ---------- Vehicules ----------
# vehicule(nom, largeur, hauteur, longueur, masse, puissance, grip, couleur=(r,g,b))
vehicule("Fusee", 0.9, 0.8, 1.3, 0.8, 2.0, 1.1, couleur=(255, 40, 120))
vehicule("Tank", 1.4, 1.5, 1.6, 3.0, 1.5, 0.7, couleur=(70, 90, 50))
vehicule("Jet", 0.85, 0.75, 1.35, 1.0, 2.5, 0.8, couleur=(250, 250, 0), propulseur=1)

# ---------- Carte ----------
# carte(nom, sol=(r,g,b), depart=(x,z), collines=(hauteur,longueur), gravite=, adherence=)
carte("Arene", sol=(90, 170, 90), depart=(0, 0), collines=(1.5, 40), gravite=14)
# rampe(x, largeur, z_debut, z_fin, hauteur_debut, hauteur_fin)
saut = 6
rampe(0, 8, 30, 45, 0, saut)
rampe(0, 8, 70, 90, saut, 0)
rampe(-20, 6, 40, 60, 0, 2 * saut / 3)
boucle(12, 120, 7)
objet("baril", 5, 25)
objet("cone", -3, 20)
objet("pneus", 10, 60)

# ---------- Pouvoirs ----------
pouvoir("Super saut", saut=14)
pouvoir("Fusee!", boost=35, duree=4, turbo=1, turbo_acceleration=4)
pouvoir("Lune", gravite=3, duree=8)
pouvoir("Retour", tp=(0, 0), stop=1, reparer=1, essence=1)

# ---------- Modes (menu: Defis > Modes perso) ----------
mode("Lune", carte="Arene", vehicule="Fusee", gravite=3, vitesse_max=200)
mode("Tank 4x4", carte="Arene", vehicule="Tank", moteur="V8", roues="Tout-terrain", trafic=0, meteo=1)

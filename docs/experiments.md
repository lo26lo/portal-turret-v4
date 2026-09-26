# Cahier d'expériences — mesurer la carte Turret2

Créé le 26.09.2026. Des expériences pour **comprendre et vérifier** la carte et le firmware en mesurant, **sans aucune modification matérielle** (carte et câblage figés). Chaque fiche donne le montage, les réglages de départ, ce qu'on devrait voir, ce qu'on apprend, et une zone « Mesures » à remplir.

> Les valeurs « attendues » viennent du plan matériel ([design-plan.md](design-plan.md)) et du firmware ([../Turret_firmware/README.md](../Turret_firmware/README.md)). Rien n'a encore été mesuré : **c'est justement le but.** Un écart n'est pas forcément une erreur, c'est une information : le noter.

## Matériel

- **Oscilloscope** (2 voies minimum ; décodage série / I²C si disponible).
- **Power Profiler Kit II (PPK2)** — uniquement en **mode banc** (SW1 fermé au démarrage : aucun servo), car il est limité à ~1 A.
- **Testeur USB-C en ligne** avec enregistrement (optionnel), entre le chargeur et la tourelle.
- Chargeur **5 V / 3 A**.

## Règles de sécurité — à relire avant chaque séance

1. **Sortie haut-parleur (J15) : jamais la pince de masse de l'oscilloscope sur OUT+ ni OUT−.** La sortie est en pont (BTL) : la masse de l'oscilloscope la court-circuiterait. Deux sondes, masses sur GND, fonction **A − B**.
2. **Masses** : l'oscilloscope est en général relié à la terre par sa prise secteur. Si la tourelle est en plus reliée à un PC par l'USB, les masses se rejoignent par deux chemins : pour les mesures de précision, alimenter la tourelle par **un chargeur seul**.
3. **Jamais les deux USB** (USB-C de la carte et USB déporté J1) sur deux sources en même temps.
4. **PPK2 : mode banc uniquement** (pas de servo), il n'accepte pas les pointes de 3 à 4 A. Vérifier sur sa fiche la tension maximale en mode ampèremètre avant de l'intercaler sur un chargeur (un chargeur USB peut sortir jusqu'à 5,25 V).
5. **Ne jamais provoquer un défaut de l'eFuse par un court-circuit.** Les défauts se testent en lisant ce que le firmware enregistre (PWR_FLT, compteurs).
6. Sondes en **×10** par défaut ; pointe fine et ressort de masse court pour les signaux rapides (NeoPixel, I²C).

## Points de mesure disponibles (sans modification)

| Où | Quoi | Remarque |
|---|---|---|
| TP « VBUS » | entrée USB, avant l'eFuse | |
| TP « 5V » (×2) | +5 V après l'eFuse | |
| TP « 3V3 » | sortie du buck-boost | |
| J16 (UART0, JST-SH 1 mm, dos) | TXD0 (IO43), RXD0 (IO44), GND, **EN**, IO0 | vérifier l'ordre des broches sur le schéma avant de sonder |
| J7–J10 (servos) | 1 GND · 2 +5 V · **3 PWM** | |
| J3 / J4 (canons) | 1 +5 V · **2 PWM servo** · **3 données LED** · 4 GND | |
| J14 (anneau) | 1 +5 V · **2 données LED** (5 V, après l'AHCT125) · 3 GND | |
| J5 (radar) | 1 +5 V · **2 IO17 ← TX radar** · **3 IO18 → RX radar** · 4 GND | |
| J11 (Qwiic) | 1 GND · 2 +3,3 V · **3 SDA** · **4 SCL** | |
| J17 / J18 (Hall) | 1 +3,3 V · **2 signal** · 3 GND | |
| J15 (haut-parleur) | OUT+ / OUT− | **règle n°1** |
| LED verte (IO33) / rouge (IO48) | repères du firmware | voir « Aides firmware » |

---

## Partie 1 — Alimentation

### X1. Soft-start de l'eFuse

- **Montage** : voie 1 sur TP VBUS, voie 2 sur TP 5V, masses sur GND. Tourelle débranchée.
- **Réglages** : 1 V/div, 2 ms/div, déclenchement **unique** sur front montant de la voie 2 vers 2,5 V.
- **Action** : brancher le chargeur.
- **Attendu** : VBUS monte presque d'un coup ; le 5 V suit en **rampe d'environ 6 ms** (condensateur dVdt de 47 nF ≈ 0,9 V/ms). Un petit plateau est possible au début (l'eFuse démarre brièvement en limitation de courant avec ~1,2 mF de condensateurs en aval).
- **On apprend** : comment l'eFuse limite l'appel de courant au branchement (≈ 1,1 A au lieu de plusieurs ampères).
- **Mesures** : durée de la rampe = ____ ms ; plateau oui / non ; remarques : ____

### X2. Délai de démarrage de l'ESP32 (EN)

- **Montage** : voie 1 sur TP 3V3, voie 2 sur la broche EN de J16.
- **Réglages** : 1 V/div, 5 ms/div, déclenchement unique sur la voie 1.
- **Attendu** : EN monte **après** le 3V3, avec une courbe de charge de condensateur (circuit RC 10 kΩ / 1 µF, constante de temps 10 ms) ; l'ESP32 démarre quand EN dépasse ~0,75 × 3,3 V.
- **Bonus** : appuyer sur RESET (SW3) et regarder EN tomber puis remonter.
- **On apprend** : pourquoi un microcontrôleur attend que son alimentation soit stable.
- **Mesures** : délai 3V3 → EN au seuil = ____ ms

### X3. Creux du 5 V quand les servos démarrent

- **Montage** : voie 1 sur TP 5V, voie 2 sur TP 3V3, **couplage AC** sur les deux (pour zoomer sur les variations).
- **Réglages** : 200 mV/div, 20 ms/div, déclenchement sur **front descendant** de la voie 1 (−150 mV environ).
- **Action** : commande `demo` (ou bouton A).
- **Attendu** : le 5 V plonge à chaque démarrage de servo ; **le 3V3 ne bouge presque pas** (le buck-boost TPS631000 continue de réguler quand son entrée baisse).
- **On apprend** : pourquoi l'ESP32 est alimenté par un buck-boost ; ce qu'est la chute de tension dans les câbles et l'eFuse.
- **Mesures** : creux max du 5 V = ____ mV ; variation du 3V3 = ____ mV ; tension minimale absolue du 5 V (couplage DC) = ____ V (objectif > 4,5 V)

### X4. Démarrage échelonné des servos

- **Montage** : comme X3, voie 1 seule, 50 ms/div ou 100 ms/div.
- **Action** : `set ServoStagger 50`, `reboot`, capturer ; puis `set ServoStagger 250`, `reboot`, capturer. (Ou la commande « cycle de charge » L4 quand elle existera.)
- **Attendu** : à 50 ms, les creux se rapprochent ou se cumulent ; à 250 ms, ils sont séparés et le creux maximal est plus faible.
- **On apprend** : pourquoi le firmware attache les servos un par un (limite de l'eFuse à 3,86 A).
- **Mesures** : creux max à 50 ms = ____ mV ; à 250 ms = ____ mV ; valeur retenue pour `ServoStagger` = ____

### X5. Courant d'entrée sur un cycle complet (testeur USB-C)

- **Montage** : testeur USB-C entre le chargeur et la tourelle, enregistrement actif.
- **Action** : boot complet, puis 3 `demo`.
- **Attendu** : pointes au démarrage des servos, plateau pendant le tir (son + LEDs), repos bas ailes fermées (servos détachés).
- **Mesures** : repos = ____ mA ; pointe max = ____ A ; moyenne pendant un cycle = ____ mA

---

## Partie 2 — Signaux numériques

### X6. Signal de commande d'un servo

- **Montage** : voie 1 sur J7 broche 3 (PWM aile gauche) ou J10 broche 3 (rotation X).
- **Réglages** : 1 V/div, 5 ms/div, puis 500 µs/div pour mesurer la largeur.
- **Action** : `servo rotx 0`, `servo rotx 90`, `servo rotx 180`, `servo rotx off`.
- **Attendu** : période 20 ms (50 Hz), impulsions de ~0,5 ms (0°) à ~2,4 ms (180°) ; `off` → plus aucune impulsion (servo détaché, politique D6).
- **On apprend** : comment un servo est commandé par la **largeur** d'impulsion, et ce que veut dire « détacher ».
- **Mesures** : largeur à 0° = ____ µs, 90° = ____ µs, 180° = ____ µs

### X7. Données des NeoPixels

- **Montage** : voie 1 sur J14 broche 2, masse sur J14 broche 3.
- **Réglages** : 1 V/div, 1 µs/div ; déclenchement sur front montant.
- **Action** : `led ring 000000` puis `led ring 800000` (un seul bit à 1 par octet de rouge), ou le motif de test L3.
- **Attendu** : ~800 kHz (bit de 1,25 µs) ; un « 0 » = impulsion haute courte (~0,4 µs), un « 1 » = impulsion haute longue (~0,8 µs) ; niveau haut ≈ **5 V** (sortie de l'AHCT125, alimenté en 5 V).
- **On apprend** : le protocole WS2812 à une seule ligne, et pourquoi un décaleur de niveau est nécessaire (les LEDs en 5 V veulent un « 1 » ≥ 3,5 V, l'ESP32 ne sort que 3,3 V).
- **Mesures** : largeur « 0 » = ____ ns, « 1 » = ____ ns, niveau haut = ____ V

### X8. UART du radar

- **Montage** : voie 1 sur J5 broche 2 (sortie du radar vers IO17).
- **Réglages** : 1 V/div, 20 µs/div ; décodage UART **256 000 bauds, 8N1** si l'oscilloscope le permet.
- **Attendu** : une trame toutes les ~100 ms environ, qui commence par `AA FF 03 00` et finit par `55 CC` ; niveau 3,3 V.
- **On apprend** : une liaison série, le débit, le format de trame du LD2450 (3 cibles × 8 octets).
- **Mesures** : durée d'un bit = ____ µs (théorie 3,9 µs) ; période des trames = ____ ms

### X9. Bus I²C

- **Montage** : voie 1 sur J11 broche 3 (SDA), voie 2 sur J11 broche 4 (SCL).
- **Réglages** : 1 V/div, 5 µs/div ; décodage I²C si disponible.
- **Action** : commande `scan`, puis `imu`.
- **Attendu** : 400 kHz ; fronts descendants raides, fronts montants arrondis (c'est la résistance de pull-up de 2,2 kΩ qui charge la capacité du bus) ; réponse (ACK) à l'adresse **0x6A**.
- **On apprend** : pourquoi l'I²C a besoin de pull-ups et comment leur valeur fixe le temps de montée.
- **Mesures** : temps de montée 10–90 % = ____ ns (limite I²C 400 kHz : 300 ns)

### X10. Capteurs Hall

- **Montage** : voie 1 sur J17 broche 2 (Hall gauche), couplage DC.
- **Réglages** : 500 mV/div, 200 ms/div.
- **Action** : `wings open` puis `wings close` (ou tourner l'aile à la main, tourelle en mode banc).
- **Attendu** : une tension qui passe d'un niveau à l'autre quand l'aimant approche ; du bruit par-dessus quand le servo tourne.
- **On apprend** : lire un capteur analogique et choisir des seuils avec de la marge par rapport au bruit.
- **Mesures** : tension aile fermée = ____ V, ouverte = ____ V ; bruit crête-à-crête = ____ mV ; comparer avec la commande `hall` (ADC 12 bits : ~0,8 mV par pas)

---

## Partie 3 — Audio

### X11. Sortie de l'ampli classe D

- **Montage** : **règle n°1.** Voie 1 sur OUT+, voie 2 sur OUT−, masses sur GND, fonction **A − B**.
- **Réglages** : 2 V/div, d'abord 2 µs/div (modulation), puis 1 ms/div (son).
- **Action** : `tone 1000 3000` ; essayer `gain 9`, `gain 12`, `gain 15`.
- **Attendu** : à 2 µs/div, des créneaux à quelques centaines de kHz (la modulation) ; à 1 ms/div, la sinusoïde de 1 kHz qui apparaît dans A − B ; l'amplitude augmente avec le gain.
- **On apprend** : comment un ampli classe D fabrique un son avec des créneaux, et ce qu'est une sortie en pont.
- **Mesures** : fréquence de modulation = ____ kHz ; amplitude A − B à 9 / 12 / 15 dB = ____ / ____ / ____ V

### X12. Chasse au « plop »

- **Montage** : comme X11, 500 mV/div, 50 ms/div, déclenchement unique sur A − B.
- **Actions** : (a) brancher la tourelle ; (b) `mute on` / `mute off` ; (c) `reboot` ; (d) une mise à jour OTA.
- **Attendu** : **aucune impulsion notable** : le firmware coupe l'ampli (SD à l'état bas) avant d'arrêter l'I²S et ne le rallume qu'avec les horloges stables.
- **On apprend** : pourquoi l'ordre des opérations compte (règle « ne jamais arrêter LRCLK pendant que BCLK tourne »).
- **Mesures** : amplitude du plus gros pic en (a) ____ (b) ____ (c) ____ (d) ____

---

## Partie 4 — Consommation fine au PPK2 (mode banc)

Montage commun : PPK2 en **mode source à 5,0 V**, relié à l'entrée de la tourelle par un adaptateur USB-C (sans rien modifier) ; **SW1 fermé** au démarrage (aucun servo) ; LED verte reliée à une entrée logique du PPK2 si le mode repères L1 est actif.

### X13. Signature du démarrage

- **Attendu** : les étapes du boot visibles dans le courant : NeoPixels effacés, I²S, **gros pic de la calibration Wi-Fi**, serveur web.
- **On apprend** : la séquence de boot réelle comparée au plan (§3.2) ; pourquoi le Wi-Fi démarre avant les servos.
- **Mesures** : durée totale du boot = ____ ms ; pic du Wi-Fi = ____ mA

### X14. Pointes du Wi-Fi

- **Action** : page web ouverte sur un téléphone, rafraîchie chaque seconde.
- **Attendu** : des pointes brèves de plusieurs centaines de mA à chaque émission.
- **Mesures** : pointe = ____ mA ; courant moyen = ____ mA

### X15. Où part l'énergie au repos

- **Action** : mesurer le courant moyen avec : `set LedBright 0`, `64`, `255` ; `mute on` / `mute off` ; Wi-Fi `on` / `off`.
- **Mesures** : LEDs 0 / 64 / 255 = ____ / ____ / ____ mA ; ampli muet / actif = ____ / ____ mA ; Wi-Fi off / on = ____ / ____ mA

---

## Aides firmware à ajouter (catalogue L de [firmware-improvements.md](firmware-improvements.md))

| # | Aide | Sert à |
|---|---|---|
| L1 | **Repères de boot** : réglage `LabMarkers` ; la LED verte change d'état à chaque étape du boot et à chaque attachement de servo | aligner une courbe (oscilloscope, PPK2) sur le code — X4, X13 |
| L2 | **Balayages** : `sweep servo <n> <ms>` (lent, aller-retour), `sweep tone <Hz début> <Hz fin> <ms>` | X6, X11 |
| L3 | **Motif NeoPixel de test** : `led pattern bit` (un seul bit à 1, trame fixe répétée) | lire les bits à l'oscilloscope — X7 |
| L4 | **Cycle de charge** : `loadtest <écart ms>` attache et bouge tous les servos avec l'écart donné, puis les détache | X4 sans redémarrer |
| L5 | **Événements PWR_FLT horodatés** dans le log, avec l'état en cours (servos attachés, LEDs, son) | corréler un défaut avec ce qui tournait |

Jusqu'à ce que ces aides existent, les expériences marchent avec les commandes actuelles (`servo`, `tone`, `led`, `set`, `demo`, `reboot`).

## Résultats — journal des séances

| Date | Expérience | Résultat principal | Écart avec l'attendu / suite à donner |
|---|---|---|---|
| | | | |

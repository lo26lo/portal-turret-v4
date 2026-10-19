# Améliorations et fonctionnalités — plan et journal de reprise

Créé le 26.09.2026. Ce fichier **est versionné** (contrairement à `docs/firmware-journal.md` et `CLAUDE.md`, qui sont locaux depuis le commit `210b05d`). Il contient à la fois le plan des améliorations qui suivent les lots 1 à 12 de [firmware-plan.md](firmware-plan.md) et leur journal d'avancement.

> Dates au format JJ.MM.AAAA. Tout le code concerné est dans `Turret_firmware/` (`Fork/` reste en lecture seule).

## Reprise rapide (VS Code)

1. `git checkout main && git pull`, ouvrir **`Turret_firmware/`** comme dossier dans VS Code (PlatformIO ne détecte `platformio.ini` qu'à la racine du dossier ouvert).
2. Lire « État actuel » ci-dessous, puis le premier lot du tableau « Avancement » qui n'est pas *fait*, et sa fiche dans « Catalogue ».
3. Relire les règles matérielles de [firmware-plan.md](firmware-plan.md) §2 et de [../Turret_firmware/README.md](../Turret_firmware/README.md) §13 avant de toucher au code (IO21 / IO47 jamais à l'état haut, ampli coupé avant tout arrêt de l'I²S, pas d'eFuse ESP32 brûlé…).
4. À la fin de chaque session : mettre à jour « État actuel », « Avancement », « Erreurs » et ajouter une entrée dans « Sessions », **dans le même commit** que le travail. Reporter aussi dans le journal local si on l'utilise.
5. Outils : deux postes Windows utilisés, sans compilateur C++ hôte (les tests natifs ne tournent que dans la CI). Le port COM de la carte change d'un poste à l'autre : le retrouver avec `pio device list` (VID:PID 303A:1001, MAC 10:51:db:5a:17:ec), puis `pio run -e turret2_bringup -t upload --upload-port COMx`.
   - poste du 17.10.2026 : PlatformIO hors du PATH → `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`, et son `python.exe` pour pyserial ; carte sur **COM3** ;
   - poste du 19.10.2026 : `pio` dans le PATH (`C:\DATA\python3\Scripts\pio.exe`, cœur PlatformIO dans `C:\DATA\platformio`), pyserial dans `C:\DATA\python3\python.exe` ; carte sur **COM5**.

## État actuel — 19.10.2026

- **GitHub** : `main` = commit du 19.10.2026 « OLED: boot no longer hangs in U8g2 begin; 5 s fault alert » (correction de l'écran, alerte de défaut, ce fichier), après `a4eb329` (flash corrigé + battement de cœur) ; CI verte (run 39216617348), `test_heartbeat` et `test_fault_alert` compris. Puis commit « Rotary encoder, factory reset with SW1 + PROG, Qwiic module plan » : l'encodeur rotatif I²C (Q1 : `sensors/RotaryEncoder`, commande `volume`, touches d'encodeur du menu, tests `test_menu_encoder` et `test_volume_screen`) et la remise à zéro par SW1 + PROG (Q2 : commande `factory-reset`, tests `test_reset_gesture` et `test_factory_reset_screen`) — compilés, poussés à la demande de l'utilisateur **sans avoir été flashés** (carte débranchée), aucun encodeur encore reçu.
- **Firmware** : lots 1 à 12 du plan + améliorations **I1, I2, IL, IM** faits et poussés ; 25 tests natifs et `tools/check_web.py` (40 vérifications) ; `test_heartbeat` pas encore vu passer dans la CI. Restent **I3** (B1 visée, B3 mode recherche, C1 vue radar de la page web ; la vue radar existe déjà sur l'OLED), **I4** et **I5**.
- **Carte réelle** : **première carte flashée le 17.10.2026** (env `turret2_bringup`, `upload` + `uploadfs` par l'USB natif) ; elle démarre et la console répond. Auto-test rapide : 9 OK, **1 échec (radar : aucune trame)**, 9 non faits. **19.10.2026 : écran OLED 0,96" (SSD1306, `OledType` 0) sur J11 vu fonctionner** en mode normal, après correction d'un blocage au boot (voir « Erreurs »). Avancement de la mise en service : colonne « État » de [firmware-plan.md](firmware-plan.md) §8.
- **Jamais vu sur le matériel** : les menus du mode debug sur l'écran (SW1 fermé), le son, les LEDs, les mouvements des servos et des ailes, la page web sur un vrai téléphone.
- **Prochaine action** (dans l'ordre) :
  1. radar (défaut 4 affiché par l'écran et la LED rouge) : vérifier le branchement sur J5 et le croisement TX / RX, puis `selftest quick` ;
  2. fermer SW1 (mode debug) tant que la carte est sur l'USB du PC (en mode normal, les 4 servos s'attachent au boot) ; essayer les menus sur l'OLED ;
  3. calibration : `ImuUpAxis` (tourelle debout), seuils Hall aimant près / loin (plan §8 G) ;
  4. sur une alimentation 3 A : son, LEDs, servos un par un, puis `selftest full` (plan §8 E, F, H) ;
  5. dès que la carte est rebranchée : flasher et essayer l'encodeur (Q1, `key cw` sans module) et la remise à zéro SW1 + PROG (Q2) ;
  6. à la réception du MPR121 : Q3 (catalogue Q) ;
  7. ensuite seulement : lot I3.
- **Écrans** (05.10.2026) : 0,96" (SSD1306) et 1,54" « M154_4P » (SSD1309 très probable, non confirmé) gérés tous les deux ; un servo à la fois par défaut, levable ; menus en français et en anglais.
- **Matériel figé** (26.09.2026) : aucune modification de la carte ni du câblage, sauf « méga plus » (aucun identifié). Les mesures se font sans modification : cahier [experiments.md](experiments.md).
- **eFuse** : TPS259573 identifié en **auto-retry** (plan §2.5, D5) — cas déjà couvert par la détection de boucle de redémarrage ; reste à lire dans la datasheet le délai d'auto-retry pour vérifier que 3 cycles tiennent dans la fenêtre de 60 s du compteur.

## Avancement

| Lot | Contenu | Statut | Commit / remarque |
|---|---|---|---|
| I1 | A1 radar muet, A2 repos + zone de détection, D4 version git, D2 CI GitHub Actions | fait | 26.09.2026 ; build `turret2` / `turret2_bringup` OK, `tools/check_web.py` 22/22 ; CI GitHub **réussie** au 1ᵉʳ passage (run 36265316764, commit `a54d633`, 5 min 42 dont 5 min 21 de compilation sans cache) |
| I2 | A3 journal de crash + coredump, A4 watchdog, D1 tests natifs | fait | 26.09.2026, `cf57922` ; build OK, `check_web.py` 27/27 ; 12 tests natifs **réussis dans la CI** |
| I3 | B1 visée, B3 mode recherche, C1 vue radar | à faire | dépend de I1 (zone de détection) |
| I4 | B2 répliques vocales, C4 gestion des sons, B4 prise en main / renversement | à faire | |
| IL | L1–L5 : aides firmware pour les expériences (repères LED, balayages, motif NeoPixel, cycle de charge, PWR_FLT horodaté) | fait | 26.09.2026, `cf57922` ; build OK, `check_web.py` 27/27, CI verte |
| IM | M1–M7 : mode debug avec écran OLED sur J11, navigation par les boutons A / B, pages d'information, tests, calibration | fait | 05.10.2026, commits `83544a4` puis `8a88267` (tâche d'affichage + dix idées d'écran) ; passé avant I3 à la demande de l'utilisateur ; `check_web.py` 40/40 ; **CI verte** (run 37303638236) avec 24 tests natifs ; 19.10.2026 : blocage au boot corrigé, écran 0,96" vu fonctionner (mode normal) ; menus du mode debug pas encore essayés sur l'écran |
| HW | Mise en service de la première carte (plan §8, étapes A à J) | en cours | 17.10.2026 : flash + premier démarrage OK (étape B) ; radar sans trame ; détail par étape dans plan §8 |
| I5 | au choix : A5, B5–B8, C2, C3, C5, C6, D3 | à faire | à trier avec l'utilisateur |
| IQ | Modules Qwiic sur J11 : Q1 encodeur rotatif, Q2 remise à zéro par SW1 + PROG (sans module), Q3 touche capacitive MPR121, Q4–Q7 en option | en cours | Q1 et Q2 écrits le 19.10.2026, poussés **sans essai sur la carte** (débranchée) ; Q3 à la réception du MPR121 (commandé ou à commander : ADA1982 + ruban cuivre PRT-13827) |

## Catalogue

Effort : S = quelques heures, M = une journée, L = plusieurs jours.

### A. Fiabilité

| # | Quoi | Pourquoi / détail | Effort |
|---|---|---|---|
| A1 | **Radar muet = aucune cible** | `Turret_firmware/src/states/IdleState.cpp` déclenche sur `target.available` sans vérifier que le radar envoie encore des trames (`Radar` a déjà un indicateur de vie, `lastSensorUpdateTime` / « radar alive »). Radar débranché ou planté avec une cible en mémoire → cycles de tir sans fin. Corriger : cibles invalidées quand le radar est muet (timeout), et `Idle` exige un radar vivant | S |
| A2 | **Repos après `Disengage` + zone de détection** | aujourd'hui n'importe quelle cible, n'importe où, déclenche ; une personne immobile devant fait tirer en boucle. Réglages : `CooldownMs`, `DetectMaxMm`, `DetectAngle` (demi-angle), vitesse minimale éventuelle. Clés NVS ≤ 15 caractères | S |
| A3 | **Journal de crash** | dernières lignes du log dans une zone `RTC_NOINIT` (survit au reset logiciel, au panic, au watchdog), relues et affichées au boot et sur la page ; coredump écrit dans la partition `coredump` de `default_8MB.csv` et téléchargeable (`GET /api/coredump`) | M |
| A4 | **Watchdog de tâche** sur `loop()` | un blocage (I²C, radar, audio) fige la tourelle en silence ; avec le watchdog elle redémarre et la cause apparaît (raison du reset + A3) | S |
| A5 | **Mouvements adoucis** | accélération progressive des rotations et des canons : pics de courant plus faibles sur l'eFuse, mouvements plus proches du jeu | M |

### B. Comportement « vraie tourelle Portal »

| # | Quoi | Détail | Effort |
|---|---|---|---|
| B1 | **Visée réelle** | le LD2450 donne x / y en mm : angle = atan2(x, y) → rotation Z qui suit la cible (lissée), bornée ; aujourd'hui les rotations restent centrées | M |
| B2 | **Répliques vocales** | « Target acquired », « Are you still there? », « Searching », « I see you », « Hello friend »… lues depuis LittleFS (1,5 Mo) avec le décodeur MP3 déjà compilé. **Fichiers audio jamais commités** (droits Valve) : envoyés depuis la page (C4) | M |
| B3 | **Mode recherche** | cible perdue → balayage gauche / droite quelques secondes, puis fermeture | S |
| B4 | **Prise en main / renversement** | l'IMU détecte soulèvement ou basculement : canons rentrés, réplique « Put me down! » | S |
| B5 | **Personnalités** | normale, « défectueuse », amicale (ne tire jamais), au choix dans la page | M |
| B6 | **Œil laser** | effets de la LED centrale de l'anneau : respiration au repos, fixe à l'acquisition, clignote au tir | S |
| B7 | **Horaires / nuit** | via le NTP : silencieuse ou endormie à certaines heures, volume réduit le soir | S |
| B8 | **Plusieurs tourelles** (ESP-NOW) | réveil en chaîne, « opéra » des tourelles | L |

### C. Page web et connectivité

| # | Quoi | Effort |
|---|---|---|
| C1 | **Vue radar en direct** : canvas avec les cibles et la zone de détection (A2), zone modifiable à la souris | M |
| C2 | **Temps réel** par WebSocket ou SSE au lieu de l'interrogation chaque seconde | M |
| C3 | **Sauvegarde / restauration des réglages** en JSON (mots de passe exclus) | S |
| C4 | **Gestion des sons** : envoi, écoute, suppression, association son ↔ événement (avec B2) | M |
| C5 | **Home Assistant par MQTT** : état, compteur de tirs, démo, mute, auto-découverte | M |
| C6 | **Statistiques** : cycles par jour, cibles vues, dernière activité | S |

### D. Qualité logicielle

| # | Quoi | Effort |
|---|---|---|
| D1 | **Tests natifs** (`pio test -e native`, sans carte) : parseur radar, bornage des réglages, interpréteur de commandes, logique Hall / seuils ; nécessite d'isoler ces parties des API Arduino | M |
| D2 | **CI GitHub Actions** : compilation des envs `turret2*` + tests natifs + test de la page contre `mock_server.py`, à chaque push | S |
| D3 | **Analyse statique** (`pio check`, cppcheck) et formatage automatique | S |
| D4 | **Version tirée de git** (`git describe --tags --dirty`) par script de pré-compilation, affichée dans la bannière, la page et `/api/status` | S |

### E. Mesures sans modification matérielle

La carte et le câblage sont figés (décision du 26.09.2026). Les mesures passent par les points de test (VBUS, 5V, 3V3) et les connecteurs, avec l'oscilloscope, le PPK2 (mode banc uniquement, < 1 A) et un éventuel testeur USB-C en ligne. Programme complet, montages et zones de résultats : **[experiments.md](experiments.md)** (X1 à X15).

Idées écartées : INA219 / INA226 en ligne (demande de couper un fil et, avec le shunt d'origine de 0,1 Ω, ajoute 0,3 V de chute à 3 A), modules Qwiic *pour les mesures* (les modules Qwiic d'interface sont revenus le 19.10.2026 : catalogue Q), modifications pour une carte v0.2 (pull-down sur IO14–16, mesure du 5 V par ADC) — gain trop faible pour un projet de loisir.

### L. Aides firmware pour les expériences

| # | Quoi | Effort |
|---|---|---|
| L1 | **Repères de boot** : réglage `LabMarkers` ; la LED verte (IO33) change d'état à chaque étape du boot et à chaque attachement de servo, pour aligner une courbe d'oscilloscope ou de PPK2 sur le code | S |
| L2 | **Balayages** : `sweep servo <n> <ms>`, `sweep tone <Hz début> <Hz fin> <ms>` | S |
| L3 | **Motif NeoPixel de test** : `led pattern bit`, une trame fixe avec un seul bit à 1, facile à lire à l'oscilloscope | S |
| L4 | **Cycle de charge** : `loadtest <écart ms>` attache et bouge tous les servos avec l'écart donné, puis les détache (expérience X4 sans redémarrer) | S |
| L5 | **Événements PWR_FLT horodatés** dans le log avec l'état en cours (servos attachés, LEDs, son) | S |

### M. Mode debug avec écran OLED (idée de l'utilisateur, 05.10.2026)

**Principe** : SW1 fermé au démarrage = **mode debug** (c'est le « mode banc » actuel, étendu). Un écran OLED I²C branché sur le port Qwiic **J11** (« I2C extender ») affiche des pages d'information et un menu de tests ; les boutons **A et B servent à naviguer** au lieu de leurs fonctions normales (démo, mute, Wi-Fi). Aucune modification de la carte : l'écran se branche sur J11 (3,3 V, SDA IO35, SCL IO36, bus partagé avec l'IMU).

| # | Quoi | Détail | Effort |
|---|---|---|---|
| M1 | **Pilote d'écran** | détection au boot (scan I²C : 0x3C, sinon 0x3D), bibliothèque figée, module `ui/Display` ; rafraîchissement seulement sur changement et au plus ~5 fois par seconde (une trame de 1 Ko à 400 kHz ≈ 25 ms pendant lesquels `loop()` est occupé) ; écran absent = mode banc actuel, sans rien d'autre | S |
| M2 | **Navigation à deux boutons** | A court = suivant, A long = précédent ; B court = valider / entrer, B long = retour ; « appui long » ramené à ~0,6 s dans ce mode (3 s en mode normal) ; arbre de menus décrit par une table ; logique pure dans `src/logic/MenuLogic.h`, testée en natif | M |
| M3 | **Pages d'information** (lecture seule, rafraîchies en direct) | voir « Contenu proposé » | M |
| M4 | **Menu de tests** | chaque entrée exécute une commande existante de `control/Actions` (`wings open`, `servo …`, `tone …`), donc même comportement et mêmes sécurités que la console et la page web ; résultat affiché sur une ligne | M |
| M5 | **Calibration à l'écran** | capture Hall ouvert / fermé, capture de l'axe IMU, trims des ailes par pas de 5 µs avec essai, enregistrement | M |
| M6 | **Écran en mode normal** (SW1 ouvert, écran laissé branché) | une seule page d'état (état, défauts, IP, heure), sans navigation : A et B gardent leurs fonctions normales | S |
| M7 | **Écran virtuel** dans la page web et le simulateur | `GET /api/screen` renvoie l'image de l'écran ; permet de mettre au point les menus sans carte ni écran, et de voir l'écran à distance | S |

**Contenu proposé** (à valider)

*Pages d'information*

| Page | Contenu |
|---|---|
| État | état de la machine, défauts actifs (mêmes numéros que la LED rouge), durée de fonctionnement, version |
| Alimentation | PWR_FLT, nombre de défauts eFuse, brownouts, boucles de redémarrage, raison du dernier reset, crash / coredump présent |
| Radar | vivant ou muet, nombre de cibles, x / y / distance de chacune, « dans la zone » ou non |
| IMU | gravité x / y / z, axe dominant, debout ou non, température |
| Hall | valeurs brutes gauche / droite avec une barre, seuils, position (ouvert / fermé / entre) |
| Servos | les 6 sorties : attaché ou non, largeur d'impulsion |
| Audio | gain, volume, muet, lecture en cours |
| Réseau | nom du point d'accès, clients, réseau maison + IP + niveau, heure |

*Tests*

| Menu | Entrées |
|---|---|
| Ailes | gauche : ouvrir / fermer ; droite : ouvrir / fermer ; les deux |
| Canons | gauche : sortir / rentrer ; droit : sortir / rentrer |
| Rotations | X et Z : −, centre, + (par pas, A = −, B = +) ; relâcher |
| Servo au choix | choisir une des 6 sorties, angle par pas de 10°, relâcher ; balayage |
| LEDs | rouge, vert, bleu, blanc, éteint ; anneau / canon gauche / canon droit ; motif « un bit » |
| Son | tonalité 1 kHz, bruit de tir, gain 9 / 12 / 15, volume ±, muet |
| Cycle | démo complète ; cycle de charge (`loadtest`, avec confirmation « alimentation 3 A ? ») |
| Bus | scan I²C |

*Calibration* : Hall (capturer ouvert / fermé, enregistrer), IMU (« la tourelle est debout », capturer), trims des ailes (pas de 5 µs, essai 2 s, enregistrer).

*Réglages rapides* : volume, luminosité des LEDs, temps de repos, distance et angle de détection, repères de labo.

*Système* : Wi-Fi marche / arrêt, redémarrer, remettre les réglages à zéro (avec confirmation), effacer le coredump, quitter les tests (`resume`).

**Points à trancher** : modèle d'écran (SSD1306 128×64 de 0,96", SH1106 de 1,3", autre) — il fixe la bibliothèque (Adafruit SSD1306, ou U8g2 qui couvre les deux) et la mise en page ; mode debug = mode banc (un seul servo à la fois, prévu pour une alimentation par un PC) ou mode à part où tous les tests sont permis ; langue des menus.

**Pièges prévus** : en mode banc un seul servo est attaché à la fois, donc « les deux ailes » et la démo ne sont possibles que si le mode debug lève cette règle ; A + B au démarrage reste la remise à zéro des réglages (lue avant le menu) ; l'écran ajoute ses résistances de tirage en parallèle des 2,2 kΩ de la carte (sans conséquence à 400 kHz, à vérifier à l'oscilloscope, expérience X9) ; un écran qui bloque le bus est déjà couvert par la récupération de bus et le watchdog.

### Q. Modules Qwiic sur J11 (19.10.2026)

Branchés en chaîne sur le port Qwiic **J11** (3,3 V, bus partagé avec l'IMU et l'écran), sans modification de la carte ; chacun détecté au boot, absent = rien ne change. **Budget de tirage** : chaque module ajoute ses résistances (souvent 10 kΩ) en parallèle des 2,2 kΩ de la carte ; écran + encodeur ≈ 1,2 kΩ ≈ 2,7 mA, juste sous les 3 mA de la norme I²C → au-delà de trois modules, couper le cavalier de tirage de certains, et vérifier les fronts (expérience X9).

| # | Quoi | Détail | Effort | État |
|---|---|---|---|---|
| Q1 | **Encodeur rotatif** | Adafruit 4991 (0x36) / 5752 (0x49) / Soldered 333188 (0x30), voir « Décisions » ; menu en mode debug, volume + muet sinon | M | écrit, non essayé |
| Q2 | **Remise à zéro par les boutons de la carte** | SW1 fermé + PROG 5 s, LEDs sans écran ; aucun module | S | écrit, non essayé |
| Q3 | **Touche capacitive MPR121** (Adafruit ADA1982, STEMMA QT, 0x5A ; idée de l'utilisateur) | électrodes en ruban de cuivre adhésif (PRT-13827, colle conductrice, 5 mm ; ou ruban anti-limaces de jardinerie, plus large) collées dans la coque ou dehors en décor, fil fin soudé, fils courts loin des câbles servos / haut-parleur, pastilles de 2–4 cm² espacées d'au moins 1 cm. **Usages** : (a) **réactions** au toucher (« Hello friend », œil qui se ferme si on la caresse, « Put me down! » si on la secoue en la tenant ; avec B2 / B4) ; (b) **boutons** : appui court / long par pastille, **commande configurable par un réglage par entrée** (ex. `TouchCmd0` = `demo`, `TouchLong0` = `wifi off`), donc modifiable depuis la page web sans recompiler ; (c) navigation du menu en mode debug ; (d) **curseur** : 3–4 bandes côte à côte, sens du glissement (volume) ; (e) **proximité** : électrodes regroupées, main détectée à quelques cm avant le contact (l'œil s'ouvre). Seuils par entrée à régler sur la tourelle montée (taille, fils, épaisseur du plastique) ; clés NVS ≤ 15 caractères | M | à la réception |
| Q4 | **Horloge temps réel** PCF8523 (Adafruit, 0x68, pile CR1220) | l'heure sans NTP (point d'accès seul) : journal horodaté, base de B7 (nuit) et C6 (statistiques par jour) | S | idée |
| Q5 | **Tampon I²C TCA4307** (Adafruit ADA5159, 4,45 CHF) | branchement à chaud sur J11 sans bloquer le bus de l'IMU, récupération automatique d'un bus coincé, capacité des câbles isolée ; rien à coder | – | idée |
| Q6 | **Capteur de lumière** (VEML7700 / VEML6030, en rupture le 19.10.2026) | baisser LEDs et écran dans le noir, sommeil la nuit sans horloge | S | idée |
| Q7 | **Distance laser** (SparkFun dToF TMF8820, 3 × 3 zones) | courte portée en complément du radar : main devant, obstacle avant d'ouvrir les ailes | M | idée |

Jugés inutiles : PIR (le radar fait mieux), ampli STEMMA (MAX98357A sur la carte), Tsunami WAV Trigger (MP3 déjà lus depuis LittleFS), multiplexeur TCA9548A (seulement pour plusieurs modules à la même adresse ; compliquerait l'accès au bus partagé avec la tâche d'affichage), LTC4311 (câbles longs seulement).

## Décisions

| Date | Sujet | Décision | Raison |
|---|---|---|---|
| 26.09.2026 | Suivi des améliorations | ce fichier versionné, plan + journal réunis | le journal principal et `CLAUDE.md` sont locaux (commit `210b05d`) ; une reprise sur un autre poste ou dans le cloud doit trouver l'état dans git |
| 26.09.2026 | Ordre | I1 → I2 → I3 → I4 → I5 | I1 corrige un bug réel et sécurise `main` (CI) avant la mise en service |
| 26.09.2026 | eFuse | TPS259573 = auto-retry | recherche web (pages produit TI) ; datasheet à confirmer |
| 26.09.2026 | Matériel | **figé** : pas de modification de carte ni de câblage ; ouvert seulement pour un « méga plus », aucun identifié ; INA, Qwiic et idées v0.2 retirés | projet de loisir ; la carte a déjà l'essentiel (eFuse avec FLT, étoile 5 V, buck-boost, AHCT, points de test) |
| 26.09.2026 | Apprentissage | cahier d'expériences [experiments.md](experiments.md) + lot IL d'aides firmware | l'utilisateur veut tester et mesurer pour apprendre (oscilloscope, PPK2 à venir) |
| 26.09.2026 | A1 | cibles effacées après 1 s sans trame complète (`Radar::Update`) ; `Idle` n'accepte une cible que si le radar est vivant (même délai) | une cible gardée en mémoire par un radar muet faisait tirer sans fin ; 1 s = une dizaine de trames manquées. Le défaut LED 4 garde son délai de 3 s |
| 26.09.2026 | A2 | réglages `CooldownMs` (5000, 0..120000), `DetectMaxMm` (3000, 300..6000), `DetectAngle` (45°, 5..60), groupe « Detection » ; cible valable si y > 0, distance ≤ max et \|atan2(x, y)\| ≤ angle ; repos appliqué à **chaque** entrée en `Idle` (après un cycle et après le boot) ; le bouton A / `demo` passent outre | le plus simple et sans état caché ; 60° = champ du LD2450, 6 m = sa portée. Vitesse minimale non retenue (une personne immobile qui entre dans la zone doit déclencher) |
| 26.09.2026 | D4 | `scripts/git_version.py` (pré-build) → `src/version_gen.h` (ignoré par git, réécrit seulement s'il change) ; `FIRMWARE_VERSION` dans la bannière, `/api/status` (`version`, plus `built` = date de compilation) et la page | un `-D` dans `build_flags` aurait tout recompilé à chaque commit |
| 26.09.2026 | A3 | journal de crash : 2 Ko de fin de journal en mémoire RTC (`RTC_NOINIT_ATTR`, placée à `0x50000000`, vérifié dans le `.map`), avec nombre magique et bornes ; conservé si le reset n'est pas un POWERON, affiché au boot seulement après un reset de type crash (PANIC, *_WDT, BROWNOUT, UNKNOWN) ; coredump : déjà activé dans le SDK précompilé (flash, ELF, CRC32) → résumé au boot via `esp_core_dump_get_summary` (tâche, PC, backtrace), mis en cache (le contrôle relit toute l'image), téléchargement `GET /api/coredump` par blocs, effacement | pas d'outil à installer pour voir la cause d'un crash ; le décodage complet reste `espcoredump.py` avec le `firmware.elf` du même build |
| 26.09.2026 | A4 | watchdog de tâche sur `loop()` : `esp_task_wdt_init(8, true)` (l'en-tête IDF 4.4 confirme qu'un 2ᵉ appel met à jour délai et panic) + `enableLoopWDT()`, en fin de `setup()` ; scan I²C interrompu au 1ᵉʳ délai dépassé (code 5 de `endTransmission`) | 8 s plutôt que les 5 s du SDK : marge pour les opérations longues légitimes (effacement du coredump, réglages) ; 112 délais I²C de 50 ms dépasseraient le watchdog |
| 26.09.2026 | D1 | logique pure extraite dans `src/logic/` (en-têtes sans Arduino) : `RadarLogic.h` (décodage LD2450, zone), `HallLogic.h` (seuils, polarité, rail), `BoardLogic.h` (anti-rebond / appuis, code LED, gain) ; le firmware les utilise (Radar, Wing, Board, Amp) ; env `native` (`platform = native@1.2.1`, Unity), 12 tests dont l'équivalence du décodage avec la formule d'origine sur les 65 536 valeurs ; `pio test -e native` ajouté à la CI | les tests portent sur le code réellement utilisé ; parties trop liées au matériel (servos, I²S, réseau) laissées au simulateur et à la carte |
| 05.10.2026 | Lot IM : écran | **U8g2 2.36.18** (un seul pilote pour SSD1306, SSD1309, SH1106), tampon complet 128×64, police `u8g2_font_6x10_tf` (Latin-1, accents) ; réglage `OledType` (0 / 1 / 2, au prochain boot) car le contrôleur ne se détecte pas ; adresse détectée (0x3C puis 0x3D) ; ~~broches SCL / SDA passées au constructeur, donc U8g2 appelle `Wire.begin(35, 36)`~~ **corrigé le 19.10.2026** : aucune broche passée au constructeur (U8g2 met en sortie toutes les broches qu'il connaît, voir « Erreurs ») ; U8g2 appelle alors `Wire.begin()`, sans effet sur le bus déjà démarré par `I2cBus` | l'utilisateur a des 0,96" et un 1,54" ; Adafruit SSD1306 ne couvre pas officiellement le SSD1309 ni le SH1106 |
| 05.10.2026 | Lot IM : architecture | l'écran est **six lignes de texte de 21 colonnes** (`logic::MenuScreen`) ; navigation et rendu dans `src/logic/MenuLogic.h` (pur, testé en natif) ; arbre dans `src/ui/MenuTree.h` (données pures, deux langues) ; chaque entrée de test exécute une **commande texte de `Actions`** ; l'OLED n'est renvoyé que si le contenu change (une trame ≈ 25 ms de bus), au plus 5 fois par seconde ; les mêmes lignes sont servies par `GET /api/screen` (écran virtuel, avec ou sans OLED) | testable sans écran ni carte ; même comportement et mêmes sécurités que la console et la page ; pas de bitmap à transporter |
| 05.10.2026 | Lot IM : comportement | SW1 = **mode debug** (ancien mode banc) ; les boutons naviguent seulement si un OLED est présent (sinon fonctions normales, et menu par la page web ou `key`) ; appui long 0,6 s dans ce cas ; en mode debug un cycle de démo finit en `Manual` (jamais de déclenchement par le radar) ; **un servo à la fois par défaut** (`Board::IsServoLimited`), levé par `power full` : alors « les deux ailes », `demo`, `loadtest` sont permis et `resume` attache et fait le homing ; SW1 ouvert + écran = page d'état seule ; `Language` (défaut 1 = français) | réponses de l'utilisateur (« 2 ok », « les deux », « IM tout de suite ») ; un port USB de PC ne supporte pas plusieurs servos |
| 05.10.2026 | Lot IM : Wi-Fi sur l'écran | menu **WiFi** à la racine : pages réseau tourelle (nom, **mot de passe affiché**, 192.168.4.1, clients), réseau maison, réseaux trouvés (4 plus forts), date et heure ; actions scanner, reconnecter, oublier (`wifi forget`), marche / arrêt, mot de passe d'usine ; **pas de saisie du mot de passe de la box avec deux boutons** (reste sur la page web) | demande de l'utilisateur ; le mot de passe du point d'accès à l'écran sert à connecter un téléphone, il n'est visible que par qui est devant la tourelle interrupteur debug fermé (et par `/api/screen`, lui-même protégé par ce mot de passe) |
| 05.10.2026 | Lot IM : animation des ailes | `MenuScreen.wingAnimation` + pourcentages ; `logic::HallPercent` (0 % au seuil fermé, 100 % au seuil ouvert, polarité indifférente) ; `logic::RenderWings` (version texte) et `Display::DrawWings` (corps ovale, œil, deux panneaux qui s'écartent de 24 px, canons dans l'ouverture) ; affichée tant qu'une aile bouge + 0,8 s, dans les deux modes ; réglage `OledAnim` (défaut oui) ; cadence inchangée (5 images / s au plus) | « ça pourrait être sympa » ; pilotée par les capteurs réels plutôt que par le temps. **Risque** : chaque image occupe `loop()` ~25 ms, donc l'arrêt d'une aile sur son seuil Hall peut être détecté jusqu'à 25 ms plus tard — à mesurer sur la carte, d'où le réglage pour la couper |
| 05.10.2026 | Lot IM : envoi de l'image hors de `loop()` | tâche FreeRTOS `oled` (priorité 1, cœur 0, pile 4 Ko) qui dessine et envoie ; `Display::Show` ne fait que copier les six lignes sous mutex et notifier la tâche ; si plusieurs images arrivent pendant un envoi, seule la dernière est dessinée ; animation portée à 10 images / s. **Remplace** le risque noté plus haut (arrêt d'une aile retardé de 25 ms) | question de l'utilisateur : « on ne peut rien faire pour améliorer ? ». Vérifié dans les sources : `Wire` prend un verrou par transaction (`CONFIG_DISABLE_HAL_LOCKS` absent du SDK), y compris pour la lecture en deux temps de l'IMU ; U8g2 (`u8x8_cad_ssd13xx_fast_i2c`) découpe l'image en transactions de 24 octets (~0,7 ms), donc une lecture IMU depuis `loop()` attend ~1 ms au plus. Écartés : envoi par bandes dans `loop()` (~3 ms par bande, plus simple mais bloque encore) ; I²C à 1 MHz (hors spécification de l'écran et de l'IMU sur un bus partagé) |
| 05.10.2026 | Lot IM : dix idées d'écran (toutes demandées) | modèle d'écran généralisé : `logic::Graphic` (None, Wings, Radar, Graph, Qr, Eye) + `value[8]` + `data[216]`, toujours avec une version texte dans les six lignes ; accroche `infoGraphic` pour qu'une page d'information ajoute un dessin ; nouveau type d'entrée `Wizard` (les enfants sont les étapes, libellé = consigne sur 3 lignes, `logic::WrapText`) ; **QR code par le générateur du SDK** (`esp_qrcode_generate`, déjà lié : aucune dépendance ; la lib `ricmoo/QRCode` aurait un `qrcode.h` en conflit avec celui de l'IDF) ; auto-test dans `control/SelfTest` (séquence non bloquante, passe par les commandes de `Actions`) ; compteurs dans `control/Stats` (NVS `stats`) ; veille, retournement et contraste appliqués par la tâche d'affichage ; œil en mode normal (`OledFace`) | « j'aimerais tout » ; le texte reste la base commune de l'OLED, de la page web, du simulateur et des tests |
| 19.10.2026 | Remise à zéro par les boutons de la carte | **SW1 fermé + PROG (SW2, IO0) maintenu 5 s**, à tout moment une fois la tourelle démarrée (`logic::ResetGesture`) ; PROG doit avoir été vu relâché une fois depuis le boot ; lâcher PROG ou ouvrir SW1 annule ; LED rouge en clignotement rapide (100 ms) pendant le maintien, LED verte en clignotement rapide une fois accepté, jusqu'au redémarrage (2 s) ; compte à rebours sur l'écran s'il y en a un. Efface **tout** : réglages, statistiques (`Stats::Clear`), compteurs boucle / brownout (`Board::ClearCounters`), coredump. Commande `factory-reset` équivalente. IO0 lu en entrée seulement (`PIN_PROG`, `INPUT_PULLUP` après le boot) ; SW1 désormais lu aussi en marche pour ce geste. A + B au boot inchangé (réglages seulement) | demande de l'utilisateur : « reset usine avec les boutons de la carte, PROG, RST et peut-être le switch debug ; l'écran pourrait ne pas être branché ». RESET ne peut pas en faire partie : PROG enfoncé au relâchement de RESET = mode téléchargement de la ROM, et RESET coupe la puce (illisible) ; SW1 + maintien long = deux actions volontaires, sans minutage |
| 19.10.2026 | Encodeur rotatif I²C | module optionnel sur J11, **détecté au boot** dans l'ordre : Adafruit 4991 (seesaw, 0x36), Adafruit 5752 quadruple (seesaw, 0x49, bouton 0 seulement), Soldered 333188 (ATtiny404, 0x30, sans registre d'identité : un appareil en 0x30 qui renvoie 5 octets est pris pour lui) ; pilote maison `sensors/RotaryEncoder` (pas de bibliothèque : registres seesaw et protocole Soldered relevés dans les sources officielles), interrogé toutes les 30 ms (~1 ms de bus) ; mode debug avec écran : navigation (`MenuKey` Clockwise / Anticlockwise / Push / PushLong, traduites selon le mode par `MenuNav::FromEncoder`, la rotation ne répond jamais à une confirmation) ; sinon : volume par pas de 5 % + muet, écran du volume 1,5 s, enregistrement NVS 2 s après le dernier cran (commande `volume`) | demande de l'utilisateur (« menu + volume ») ; le 4991 visé d'abord était en rupture, l'utilisateur hésite entre le Soldered et le 5752 → les trois sont gérés. Interrogation et non interruption : le port Qwiic n'a pas de ligne INT |
| 19.10.2026 | Défauts sur l'écran | un défaut qui **apparaît** est affiché 5 s en plein écran (`logic::FaultAlert`, `logic::RenderFaultAlert` : un défaut par ligne avec son nom, code de la LED rouge en bas), dans les deux modes, avant le menu, l'animation et l'œil ; il réveille l'écran ; une touche le ferme plus tôt (mode debug) ; un défaut qui disparaît puis revient est annoncé de nouveau ; ensuite **seule la LED rouge** le signale (l'œil n'a plus de ligne « Défauts » permanente). Restent inchangées : la page d'état (`OledFace` à non) et la page « État » du menu, qui listent les défauts actifs | demande de l'utilisateur : « la LED rouge clignote pour le défaut, mais l'écran doit afficher le problème 5 secondes et passer à la suite » ; le défaut 4 (radar pas encore soudé) restait affiché en permanence |
| 26.09.2026 | Lot IL | L1 : `LabMarkers` (Bool, groupe Lab, appliqué aussitôt), `Board::Mark(label)` statique (bascule + ligne de journal), repères aux étapes 4 à 10, à chaque attache (`Gantry`) et à la fin du boot ; L2 : balayage servo en triangle, 1 consigne / 20 ms ; balayage de fréquence linéaire dans le générateur de tonalité (`PlayChirp`) ; L3 : `SetBitPattern` (luminosité 255 exacte car `FASTLED_SCALE8_FIXED` = 1, dithering coupé, luminosité restaurée) ; L4 : écart d'attache imposé en RAM (`Gantry::SetStaggerOverride`, 0 → 1 ms « tous ensemble »), ailes en `TestStop` ; L5 : ligne de journal avant le délestage ; balayage et `loadtest` annulés par `Fault` et `resume` | sans annulation, un balayage ou un `loadtest` redemanderait des servos pendant un défaut d'alimentation |
| 26.09.2026 | D2 | `.github/workflows/firmware.yml` : sur push / PR touchant `Turret_firmware/` ; PlatformIO 6.1.18 figé, cache `~/.platformio` + `libdeps` ; contrôle de la casse des `#include` ; build `turret2` + `turret2_bringup` ; `tools/check_web.py` (simulateur + `node --check`). Tests natifs ajoutés à la CI avec D1 (lot I2) | le runner est sous Linux, sensible à la casse (le fichier `FIringState.cpp` montre que le dépôt a déjà des noms atypiques) |

## Erreurs, impasses et pièges

| Date | Quoi | Conséquence / solution |
|---|---|---|
| 26.09.2026 | `www.ti.com` et `www.digchip.com` bloqués par le proxy du conteneur cloud (curl et WebFetch) | variante de l'eFuse obtenue par les extraits d'une recherche web seulement ; confirmer dans la datasheet depuis un poste normal |
| 26.09.2026 | Sous VS Code (Windows), l'outil Bash retire les `\` du texte des commandes, même dans un heredoc `<<'EOF'` : un script Python passé en ligne a échoué (`SyntaxError`) | écrire tout script ou code contenant des `\` dans un fichier (outil d'écriture), puis l'exécuter ; déjà vu aux lots 4 et 10 (journal local) |
| 17.10.2026 | Premier flash sur la carte : après l'écriture, le reset RTS d'esptool (`hard_reset`) laisse l'ESP32-S3 en mode téléchargement (ROM : `rst:0x15 (USB_UART_CHIP_RESET),boot:0x20 (DOWNLOAD(USB/UART0))`) ; console muette, partitions NVS et coredump vierges = l'application n'avait jamais tourné. `after_reset: watchdog_reset` démarre l'application mais le port USB disparaît pendant le reset : esptool lève une erreur série (`OSError(22)`) et pio affiche FAILED alors que tout est écrit et vérifié | `upload.after_reset = no_reset` dans `boards/turret2.json` + `scripts/reset_after_upload.py` (post-action de `upload` / `uploadfs`) qui relance esptool `--after watchdog_reset read_mac` et ignore la perte du port ; vérifié : SUCCESS et application démarrée |
| 19.10.2026 | Écran OLED branché sur J11 : le boot se fige juste après `Bus already started in Master Mode` (appel de `Wire.begin` par U8g2), écran noir ; l'écran est pourtant vu au scan (0x3C). Cause : les broches SCL / SDA passées au constructeur U8g2 ; son `U8X8_MSG_GPIO_AND_DELAY_INIT` (`U8x8lib.cpp`) fait `pinMode(OUTPUT)` sur toute broche connue d'indice < 16 (`U8X8_PIN_I2C_CLOCK` = 12, `U8X8_PIN_I2C_DATA` = 13) **avant** `Wire.begin`, ce qui retire IO35 / IO36 au contrôleur I²C et les tire à la masse : `u8g2->begin()` ne revient jamais (watchdog pas encore armé à ce stade du boot). Les redémarrages « POWERON » vus pendant l'enquête venaient des débranchements, et ont mis la carte en mode réduit (compteur de boots suspects) | constructeurs appelés sans broches (`U8X8_PIN_NONE` seul) ; vérifié : `Display: OLED at 0x3C, type 0`, boot complet en 1,7 s, écran affiché. Le simulateur et les tests natifs ne pouvaient pas le voir : ils n'exécutent pas U8g2 |
| 26.09.2026 | Dans le conteneur cloud, la branche locale était restée au commit `dae8587` alors que `main` avait avancé (lots 2 à 12 faits sous VS Code) | toujours `git fetch` + se placer sur `origin/main` en début de session ; le journal local n'étant plus versionné, l'état se lit dans ce fichier, `Turret_firmware/README.md` et les messages de commit |

## Sessions

### 26.09.2026 — Proposition du plan d'améliorations

**Demande** : un nouveau plan d'amélioration et de fonctionnalités ; puis « confirmer la variante latch-off ou auto-retry de l'eFuse ? » ; puis « Go et pousse le journal amélioration pour une reprise sur VS Code ».

**Fait**
- Relu l'état réel sur `main` (`7a20d46`) : `Turret_firmware/README.md`, `IdleState.cpp`, `Radar.cpp`, `platformio.ini` ; constaté l'absence de tests (`test/` vide), de coredump et de watchdog, et le bug A1 (radar muet non vérifié par `Idle`).
- Proposé le catalogue A–E et l'ordre I1–I5 (ce fichier).
- Recherche de la variante de l'eFuse : auto-retry ; noté dans [firmware-plan.md](firmware-plan.md) §2.5 et D5.
- Création de ce fichier, commit et push sur `main`.

**Non fait** : aucune ligne de code ; aucune compilation dans cette session.

**Prochaine étape** : lot I1.

### 26.09.2026 (2ᵉ entrée) — Matériel figé, cahier d'expériences

**Demande** : discussion sur un INA219 puis un Power Profiler Kit II ; l'utilisateur a déjà un oscilloscope ; matériel figé sauf « méga plus » ; envie de tester, mesurer et apprendre ; « oui go ».

**Fait**
- Création de [experiments.md](experiments.md) : règles de sécurité (sortie BTL, masses, PPK2 < 1 A, pas de court-circuit), points de mesure, 15 expériences (alimentation, signaux, audio, PPK2) avec montage, réglages, attendu, ce qu'on apprend et zone « Mesures », journal des séances.
- Vérifié que les commandes citées existent dans `Turret_firmware/src/control/Actions.cpp` (`servo rotx`, `tone`, `led`, `set`, `demo`, `reboot`).
- Ce fichier : décision « matériel figé », partie E remplacée, nouveau catalogue L et lot IL.

**Prochaine étape** : lot I1 (ou IL, petit, si l'on veut commencer par les aides aux mesures).


### 26.09.2026 (3ᵉ entrée) — Lot I1, sous VS Code

**Demande** : « fais un synch », puis « go » (lot I1).

**Fait**
- **A1** : `sensors/Radar` — `Update()` efface les cibles après 1 s sans trame ; `IsInZone`, `FirstTargetInZone` (renvoie -1 si le radar est muet).
- **A2** : `states/IdleState` — repos `CooldownMs` à l'entrée, puis première cible vivante dans la zone ; la cible retenue est journalisée (x, y). Réglages `CooldownMs`, `DetectMaxMm`, `DetectAngle`.
- **D4** : `scripts/git_version.py`, `platformio.ini` (`extra_scripts` en liste), `.gitignore` (`src/version_gen.h`), bannière (`Firmware: <version> (built …)`), statut JSON (`version`, `built`).
- **D2** : `.github/workflows/firmware.yml` ; `tools/check_web.py` (22 vérifications : identifiants, page, réglages listés = lignes de `Settings.cpp`, mots de passe jamais renvoyés, clés NVS ≤ 15 caractères, bornage, refus du mot de passe court, champs du statut, portail captif, scan Wi-Fi, syntaxe JS).
- `Turret_firmware/README.md` : `Idle`, radar, réglages de détection, version, CI, `check_web.py`.

**Testé** : `pio run -e turret2 -e turret2_bringup` SUCCESS (`turret2` : flash 1 480 797, 44,3 % ; RAM 75 828, 23,1 %) ; version générée `fd63cb0-dirty` ; `python tools/check_web.py` : 22/22 ; casse des `#include` : 0 écart ; YAML du workflow et script intégré validés (PyYAML + `compile`). **Non testé** : la CI elle-même (au premier push), le radar réel.

**Prochaine étape** : I2 (ou IL).

### 26.09.2026 (4ᵉ entrée) — Lot I2, sous VS Code

**Demande** : « dans l'ordre » (après I1 : lot I2).

**Fait**
- **A3** : `board/Log` (`CrashLog::Begin`, `PreviousRun`, copie RTC dans `Append`), `board/CoreDump.{h,cpp}` (nouveau), `main.cpp` (`CrashLog::Begin` juste après `board.Begin()`, `PrintCrashReport()` après la bannière), commandes `crashlog` et `coredump [erase]`, routes `GET /api/crashlog`, `GET /api/coredump`, `POST /api/coredump/erase`, statut `crash` ; page : bandeau « The turret crashed » dans Status, carte « Crash report » dans Maintenance.
- **A4** : watchdog 8 s sur `loop()` ; scan I²C interruptible.
- **D1** : `src/logic/*.h`, refactorisation de `Radar.cpp`, `Wing.cpp`, `Board.{h,cpp}` (`ButtonEvent` = alias de `logic::PressEvent`), `Amp.cpp` ; `test/test_logic/test_main.cpp` ; env `native` ; étape CI.
- Simulateur : champ `crash`, routes crash, commande `sim crash on|off` ; `check_web.py` : 5 vérifications de plus.
- `Turret_firmware/README.md` : diagnostics de crash, tests natifs, commandes, API.

**Testé** : build `turret2` et `turret2_bringup` SUCCESS du premier coup (`turret2` : flash 1 487 157, 44,5 % ; RAM 76 120, 23,2 %) ; `rtcLog` à `0x50000000` dans `.rtc_noinit` (`nm` + `.map`) ; `check_web.py` 27/27. **Non exécuté** : les tests natifs (aucun `gcc` sur ce PC ; WSL absent) — cas vérifiés à la main contre la logique, exécution à la CI. **Non testable sans carte** : crash réel, coredump réel, déclenchement du watchdog.

**Prochaine étape** : IL, puis I3.

### 26.09.2026 (5ᵉ entrée) — Lot IL, sous VS Code

**Demande** : « continue lot II » (compris : lot IL, suivant dans l'ordre). Pas de réponse sur le push du lot I2 : rien n'est commité.

**Fait**
- **L1** : `board/Board` (`SetLabMarkers`, `Mark`, battement suspendu, état initial synchronisé sur la LED réelle), réglage `LabMarkers`, repères dans `main.cpp` (étapes 4 à 10) et `motion/Gantry` (chaque attache), `Actions::ApplyAllSettings`.
- **L2** : `sweep servo`, `sweep tone`, `sweep stop` (`control/Actions`) ; `Audio::PlayChirp`.
- **L3** : `led pattern bit` ; `Light::SetBitPattern`.
- **L4** : `loadtest <écart>` (phases Attaching → Holding → Returning) ; `Gantry::SetStaggerOverride`.
- **L5** : `Actions::LoadSnapshot` ; ligne « PWR_FLT event #n » dans `UpdateFaults()`.
- `Light::IsEnabled`, `Audio::IsPlaying` ; simulateur : réponses pour `sweep`, `loadtest`, `led pattern bit`.
- Docs : `Turret_firmware/README.md` (aides de labo, réglage `LabMarkers`), [experiments.md](experiments.md) (aides disponibles, X4 avec `loadtest`).

**Testé** : build `turret2` et `turret2_bringup` SUCCESS du premier coup (`turret2` : flash 1 492 605, 44,7 % ; RAM 76 448, 23,3 %) ; `check_web.py` 27/27. Sans carte : rien de mesuré.

**Prochaine étape** : I3.

### 26.09.2026 (6ᵉ entrée) — Push, CI, documentation

**Demande** : « commit », « go » (push), puis « est-ce que toutes les docs sont à jour ».

**Fait**
- Commit `cf57922` (I2 + IL, un seul commit : mêmes fichiers) poussé ; CI run 36267773678 **verte** en 5 min 42 : casse des includes, compilation (5 min 15 : le cache ne garde que les paquets, pas `.pio/build`), **tests natifs 6 s**, vérification de la page.
- Relecture des docs : ce fichier (statuts « non commité » / « tests non exécutés » périmés, corrigés) ; [firmware-plan.md](firmware-plan.md) (encadré « État au 26.09.2026 » avec renvoi au README du firmware et liste des écarts : page protégée, redémarrage après toute OTA, Wi-Fi maison / NTP / portail captif ; §10.2 annoté) ; `README.md` racine (détection, diagnostics, arborescence avec `docs/` et `.github/`). À jour sans changement : `Turret_firmware/README.md`, [experiments.md](experiments.md). Non concernés : `design-plan.md`, `status-and-history.md` (matériel).

**Prochaine étape** : I3.

### 05.10.2026 — Reprise ; idée du mode debug avec écran OLED

**Demande** : reprendre avec le lot I3 ; nouvelle idée à ajouter au plan : utiliser l'interrupteur de mode (SW1) pour passer la tourelle en mode debug, brancher un écran OLED sur le port I²C (J11), attribuer les boutons A et B à la navigation, et réfléchir aux informations et aux tests utiles (bouger l'aile droite, son, etc.).

**Fait**
- Synchronisation : `main` à `b1df2c3`, rien de nouveau depuis le 26.09.2026.
- Catalogue : famille **M** (M1 pilote, M2 navigation, M3 pages d'information, M4 tests, M5 calibration, M6 écran en mode normal, M7 écran virtuel), contenu proposé des pages et des menus, points à trancher et pièges ; lot **IM** dans « Avancement ».

**Non fait** : aucun code ; lot I3 pas commencé (attente des réponses sur le lot IM et de sa place dans l'ordre).

**Prochaine étape** : I3, ou IM d'abord selon la réponse.

### 05.10.2026 (2ᵉ entrée) — Lot IM : mode debug avec écran OLED

**Demande** : « j'ai plein de 0.96 mais j'ai aussi un m154_4p, il faudrait pour les deux ; 2 ok ; 3 les deux ; IM tout de suite ».

**Fait**
- **M1** `ui/Display.{h,cpp}` : U8g2, détection 0x3C / 0x3D, trois contrôleurs, dessin des six lignes (titre et ligne sélectionnée en inverse).
- **M2** `logic/MenuLogic.h` : `MenuNav` (modes Browse / Info / Adjust / Confirm, touches A, A long, B, B long), `MenuScreen` ; `Debouncer::SetLongPressMs`, `Board::SetNavigationButtons` (0,6 s).
- **M3–M5** `ui/MenuTree.h` (≈ 75 entrées : Informations, Tests, Calibration, Réglages, Système) ; `ui/DebugUi.{h,cpp}` (8 pages d'information en direct, accroches vers `Actions`, réglages et pages).
- **M6** page d'état en mode normal. **M7** `GET /api/screen`, carte « Debug screen » dans l'onglet Tests de la page (écran + boutons A, A long, B, B long), commandes `key` et `screen`.
- `control/Actions` : `wing left|right open|close`, `gun left|right extend|retract`, `servos off`, `shot`, `power [full|limited]`, `cal hall … / cal imu`, `key`, `screen` ; `demo` permis depuis `Manual` en mode debug avec `power full` ; `wings` / `guns` / `loadtest` / `demo` refusés tant que limité à un servo ; `LimitToChannel`.
- `board/Board` : `IsServoLimited`, `SetFullPower` ; `states/BootState` (attache si non limité), `states/DisengageState` (fin en `Manual` en mode debug).
- Réglages `Language`, `OledType` (groupe Display). `platformio.ini` : `olikraus/U8g2@2.36.18`.
- Simulateur : `tools/mock_menu.py` lit `MenuTree.h` et reproduit la navigation ; `sim debug|oled on|off`. `check_web.py` : 12 vérifications de plus (arbre lu, libellés ≤ 19 caractères, **chaque commande du menu existe dans `Actions.cpp`**, chaque valeur réglable est un réglage, parents valides, écran à 6 lignes, touches).
- Tests natifs : 8 tests de menu (rendu, bouclage et défilement, commande et réponse, confirmation, pages d'information, réglage avec bornes et sauvegarde, sous-menu et retour, cohérence de l'arbre réel).
- Docs : `Turret_firmware/README.md` (section « Debug mode and OLED screen », réglages, commandes, API, arborescence), `README.md` racine.

**Recherche** : le libellé exact « M154_4P » n'a pas été trouvé ; les modules OLED 1,54" 128×64 I²C à 4 broches trouvés utilisent le SSD1309 (LCDWIKI MC154GX, Waveshare, BuyDisplay). D'où `OledType` = 1 pour cet écran, **à confirmer en le branchant** (si l'image est décalée de 2 pixels, essayer 2 = SH1106).

**Testé** : `pio run -e turret2 -e turret2_bringup` SUCCESS du premier coup (`turret2` : flash 1 520 977, 45,5 % ; RAM 78 440, 23,9 % ; +28 Ko de flash pour U8g2 et les menus) ; page 19 454 → 7 262 octets gzip ; `check_web.py` 39/39 ; parcours du menu sur le simulateur (Tests → Ailes → Droite : ouvrir ; Son → Tonalité 1 kHz exécute `tone 1000 500`). **Non exécuté** : tests natifs (CI). **Non testable sans matériel** : affichage réel, lisibilité, boutons physiques, contrôleur du 1,54".

**Pièges** : `snprintf` ne remplit pas la fin du tampon → `memset` de tout l'écran avant le rendu, sinon la comparaison octet à octet échoue et l'OLED est renvoyé en continu ; les libellés accentués occupent plus d'octets que de colonnes (lignes de 48 octets pour 21 colonnes).

**Prochaine étape** : essai par l'utilisateur sur le simulateur, commit / push (CI : tests natifs), puis I3.

### 05.10.2026 (3ᵉ entrée) — Lot IM : Wi-Fi sur l'écran, animation des ailes

**Demande** : « il faudrait encore mettre des infos et paramètres sur l'OLED, le Wi-Fi ; ça pourrait être sympa une animation lorsque les bras bougent ».

**Fait**
- Menu **WiFi** (`ui/MenuTree.h`, `MenuWifi`, pages `PageWifiAp`, `PageWifiHome`, `PageWifiScan`, `PageTime`) ; les entrées Wi-Fi quittent « Système ». `web/AccessPoint::GetPassword`, `web/Station` (4 réseaux les plus forts du dernier scan), commande `wifi forget`.
- **Animation** : `logic/HallLogic.h` (`HallPercent`), `Wing::GetOpenPercent`, `logic/MenuLogic.h` (`wingAnimation`, `RenderWings`), `ui/Display` (`DrawWings`), `ui/DebugUi::Update` ; réglage `OledAnim` et entrées de menu « Animation ailes oui / non ».
- Simulateur : pages Wi-Fi, `wifi forget`, animation en texte quand les ailes bougent.
- Tests natifs : `test_hall_percent`, `test_wing_animation_screen` (22 tests au total).
- Docs : `Turret_firmware/README.md`.

**Testé** : build `turret2` et `turret2_bringup` SUCCESS (`turret2` : flash 1 525 433, 45,6 % ; RAM 78 912, 24,1 %) ; `check_web.py` 39/39 ; sur le simulateur : page « Réseau tourelle » (nom, mot de passe, adresse), « Scanner » puis « Réseaux trouvés », animation `[]==(O)==[]` à 55 % pendant l'ouverture. **Non exécuté** : tests natifs (CI). **Non testable sans matériel** : le dessin réel de l'animation sur l'OLED et son effet sur la précision d'arrêt des ailes.

**Idée notée, non faite** : un QR code Wi-Fi à l'écran (`WIFI:T:WPA;S:…;P:…;;`, version 3 = 29 × 29 modules, 58 px à 2 px par module : il tient sur 64 px de haut) pour connecter le téléphone sans rien taper ; demande une petite bibliothèque de QR code.

**Prochaine étape** : essai sur le simulateur par l'utilisateur, commit / push, puis I3.

### 05.10.2026 (4ᵉ entrée) — Push du lot IM ; l'écran ne bloque plus la boucle

**Demande** : « génial vas-y et as-tu d'autres idées » ; puis, sur le risque des 25 ms par image : « on ne peut rien faire pour améliorer ? ».

**Fait**
- Commit `83544a4` (lot IM) poussé ; CI verte (run 37297330123), 22 tests natifs réussis. Raté : un premier `git commit -m` avec des guillemets dans le message a échoué en silence sous PowerShell (rien n'était commité ni poussé) → message passé par fichier (`git commit -F`) ; un `tools/__pycache__/*.pyc` s'était glissé dans l'index → retiré, `__pycache__/` ajouté au `.gitignore`.
- Idées d'écran proposées (voir « État actuel »), en attente de choix.
- `ui/Display` : tâche d'affichage (voir « Décisions ») ; `ui/DebugUi` : 10 images / s pendant l'animation.
- Docs : `Turret_firmware/README.md` (paragraphe « The screen never holds up the main loop »).

**Testé** : build `turret2` et `turret2_bringup` SUCCESS (`turret2` : flash 1 526 113, 45,7 % ; RAM 79 216, 24,2 %). **Non testable sans matériel** : le partage réel du bus entre la tâche d'affichage et l'IMU (raisonnement sur les sources seulement) — à vérifier à la mise en service : pas d'erreur I²C dans le journal, pas de lecture IMU aberrante pendant une animation.

**Prochaine étape** : choix des idées d'écran, puis I3.

### 05.10.2026 (5ᵉ entrée) — Lot IM : les dix idées d'écran

**Demande** : « passe aux idées, j'aimerais tout ».

**Fait**
1. **Retournement et contraste** : réglages `OledFlip`, `OledContrast` ; `Display::Configure`, appliqués aussitôt (`Actions::SetSettingsListener` → `DebugUi::ApplySettings`).
2. **Veille** : `OledSleepS` (300 s, 0 = jamais) ; réveil sur touche (la première ne fait que réveiller), mouvement d'aile, changement d'état, défaut d'alimentation ; `Display::SetPower`.
3. **Auto-test** : `control/SelfTest.{h,cpp}`, 19 contrôles (11 passifs, 8 actifs), résultats OK / FAIL / SKIP / CHECK ; commandes `selftest [quick|report|stop]` ; `GET /api/selftest` ; carte « Self-test » dans l'onglet Tests de la page ; page « Rapport auto-test » du menu. `Wing::LastMoveTimedOut`.
4. **Assistant de calibration** : entrée « Assistant » (7 étapes : Hall G ouvert / fermé, Hall D ouvert / fermé, enregistrer, IMU debout, rappel des trims).
5. **Courbes Hall** : 100 échantillons par aile (un toutes les 50 ms, 5 s), seuils en pointillés.
6. **Journal** (4 dernières lignes) et **dernier crash** (résumé du coredump ou dernières lignes avant le reset).
7. **QR codes** : rejoindre le réseau de la tourelle (`WIFI:T:WPA;S:…;P:…;;`, champs échappés), adresse de la page web (IP maison si connectée).
8. **Vue radar** : zone de détection (arc + deux droites), cibles en points pleins (dans la zone) ou cercles, pleine échelle 6 m.
9. **Œil** en mode normal : endormi / éveillé (pupille vers la première cible) / en colère ; sous-titres aux changements d'état ; défauts actifs ; heure. `OledFace` pour revenir à la page d'état.
10. **Statistiques** : `control/Stats.{h,cpp}` (démarrages et cycles en NVS, cibles et dernier cycle depuis le boot) ; commande `stats`, page du menu, `/api/status`, ligne « Cycles » de la page web.
- Simulateur (`mock_menu.py` : entrées sur plusieurs lignes, assistant ; `mock_server.py` : nouvelles pages, `selftest`, `stats`), `check_web.py` (40 vérifications ; les « 39/39 » des entrées précédentes étaient un décompte erroné, il y en avait 35), tests natifs (`test_wrap_text`, `test_menu_wizard`, arbre réel avec étapes : 24 tests).
- Docs : `Turret_firmware/README.md`.

**Testé** : build `turret2` et `turret2_bringup` SUCCESS du premier coup (`turret2` : flash 1 553 921, 46,5 % ; RAM 82 592, 25,2 %) ; page 20 348 → 7 602 octets gzip ; `check_web.py` : tout passe ; 115 entrées de menu, 21 pages, 7 étapes ; assistant parcouru sur le simulateur. **Non exécuté** : tests natifs (CI au prochain push). **Jamais vu sur un écran** : aucun des cinq dessins (ailes, radar, courbe, QR, œil) — positions calculées, à ajuster sur le matériel ; lisibilité du QR code par un téléphone à vérifier (surtout sur le 0,96").

**Rappel** : la tâche d'affichage (entrée précédente) et ces dix idées ne sont pas encore commitées.

**Prochaine étape** : commit / push (CI), puis I3 (la vue radar de l'écran en couvre déjà une partie ; reste la visée, le mode recherche et la vue radar de la page web).

### 05.10.2026 (6ᵉ entrée) — Push des idées d'écran

**Demande** : « oui pousse ».

**Fait** : commit `8a88267` (tâche d'affichage + dix idées d'écran) poussé ; **CI verte** (run 37303638236) : casse des includes, compilation des deux envs, **24 tests natifs** (dont `test_wrap_text` et `test_menu_wizard`, exécutés pour la première fois), vérification de la page.

**Prochaine étape** : I3 (B1 visée, B3 mode recherche, C1 vue radar de la page web). Rien de ce lot IM n'a été vu sur un écran réel.

### 17.10.2026 — Premier flash sur une vraie carte (COM3)

**Demande** : « la carte est branchée sur COM3, peux-tu flasher le firmware de base ». Le firmware d'origine (`Fork/`) ne convient pas (cible 4 Mo + PSRAM) : c'est notre firmware, env `turret2_bringup`, qui a été flashé.

**Problème et solution** : voir « Erreurs, impasses et pièges » (17.10.2026) : reset après flash laissant la puce en mode téléchargement ; corrigé par `no_reset` + `scripts/reset_after_upload.py` ; nouveau flash : SUCCESS, l'application démarre seule.

**Constaté au premier démarrage** (console USB) :
- ESP32-S3 QFN56 rév. v0.2, flash 8 Mo GD intégrée, pas de PSRAM, MAC 10:51:db:5a:17:ec.
- Scan I2C : 0x6A seul ; LSM6DSOX OK, gravité (-8,73 ; 2,86 ; 3,50) m/s², |g| ≈ 9,83, axe dominant **-X** (ImuUpAxis encore à 0), 26,6 °C. Pas d'OLED branché.
- Audio I2S démarré ; point d'accès « Portal Turret » actif ; 4 servos attachés à 250 ms d'écart (1450 µs) ; démarrage en ~4 s, puis Idle.
- Raison du reset POWERON, PWR_FLT haut, 0 défaut, 0 brownout.
- Auto-test rapide : 9 OK, **1 FAIL (trames radar)**, 9 SKIP.
- Hall gauche 353, droite 187, tous deux vus « fermés » : calibration à faire.

**À faire** : vérifier le radar (branché sur J5 ? TX/RX croisés ?) ; calibration Hall et ImuUpAxis ; auto-test complet sur alimentation 3 A ; utiliser le mode debug (SW1) tant que la carte est alimentée par l'USB du PC.

### 18.10.2026 — État des lieux, plan mis à jour pour la reprise

**Demande** : un état des lieux, savoir si GitHub est à jour, et mettre le plan à jour pour une reprise propre.

**Constaté** : `git fetch` → `main` local = `origin/main` = `8a88267` ; la correction du flash du 17.10.2026 et la documentation correspondante ne sont pas encore commitées.

**Fait** :
- « État actuel » réécrit (daté du 18.10.2026) avec la liste ordonnée des prochaines actions ;
- « Reprise rapide » : outils du PC (chemin de pio, Python de PlatformIO pour pyserial, COM3) ;
- tableau « Avancement » : lot IM complété (`8a88267`), nouvelle ligne **HW** (mise en service) ;
- [firmware-plan.md](firmware-plan.md) §8 : colonne « État » par étape (A à J).

**Prochaine étape** : commit / push de tout cela si l'utilisateur le demande, puis radar et mise en service (voir « État actuel »).

### 18.10.2026 (2ᵉ entrée) — LED verte : battement de cœur

**Demande** : la LED verte doit rester allumée pendant le boot, puis battre comme un cœur. L'utilisateur voyait aussi la rouge clignoter : c'est le code de défaut 4 (radar muet), attendu tant que le radar n'envoie rien ; non modifié.

**Fait** :
- `logic::HeartbeatOn()` (`src/logic/BoardLogic.h`) : deux battements de 100 ms (à 0 et 250 ms) puis repos, période 1200 ms (50 par minute) ; remplace le flash de 80 ms par seconde ;
- `Board::UpdateGreenLed()` l'utilise ; fixe pendant le boot inchangé ; `LabMarkers` garde la priorité ;
- test natif `test_heartbeat` (25 tests) ; README §7 mis à jour.

**Vérifié** : compilation `turret2_bringup` OK (RAM 25,2 %, flash 50,3 %). **Flash non fait** : COM3 avait disparu (`pio device list` vide : carte débranchée ou hors tension). Test natif à vérifier dans la CI (pas de gcc sur le PC).

### 19.10.2026 — Flash sur un autre poste (COM5) ; l'écran OLED bloquait le boot

**Demande** : lire les fichiers de reprise ; flasher (carte sur COM5) ; l'écran est branché mais « le boot s'arrête ».

**Fait** :
- Poste différent : `pio` dans le PATH, cœur PlatformIO dans `C:\DATA\platformio` (« Reprise rapide » mise à jour) ; `main` = `origin/main` = `a4eb329`.
- Flash de `a4eb329` (battement de cœur) : SUCCESS. Console : `bootLoopCount` 6 → **mode réduit** (servos non attachés, état `Manual`), écran absent de l'état.
- Journal de boot capturé par un petit script pyserial qui survit à la disparition du port pendant un reset : scan `0x3C 0x6A`, puis blocage dans `u8g2->begin()` ; après débranchement, l'écran ne répondait plus au scan. La broche EN de l'écran, en l'air, a été reliée au 3V3 par l'utilisateur : sans effet sur le blocage.
- Cause trouvée dans les sources U8g2 et corrigée dans `src/ui/Display.cpp` (voir « Erreurs ») ; décision du 05.10.2026 annotée.

**Vérifié sur la carte** : boot complet (`Display: OLED at 0x3C, type 0 (SSD1306)`, `Boot: done in 1693 ms`, Idle) ; l'écran affiche, avec le **défaut 4** (radar muet, attendu tant que le radar n'envoie rien). Compilation sans erreur. **Non essayés** : menus du mode debug (SW1 fermé), boutons A / B.

**Prochaine étape** : radar (J5, croisement TX / RX), puis menus du mode debug avec SW1 fermé.

### 19.10.2026 (2ᵉ entrée) — Alerte de défaut de 5 s sur l'écran

**Demande** : le connecteur du radar n'est pas encore soudé ; le mode debug fonctionne-t-il sans radar (oui : état `Manual`, le radar n'y sert pas) ; puis « la LED rouge doit clignoter pour le défaut, mais l'écran doit afficher le problème 5 secondes et passer à la suite ».

**Fait** : `logic::FaultName`, `logic::FaultAlert`, `logic::RenderFaultAlert` (`src/logic/MenuLogic.h`) ; `ui/DebugUi` (alerte prioritaire, réveil, fermeture par une touche ; ligne « Défauts » retirée de l'œil) ; test natif `test_fault_alert` (26 tests) ; voir « Décisions ».

**Vérifié sur la carte** (commande `screen`) : ~5 s après le boot `! Défaut` / `4 Radar muet` / `LED rouge : code 4`, ~12 s après l'œil `(-)` seul. Compilation sans erreur (RAM 25,2 %, flash 50,3 %). **Non exécuté** : `test_fault_alert` (CI). **Non vu** : l'alerte en mode debug par-dessus le menu, la fermeture par un bouton.

**Prochaine étape** : souder le connecteur du radar ; menus du mode debug avec SW1 fermé.

### 19.10.2026 (3ᵉ entrée) — Encodeur rotatif I²C

**Demande** : prise en charge d'un encodeur rotatif I²C utilisable aussi comme bouton ; choix : Adafruit 4991, rôle « menu + volume ». Puis le 4991 est en rupture : Soldered 333188 ou Adafruit 5752 ? Faut-il un LTC4311 ? Et un multiplexeur SparkFun TCA9548A (BOB-16784) pour « augmenter la puissance » ?

**Fait** : `sensors/RotaryEncoder.{h,cpp}` (trois modèles, voir « Décisions ») ; `logic/MenuLogic.h` (touches d'encodeur, `FromEncoder`, indications qui suivent la dernière commande, `RenderVolume`) ; `ui/DebugUi` (`ShowVolume`) ; `audio/Audio` (`SetVolume`, `GetVolume`) ; `control/Actions` (commande `volume` avec enregistrement différé, aussi fait avant un redémarrage ; `key cw|ccw|push|hold`) ; `main.cpp` (`HandleEncoder`) ; tests natifs `test_menu_encoder`, `test_volume_screen` (28 tests) ; README du firmware.

**Réponses données** : LTC4311 inutile (bus de quelques dizaines de cm, 3–4 modules ; il sert aux câbles longs) ; le point à surveiller est l'inverse : les tirages des modules en parallèle des 2,2 kΩ (≈ 1,2 kΩ avec écran + encodeur, ≈ 2,7 mA, juste sous les 3 mA de la norme) ; multiplexeur TCA9548A : voir la réponse de la session (non nécessaire pour l'instant).

**Vérifié** : compilation `turret2_bringup` sans erreur ni avertissement (RAM 25,3 %, flash 50,4 %) ; `check_web.py` : tout passe. **Non fait** : flash (carte débranchée) ; tests natifs (CI) ; aucun module reçu, donc rien vu sur le matériel. **À vérifier à la réception** : sens de rotation (seesaw : inversé d'après les exemples Adafruit ; Soldered : compteur supposé croissant en sens horaire), nombre de pas du compteur Soldered par cran (`SOLDERED_COUNTS_PER_STEP` = 1 supposé).

**Prochaine étape** : flasher, puis essayer l'encodeur reçu (`key cw` etc. permettent déjà de tester le menu sans lui).

### 19.10.2026 (4ᵉ entrée) — Remise à zéro par les boutons de la carte

**Demande** : pouvoir revenir aux réglages d'usine avec les boutons de la carte (PROG, RST, peut-être SW1) ; réponses : geste « SW1 fermé + PROG 5 s », sans écran (LED rouge qui clignote, puis verte une fois accepté), et tout effacer, statistiques comprises.

**Fait** : `logic::ResetGesture` (`src/logic/BoardLogic.h`) ; `pins.h` (`PIN_PROG`, règle sur IO0 précisée) ; `board/Board` (lecture de PROG et de SW1 en marche, LEDs, `TakeFactoryResetRequest`, `ClearCounters`) ; `control/Stats::Clear` ; commande `factory-reset` (`control/Actions`, enregistrement différé du volume annulé) ; `main.cpp` ; écran `logic::RenderFactoryReset` dans `ui/DebugUi` (prioritaire) ; tests natifs `test_reset_gesture`, `test_factory_reset_screen` (30 tests) ; README du firmware (§7, commandes).

**Vérifié** : compilation sans erreur ni avertissement (RAM 25,3 %, flash 50,5 %) ; `check_web.py` : tout passe. **Non fait** : flash (carte débranchée), tests natifs (CI). **À vérifier sur la carte** : que PROG se lit bien à l'état haut au repos (pull-up interne + éventuelle externe), les deux clignotements, l'effacement réel (`stats`, `get`, `reset-reason` après le redémarrage).

**Prochaine étape** : flasher et essayer le geste ; commit / push de l'encodeur et de la remise à zéro.

### 19.10.2026 (5ᵉ entrée) — Modules Qwiic : plan, touche capacitive ; synchronisation

**Demande** : d'autres modules Qwiic utiles (catalogue Berrybase) ; l'idée du MPR121 plaît (disponible : ADA1982, 7,55 CHF) ; où trouver du cuivre adhésif ; « on peut aussi faire des boutons ? » ; « mets-le dans le journal à faire et synch ».

**Fait** : catalogue **Q** (Q1–Q7) et lot **IQ** dans « Avancement » ; « Idées écartées » précisé ; « Prochaine action » complétée ; commit et push de tout le travail du jour non poussé (Q1, Q2, documentation). Stock vérifié sur berrybase.ch le 19.10.2026 : MPR121 ADA1982 et ruban PRT-13827 disponibles, TCA4307 disponible, capteurs de lumière et VL53L1X Qwiic en rupture.

**Non fait** : aucun essai sur la carte de Q1 / Q2 (débranchée) ; pilote MPR121 (à la réception).

**Prochaine étape** : rebrancher la carte, flasher, essayer Q2 puis Q1 ; Q3 à la réception du MPR121.

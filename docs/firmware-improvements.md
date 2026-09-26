# Améliorations et fonctionnalités — plan et journal de reprise

Créé le 26.09.2026. Ce fichier **est versionné** (contrairement à `docs/firmware-journal.md` et `CLAUDE.md`, qui sont locaux depuis le commit `210b05d`). Il contient à la fois le plan des améliorations qui suivent les lots 1 à 12 de [firmware-plan.md](firmware-plan.md) et leur journal d'avancement.

> Dates au format JJ.MM.AAAA. Tout le code concerné est dans `Turret_firmware/` (`Fork/` reste en lecture seule).

## Reprise rapide (VS Code)

1. `git checkout main && git pull`, ouvrir **`Turret_firmware/`** comme dossier dans VS Code (PlatformIO ne détecte `platformio.ini` qu'à la racine du dossier ouvert).
2. Lire « État actuel » ci-dessous, puis le premier lot du tableau « Avancement » qui n'est pas *fait*, et sa fiche dans « Catalogue ».
3. Relire les règles matérielles de [firmware-plan.md](firmware-plan.md) §2 et de [../Turret_firmware/README.md](../Turret_firmware/README.md) §13 avant de toucher au code (IO21 / IO47 jamais à l'état haut, ampli coupé avant tout arrêt de l'I²S, pas d'eFuse ESP32 brûlé…).
4. À la fin de chaque session : mettre à jour « État actuel », « Avancement », « Erreurs » et ajouter une entrée dans « Sessions », **dans le même commit** que le travail. Reporter aussi dans le journal local si on l'utilise.

## État actuel — 26.09.2026

- **Firmware** : lots 1 à 12 du plan faits (commit `7a20d46` sur `main`) ; compile (`turret2`, `turret2_bringup`, `turret2_ota_home`) ; **n'a jamais tourné sur une carte**. Page web testée seulement contre `Turret_firmware/tools/mock_server.py`.
- **Améliorations** : catalogue ci-dessous proposé le 26.09.2026 ; l'utilisateur a validé le démarrage (« Go »). **Aucune amélioration codée à ce jour.**
- **Prochaine action** : lot I1 (A1 + A2 + D4 + D2).
- **Matériel figé** (26.09.2026) : aucune modification de la carte ni du câblage, sauf « méga plus » (aucun identifié). Les mesures se font sans modification : cahier [experiments.md](experiments.md).
- **eFuse** : TPS259573 identifié en **auto-retry** (plan §2.5, D5) — cas déjà couvert par la détection de boucle de redémarrage ; reste à lire dans la datasheet le délai d'auto-retry pour vérifier que 3 cycles tiennent dans la fenêtre de 60 s du compteur.

## Avancement

| Lot | Contenu | Statut | Commit / remarque |
|---|---|---|---|
| I1 | A1 radar muet, A2 repos + zone de détection, D4 version git, D2 CI GitHub Actions | à faire | corrige un vrai bug avant le premier allumage |
| I2 | A3 journal de crash + coredump, A4 watchdog, D1 tests natifs | à faire | boîte noire pour la mise en service |
| I3 | B1 visée, B3 mode recherche, C1 vue radar | à faire | dépend de I1 (zone de détection) |
| I4 | B2 répliques vocales, C4 gestion des sons, B4 prise en main / renversement | à faire | |
| IL | L1–L5 : aides firmware pour les expériences (repères LED, balayages, motif NeoPixel, cycle de charge, PWR_FLT horodaté) | à faire | petit lot, peut passer avant ou avec I2 ; sert le cahier [experiments.md](experiments.md) |
| I5 | au choix : A5, B5–B8, C2, C3, C5, C6, D3 | à faire | à trier avec l'utilisateur |

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

Idées écartées : INA219 / INA226 en ligne (demande de couper un fil et, avec le shunt d'origine de 0,1 Ω, ajoute 0,3 V de chute à 3 A), modules Qwiic, modifications pour une carte v0.2 (pull-down sur IO14–16, mesure du 5 V par ADC) — gain trop faible pour un projet de loisir.

### L. Aides firmware pour les expériences

| # | Quoi | Effort |
|---|---|---|
| L1 | **Repères de boot** : réglage `LabMarkers` ; la LED verte (IO33) change d'état à chaque étape du boot et à chaque attachement de servo, pour aligner une courbe d'oscilloscope ou de PPK2 sur le code | S |
| L2 | **Balayages** : `sweep servo <n> <ms>`, `sweep tone <Hz début> <Hz fin> <ms>` | S |
| L3 | **Motif NeoPixel de test** : `led pattern bit`, une trame fixe avec un seul bit à 1, facile à lire à l'oscilloscope | S |
| L4 | **Cycle de charge** : `loadtest <écart ms>` attache et bouge tous les servos avec l'écart donné, puis les détache (expérience X4 sans redémarrer) | S |
| L5 | **Événements PWR_FLT horodatés** dans le log avec l'état en cours (servos attachés, LEDs, son) | S |

## Décisions

| Date | Sujet | Décision | Raison |
|---|---|---|---|
| 26.09.2026 | Suivi des améliorations | ce fichier versionné, plan + journal réunis | le journal principal et `CLAUDE.md` sont locaux (commit `210b05d`) ; une reprise sur un autre poste ou dans le cloud doit trouver l'état dans git |
| 26.09.2026 | Ordre | I1 → I2 → I3 → I4 → I5 | I1 corrige un bug réel et sécurise `main` (CI) avant la mise en service |
| 26.09.2026 | eFuse | TPS259573 = auto-retry | recherche web (pages produit TI) ; datasheet à confirmer |
| 26.09.2026 | Matériel | **figé** : pas de modification de carte ni de câblage ; ouvert seulement pour un « méga plus », aucun identifié ; INA, Qwiic et idées v0.2 retirés | projet de loisir ; la carte a déjà l'essentiel (eFuse avec FLT, étoile 5 V, buck-boost, AHCT, points de test) |
| 26.09.2026 | Apprentissage | cahier d'expériences [experiments.md](experiments.md) + lot IL d'aides firmware | l'utilisateur veut tester et mesurer pour apprendre (oscilloscope, PPK2 à venir) |

## Erreurs, impasses et pièges

| Date | Quoi | Conséquence / solution |
|---|---|---|
| 26.09.2026 | `www.ti.com` et `www.digchip.com` bloqués par le proxy du conteneur cloud (curl et WebFetch) | variante de l'eFuse obtenue par les extraits d'une recherche web seulement ; confirmer dans la datasheet depuis un poste normal |
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


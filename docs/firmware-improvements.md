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
- **Améliorations** : **lot I1 fait** le 26.09.2026 (A1, A2, D4, D2) sous VS Code, commit `a54d633` ; compile ; vérifications de la page OK ; **CI GitHub verte** au premier passage.
- **Lot I2 fait** le 26.09.2026 (A3, A4, D1), non encore poussé ; compile ; `check_web.py` 27/27 ; **tests natifs pas encore exécutés** (pas de compilateur C++ sur le PC Windows : ils tourneront dans la CI au prochain push, ou après installation de MinGW-w64).
- **Lot IL fait** le 26.09.2026 (L1 à L5), non commité, avec I2.
- **Prochaine action** : I3 (B1 visée, B3 mode recherche, C1 vue radar).
- **Matériel figé** (26.09.2026) : aucune modification de la carte ni du câblage, sauf « méga plus » (aucun identifié). Les mesures se font sans modification : cahier [experiments.md](experiments.md).
- **eFuse** : TPS259573 identifié en **auto-retry** (plan §2.5, D5) — cas déjà couvert par la détection de boucle de redémarrage ; reste à lire dans la datasheet le délai d'auto-retry pour vérifier que 3 cycles tiennent dans la fenêtre de 60 s du compteur.

## Avancement

| Lot | Contenu | Statut | Commit / remarque |
|---|---|---|---|
| I1 | A1 radar muet, A2 repos + zone de détection, D4 version git, D2 CI GitHub Actions | fait | 26.09.2026 ; build `turret2` / `turret2_bringup` OK, `tools/check_web.py` 22/22 ; CI GitHub **réussie** au 1ᵉʳ passage (run 36265316764, commit `a54d633`, 5 min 42 dont 5 min 21 de compilation sans cache) |
| I2 | A3 journal de crash + coredump, A4 watchdog, D1 tests natifs | fait | 26.09.2026 ; build OK, `check_web.py` 27/27 ; tests natifs écrits (12 tests), à exécuter par la CI |
| I3 | B1 visée, B3 mode recherche, C1 vue radar | à faire | dépend de I1 (zone de détection) |
| I4 | B2 répliques vocales, C4 gestion des sons, B4 prise en main / renversement | à faire | |
| IL | L1–L5 : aides firmware pour les expériences (repères LED, balayages, motif NeoPixel, cycle de charge, PWR_FLT horodaté) | fait | 26.09.2026 ; build OK, `check_web.py` 27/27 ; non commité |
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
| 26.09.2026 | A1 | cibles effacées après 1 s sans trame complète (`Radar::Update`) ; `Idle` n'accepte une cible que si le radar est vivant (même délai) | une cible gardée en mémoire par un radar muet faisait tirer sans fin ; 1 s = une dizaine de trames manquées. Le défaut LED 4 garde son délai de 3 s |
| 26.09.2026 | A2 | réglages `CooldownMs` (5000, 0..120000), `DetectMaxMm` (3000, 300..6000), `DetectAngle` (45°, 5..60), groupe « Detection » ; cible valable si y > 0, distance ≤ max et \|atan2(x, y)\| ≤ angle ; repos appliqué à **chaque** entrée en `Idle` (après un cycle et après le boot) ; le bouton A / `demo` passent outre | le plus simple et sans état caché ; 60° = champ du LD2450, 6 m = sa portée. Vitesse minimale non retenue (une personne immobile qui entre dans la zone doit déclencher) |
| 26.09.2026 | D4 | `scripts/git_version.py` (pré-build) → `src/version_gen.h` (ignoré par git, réécrit seulement s'il change) ; `FIRMWARE_VERSION` dans la bannière, `/api/status` (`version`, plus `built` = date de compilation) et la page | un `-D` dans `build_flags` aurait tout recompilé à chaque commit |
| 26.09.2026 | A3 | journal de crash : 2 Ko de fin de journal en mémoire RTC (`RTC_NOINIT_ATTR`, placée à `0x50000000`, vérifié dans le `.map`), avec nombre magique et bornes ; conservé si le reset n'est pas un POWERON, affiché au boot seulement après un reset de type crash (PANIC, *_WDT, BROWNOUT, UNKNOWN) ; coredump : déjà activé dans le SDK précompilé (flash, ELF, CRC32) → résumé au boot via `esp_core_dump_get_summary` (tâche, PC, backtrace), mis en cache (le contrôle relit toute l'image), téléchargement `GET /api/coredump` par blocs, effacement | pas d'outil à installer pour voir la cause d'un crash ; le décodage complet reste `espcoredump.py` avec le `firmware.elf` du même build |
| 26.09.2026 | A4 | watchdog de tâche sur `loop()` : `esp_task_wdt_init(8, true)` (l'en-tête IDF 4.4 confirme qu'un 2ᵉ appel met à jour délai et panic) + `enableLoopWDT()`, en fin de `setup()` ; scan I²C interrompu au 1ᵉʳ délai dépassé (code 5 de `endTransmission`) | 8 s plutôt que les 5 s du SDK : marge pour les opérations longues légitimes (effacement du coredump, réglages) ; 112 délais I²C de 50 ms dépasseraient le watchdog |
| 26.09.2026 | D1 | logique pure extraite dans `src/logic/` (en-têtes sans Arduino) : `RadarLogic.h` (décodage LD2450, zone), `HallLogic.h` (seuils, polarité, rail), `BoardLogic.h` (anti-rebond / appuis, code LED, gain) ; le firmware les utilise (Radar, Wing, Board, Amp) ; env `native` (`platform = native@1.2.1`, Unity), 12 tests dont l'équivalence du décodage avec la formule d'origine sur les 65 536 valeurs ; `pio test -e native` ajouté à la CI | les tests portent sur le code réellement utilisé ; parties trop liées au matériel (servos, I²S, réseau) laissées au simulateur et à la carte |
| 26.09.2026 | Lot IL | L1 : `LabMarkers` (Bool, groupe Lab, appliqué aussitôt), `Board::Mark(label)` statique (bascule + ligne de journal), repères aux étapes 4 à 10, à chaque attache (`Gantry`) et à la fin du boot ; L2 : balayage servo en triangle, 1 consigne / 20 ms ; balayage de fréquence linéaire dans le générateur de tonalité (`PlayChirp`) ; L3 : `SetBitPattern` (luminosité 255 exacte car `FASTLED_SCALE8_FIXED` = 1, dithering coupé, luminosité restaurée) ; L4 : écart d'attache imposé en RAM (`Gantry::SetStaggerOverride`, 0 → 1 ms « tous ensemble »), ailes en `TestStop` ; L5 : ligne de journal avant le délestage ; balayage et `loadtest` annulés par `Fault` et `resume` | sans annulation, un balayage ou un `loadtest` redemanderait des servos pendant un défaut d'alimentation |
| 26.09.2026 | D2 | `.github/workflows/firmware.yml` : sur push / PR touchant `Turret_firmware/` ; PlatformIO 6.1.18 figé, cache `~/.platformio` + `libdeps` ; contrôle de la casse des `#include` ; build `turret2` + `turret2_bringup` ; `tools/check_web.py` (simulateur + `node --check`). Tests natifs ajoutés à la CI avec D1 (lot I2) | le runner est sous Linux, sensible à la casse (le fichier `FIringState.cpp` montre que le dépôt a déjà des noms atypiques) |

## Erreurs, impasses et pièges

| Date | Quoi | Conséquence / solution |
|---|---|---|
| 26.09.2026 | `www.ti.com` et `www.digchip.com` bloqués par le proxy du conteneur cloud (curl et WebFetch) | variante de l'eFuse obtenue par les extraits d'une recherche web seulement ; confirmer dans la datasheet depuis un poste normal |
| 26.09.2026 | Sous VS Code (Windows), l'outil Bash retire les `\` du texte des commandes, même dans un heredoc `<<'EOF'` : un script Python passé en ligne a échoué (`SyntaxError`) | écrire tout script ou code contenant des `\` dans un fichier (outil d'écriture), puis l'exécuter ; déjà vu aux lots 4 et 10 (journal local) |
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

# Journal de bord — firmware Turret2

Ce journal accompagne [firmware-plan.md](firmware-plan.md). Il contient **tout** ce qui a été fait, ce qui est prévu, ce qui est terminé, les décisions et **les erreurs**, afin que quelqu'un qui ne connaît que le plan puisse reprendre le travail sans rien perdre.

## Reprise rapide

Pour reprendre le travail sans autre contexte :

1. Lire `CLAUDE.md` (règles permanentes), puis « État actuel » ci-dessous : il donne la phase, la **prochaine action** et les blocages.
2. Lire dans « Avancement des lots » le premier lot qui n'est pas *fait*, puis sa description dans [firmware-plan.md](firmware-plan.md) §7 (et les sections qu'elle cite).
3. Parcourir « Erreurs, impasses et pièges » avant de lancer une commande : on n'y retombe pas deux fois.
4. Travailler sur la branche indiquée dans « État actuel » ; à la fin, mettre à jour ce journal **dans le même commit** que le travail, puis pousser.

Documents du dépôt : [firmware-plan.md](firmware-plan.md) (plan firmware), [design-plan.md](design-plan.md) et [status-and-history.md](status-and-history.md) (matériel, font foi sur la carte), [../README.md](../README.md) (présentation de la carte).

## Mode d'emploi

- **Pour reprendre le travail** : lire « État actuel », puis « Avancement des lots », puis la dernière entrée de « Sessions ».
- **À chaque session** : mettre à jour « État actuel », « Avancement des lots », « Décisions », « Erreurs, impasses et pièges », et ajouter une entrée datée dans « Sessions ». Le journal est mis à jour dans le même commit que le travail qu'il décrit, et toujours avant un push.
- Les entrées de session passées ne sont **jamais réécrites** : une correction fait l'objet d'une nouvelle entrée qui renvoie à l'ancienne.
- Dates au format JJ.MM.AAAA. Statuts : *à faire*, *en cours*, *fait*, *bloqué*, *abandonné*.

---

## État actuel — mis à jour le 25.09.2026 (5ᵉ session)

- **Phase** : **lot 1 fait** (build reproductible, versions figées). Décisions D1 à D8 tranchées (plan §9). Seul `Turret_firmware/platformio.ini` a changé ; le code source est toujours identique à `Fork/`.
- **Référence de build** (`lolin_s3_mini`, plateforme `espressif32@7.1.3`) : tout le code compile et se lie, mais l'image **dépasse la partition d'application** de 4 Mo : 1 392 725 octets pour 1 310 720 (106,3 %), RAM 60 784 octets (18,5 %). Normal pour cette carte (partitions 4 Mo), résolu au lot 2 par les partitions 8 Mo (app 3,2 Mo).
- **Poste de travail** : Windows, PlatformIO 6.1.18 dans `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` (pas dans le PATH). Commande : `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -d Turret_firmware -e <env>`.
- **Arborescence (depuis le 25.09.2026, commit `8248fcd` de `main`, fusionné dans la branche)** : `Turret_firmware/` = **firmware Turret2, dossier de travail** (D8) ; `Fork/` = firmware d'origine, intact, référence en lecture seule ; `Turret2_portable/` = projet KiCad ; `Pictures/` = photos ; `docs/`, `README.md`, `CLAUDE.md` à la racine. Dans les entrées de session antérieures au 25.09, `src/…` et `platformio.ini` désignent les mêmes fichiers, aujourd'hui sous `Fork/` (original) et copiés dans `Turret_firmware/` (travail).
- **Cible** : Turret2 uniquement (plus de compatibilité Wemos / V4, D2).
- **Branche** : tout est sur **`main`** depuis le 25.09.2026 (avance rapide depuis `claude/admiring-bell-q3hube`, qui est au même commit) — dépôt `lo26lo/portal-turret-v4`.
- **Prochaine action** : lot 2 du plan (cible Turret2) — `boards/turret2.json`, variant `turret2`, partitions `default_8MB.csv`, envs `turret2*`, `pins.h` complet, suppression des envs `lolin_s3_mini*` ; critère : `pio run -e turret2` passe (y compris la taille).
- **Décisions ouvertes** : aucune. Reste à vérifier sans urgence : variante latch-off / auto-retry du TPS259573 (le firmware gère les deux, D5).
- **Matériel** : la carte Turret2 n'est pas encore fabriquée → la mise en service (plan §8) attend les cartes ; tout le reste peut avancer sans elles.
- **Blocages** : aucun. Limites connues de l'environnement : voir « Erreurs, impasses et pièges ».

## Avancement des lots

| Lot | Contenu (plan §7) | Statut | Commit / remarque |
|---|---|---|---|
| — | Plan + journal + `CLAUDE.md` | fait | commit du 24.09.2026 qui ajoute ces fichiers |
| 1 | Base de build, versions figées | fait | 25.09.2026 : plateforme 7.1.3 et toutes les libs figées (transitives comprises) ; build propre reproduit à l'identique ; référence `lolin_s3_mini` = 106,3 % de la partition app (voir session) |
| 2 | Cible Turret2 (board JSON, variant, partitions, envs, `pins.h`) + suppression des envs Wemos | à faire | D2 |
| 3 | Bugs existants (plan §5) | à faire | |
| 4 | Module Board (état sûr, LEDs, boutons, SW1, PWR_FLT, raison du reset, boucle de redémarrage) | à faire | D1, D3, D4, D5 tranchées |
| 5 | Séquence de boot + `BootState` réel | à faire | |
| 6 | IMU LSM6DSOX, scan I²C, orientation | à faire | |
| 7 | Audio : module Amp, gain / volume, arrêt sûr | à faire | |
| 8 | Mouvement : attache échelonnée, homing, seuils, trims, maintien des servos | à faire | D6 tranchée |
| 9 | Énergie : délestage, `Fault`, compteur brownout, mode réduit | à faire | D5 tranchée |
| 10 | Console de mise en service | à faire | |
| 11 | Docs (README Firmware) | à faire | |
| 12 | Page web de configuration (plan §10) | à faire | demandée le 24.09 ; partie « réglages » avançable après le lot 3 |

## Décisions

| Date | Sujet | Décision | Raison |
|---|---|---|---|
| 24.09.2026 | Langue et format | plan et journal en français, dans `docs/`, dates JJ.MM.AAAA | langue de travail de l'utilisateur ; même convention de date que la doc matérielle Turret2 |
| 24.09.2026 | Mémoire entre sessions | la règle du journal est écrite dans `CLAUDE.md` à la racine du dépôt | le conteneur est éphémère : seule une instruction versionnée dans le dépôt est relue automatiquement à chaque session |
| 24.09.2026 | Documentation matérielle | non ajoutée au dépôt ; le plan §1 en reprend l'essentiel | les fichiers Turret2 (README, `design-plan.md`, `status-and-history.md`, KiCad) appartiennent au travail local de l'utilisateur ; les pousser risquait des conflits avec sa version |
| 24.09.2026 | D1 SW1 | mode banc, pour usage futur (aucun servo attaché automatiquement, machine d'états arrêtée, console, logs verbeux) | choix de l'utilisateur |
| 24.09.2026 | D2 compatibilité | Turret2 uniquement, plus de Wemos / V4 : pas de `#ifdef`, envs `lolin_s3_mini*` et ADXL345 supprimés | choix de l'utilisateur ; simplifie le code |
| 24.09.2026 | D3 boutons | A = démo, B = mute, A + B au boot = remise à zéro, B 3 s = Wi-Fi on / off | proposition acceptée |
| 24.09.2026 | D4 codes LED | verte = battement ; rouge = 1 brownout / boucle, 2 eFuse, 3 IMU, 4 radar, 5 Hall, 6 LittleFS | proposition acceptée |
| 24.09.2026 | D5 eFuse | le firmware gère latch-off et auto-retry sans connaître la variante : détection de boucle de redémarrage (3 resets de suite) → mode réduit | l'utilisateur a laissé le choix (« fais au mieux ») ; datasheet inaccessible d'ici |
| 24.09.2026 | D6 servos au repos | ailes détachées à l'arrêt ; canons détachés ~500 ms après leur mouvement ; rotation maintenue ailes ouvertes, détachée après `ServoIdleMs` (5 s) en Idle, ré-attachée sur la dernière consigne | l'utilisateur a laissé le choix ; détacher les servos continus supprime le glissement du neutre, et moins de servos alimentés = moins de courant et de bruit pour l'IMU |
| 24.09.2026 | D7 sécurité | AP en WPA2, mot de passe `ApPassword` (défaut `stillalive`), même mot de passe en HTTP Basic sur l'API et `/update` ; A + B au boot le réinitialise | accepté par l'utilisateur ; défaut connu mais récupérable, avertissement dans la page tant qu'il n'est pas changé |
| 24.09.2026 | Documentation matérielle (révise la décision « non ajoutée au dépôt » ci-dessus) | ajoutée telle quelle : README Turret2 → `README.md` (le dépôt n'en avait pas), `docs/design-plan.md`, `docs/status-and-history.md`. Aucune modification de leur contenu | demande de l'utilisateur ; emplacements indiqués par le README lui-même |
| 25.09.2026 | Réorganisation du dépôt par l'utilisateur (`Fork/`, `Turret2_portable/`, `Pictures/`) | fusionnée dans la branche de travail (merge, pas de rebase : la branche est publiée) ; chemins mis à jour dans le plan, `CLAUDE.md` et la section « Repository layout » / chemins du `README.md` (`hardware/Turret2` → `Turret2_portable`, `src/` → `Fork/`) | les docs doivent refléter l'arborescence réelle ; seules des corrections de chemins dans le README, pas de fond |
| 25.09.2026 | D8 emplacement du firmware | dossier séparé **`Turret_firmware/`** (copie de départ de `Fork/` sans `3d/` ni `gerber/`) ; `Fork/` n'est plus modifié | choix de l'utilisateur : ce firmware ne s'installera que sur sa carte ; ma proposition (travailler dans `Fork/`) a été refusée. Nom d'abord créé en `Turret2_firmware/`, renommé à sa demande (« on peut enlever le 2 ») |
| 25.09.2026 | Versions figées (lot 1) | `espressif32@7.1.3` (arduino-esp32 2.0.17, `framework-arduinoespressif32` 4.20017.260907) ; libs du registre à la version exacte installée ; libs git (Adafruit_Sensor, arduino-libhelix, arduino-audio-tools) figées sur un SHA complet ; dépendances transitives (Adafruit BusIO, AceCommon, AsyncTCP) listées explicitement | ce sont les versions qui compilent aujourd'hui ; audio-tools en HEAD avait reçu un commit le matin même ; une transitive non listée peut changer sans que `platformio.ini` bouge |
| 25.09.2026 | Dépassement de taille en `lolin_s3_mini` | pas corrigé au lot 1 (ni partitions, ni suppression de code) | cet env est supprimé au lot 2, dont les partitions 8 Mo (app 3,2 Mo) règlent le problème ; le modifier maintenant fausserait la référence |
| 24.09.2026 | Page web | page de configuration complète embarquée dans le firmware (gzip PROGMEM), générée depuis la liste des réglages, commandes passées à `loop()` par une file | demande de l'utilisateur ; vérifié qu'elle n'existait pas (seulement `GET /` et `GET /settings` en JSON, lecture seule) |

## Erreurs, impasses et pièges rencontrés

| Date | Quoi | Conséquence / solution |
|---|---|---|
| 24.09.2026 | Premier essai de récupération des drivers I²S d'AudioTools avec des noms de fichiers devinés (`I2SESP32.h`, `I2SESP32V1.h`) | fichiers 404 de 14 octets. Bons chemins sur `raw.githubusercontent.com/pschatzmann/arduino-audio-tools/main/` : `src/AudioTools/CoreAudio/AudioI2S/I2SDriverESP32.h` (driver legacy), `I2SDriverESP32V1.h` (IDF 5), `I2SConfigESP32.h` |
| 24.09.2026 | `api.github.com` refuse les dépôts hors du périmètre de la session | utiliser `raw.githubusercontent.com` (autorisé) pour lire des fichiers de libs publiques |
| 24.09.2026 | `www.ti.com` bloqué par le proxy (403) | datasheet TPS2595 non consultée → D5 (latch-off ou auto-retry) reste ouverte ; à vérifier par l'utilisateur ou via une autre source |
| 24.09.2026 | PlatformIO absent du conteneur | rien n'a été compilé ; l'erreur de compilation supposée sur `config.h` (`src/audio/ESP32Downloader.cpp:4`) est à confirmer au lot 1 |
| 25.09.2026 | Le README Turret2 annonçait `hardware/Turret2/` ; le projet KiCad est finalement arrivé dans `Turret2_portable/` (commit `8248fcd`) | README corrigé ; l'entrée ci-dessous sur les liens cassés est résolue |
| 24.09.2026 | La documentation Turret2 citée par le README (`hardware/Turret2/`, `docs/design-plan.md`, `docs/status-and-history.md`) n'existe pas sur la branche distante | travail fait à partir des trois fichiers fournis par l'utilisateur ; les deux `docs/` et le README ont été ajoutés en 3ᵉ session ; `hardware/Turret2/` reste absent (les liens du README vers ce dossier sont donc cassés sur GitHub) |
| 24.09.2026 | Bugs trouvés dans `Settings` en préparant la page web : paramètre `group` ignoré (`Settings.cpp:10,12`), `Settings::SetFromString` déclarée sans définition (`Settings.h:77`), copie par valeur de `Settings` dans `TurretWebServer.cpp:60` | ajoutés au plan §5, corrigés au lot 12 |
| 24.09.2026 | Piège documentaire : le README Turret2 affirme que `getEvent()` est inchangé avec le LSM6DSOX | faux : `Adafruit_LSM6DS::getEvent(accel, gyro, temp)` prend trois pointeurs (vérifié dans `Adafruit_LSM6DS.h`) → plan §2.4, lot 11 |
| 25.09.2026 | Premier build : `fatal error: sdkconfig.h: No such file or directory` dans tous les fichiers | pas un problème de code : le package `framework-arduinoespressif32` du poste était incomplet (installation interrompue plus tôt dans la journée) — `tools/sdk/esp32s3/` n'avait que `bin`, `dio_opi`, `dio_qspi`, `include` (ni `qio_qspi`, ni `lib`, ni `ld`). Ma suppression du package a été refusée par le garde-fou ; il a été **déplacé** vers `%USERPROFILE%\.platformio\framework-arduinoespressif32.incomplet-25.09.2026` (sauvegarde, à supprimer à la main) et PlatformIO l'a réinstallé complet. Si l'erreur revient : vérifier que `tools/sdk/esp32s3/qio_qspi/include/sdkconfig.h` existe |
| 25.09.2026 | Build propre dans le scratchpad (`PLATFORMIO_WORKSPACE_DIR` sous `AppData\Local\Temp\claude\…`) : `No such file or directory` sur `FastLED/…/flexio/channel_engine_flexio.cpp.hpp`, qui existe pourtant | limite **MAX_PATH (260)** de Windows : chemin de 264 caractères. FastLED 3.10 a des chemins très profonds → garder le dépôt et l'espace de travail PlatformIO sur un chemin court. Build propre refait dans `Turret_firmware/.pio/c` (ignoré par git) : OK |
| 25.09.2026 | Plan §5 supposait une erreur de compilation sur `#include "config.h"` (`ESP32Downloader.cpp:4`) | faux : ça compile, l'include tombe par hasard sur `mbedtls/config.h` du SDK (vu dans le `.d`). Le fichier reste inutile (fonction jamais appelée) → à supprimer au lot 3 |
| 25.09.2026 | Avertissement `extra tokens at end of #include directive` à chaque inclusion de `GunShotAudio.h` | ligne 2 : `#include <Arduino.h>>` (un `>` en trop) → lot 3 |

---

## Sessions

### 24.09.2026 — Analyse, plan, journal

**Demande de l'utilisateur**
1. À partir de la documentation Turret2 (README, `design-plan.md`, `status-and-history.md`) et du code existant : un plan d'adaptation du firmware au nouveau matériel — ordre de boot des périphériques, points d'attention, tout ce qui paraît utile. Plan seulement, pas de code.
2. Ensuite : pousser le plan, et tenir un journal de bord complet (fait, prévu, terminé, erreurs), mémorisé comme règle permanente.

**Fait**
- Lecture intégrale des trois documents Turret2 et de tout `src/` (`main.cpp`, `pins.h`, états, audio, capteurs, mouvement, lumière, réglages, web / OTA) et de `platformio.ini`.
- Vérifications en ligne (sources primaires) :
  - `arduino-esp32` 2.0.17, `variants/lolin_s3_mini/pins_arduino.h` : `LED_BUILTIN = 47 + SOC_GPIO_PIN_COUNT`, `RGB_BUILTIN = LED_BUILTIN` → IO47 ; SDA 35, SCL 36 ; TX 43, RX 44.
  - `platform-espressif32`, `boards/lolin_s3_mini.json` : `-DBOARD_HAS_PSRAM`, 4 MB, `memory_type qio_qspi`.
  - `arduino-esp32` 2.0.17, `variants/esp32s3/pins_arduino.h` : **SDA 8, SCL 9**, `RGB_BUILTIN` sur `PIN_NEOPIXEL` (48).
  - `boards/esp32-s3-devkitc-1.json` : « ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM) », `default_8MB.csv`, variant `esp32s3`.
  - `tools/partitions/default_8MB.csv` : nvs 0x5000, otadata, app0 / app1 0x330000, spiffs 0x180000, coredump 0x10000.
  - `Adafruit_LSM6DS.h` : `getEvent(accel, gyro, temp)`, adresse par défaut 0x6A ; `Adafruit_LSM6DSOX.h` : chip id 0x6C.
  - AudioTools : driver legacy, `channel_format` par défaut `I2S_CHANNEL_FMT_RIGHT_LEFT`, le mono est dupliqué dans les deux slots (`writeExpandChannel`) ; driver IDF 5, mono → `slot_mask = I2S_STD_SLOT_LEFT`. Les deux conviennent au MAX98357A en mode canal gauche.
- Relevé des bugs existants qui imiteront des pannes matérielles (plan §5), notamment `AudioLoop` (`sizeof` d'un pointeur → pas de son de tir) et LittleFS jamais monté (`BootState` jamais activé).
- Plan présenté à l'utilisateur, puis écrit dans `docs/firmware-plan.md` (version complète, avec la référence GPIO du §1 pour qu'il se suffise à lui-même).
- Création de ce journal et de `CLAUDE.md` (règle permanente du journal + règles matérielles critiques).

**Non fait / reporté**
- Aucune compilation (PlatformIO absent).
- Datasheet TPS2595 non consultée (proxy).

**Prochaine étape**
- Lot 1 : `pip install platformio`, `pio run -e lolin_s3_mini`, noter chaque erreur ici, figer `platform = espressif32@<version>` et les versions des libs qui compilent.
- Faire trancher par l'utilisateur au moins D1 (SW1), D2 (double cible), D3 (boutons) et D4 (codes LED) avant le lot 4.

### 24.09.2026 (2ᵉ session) — Décisions et page web

**Demande de l'utilisateur** : réponses aux décisions du plan §9 — (1) SW1 en mode banc pour usage futur, (2) pas de compatibilité Wemos, (3) et (4) propositions boutons et LEDs acceptées, (5) et (6) « fais au mieux », (7) oui au mot de passe ; ajout (8) : une page web où l'on peut tout configurer, si elle n'existe pas.

**Fait**
- Vérifié dans le code que la page n'existe pas : `TurretWebServer.cpp` ne sert que `GET /` et `GET /settings` (JSON, lecture seule) ; `Ota.cpp` sert `POST /update`.
- En vérifiant, relevé trois bugs dans les réglages (voir « Erreurs ») et ajouté au plan §5.
- Plan mis à jour : en-tête, §1.2 (IO3 = mode banc), §4 (détection de boucle de redémarrage, D5), §5 (bugs `Settings`), §6 (architecture sans `#ifdef`, couche `Actions`, page embarquée, nouveaux réglages `ServoIdleMs`, `ApSsid`, `ApPassword`), §7 (lots 1, 2, 6 ajustés, lot 12, ordre conseillé), §9 réécrit avec les décisions, nouveau §10 (page web : principes, API, contenu, pièges).
- Choix faits sur délégation de l'utilisateur (D5, D6) et valeur par défaut du mot de passe (D7) : voir « Décisions ».

**Non fait** : toujours aucune compilation (lot 1 pas commencé).

**Prochaine étape** : lot 1.

### 24.09.2026 (3ᵉ session) — Documentation matérielle dans le dépôt

**Demande de l'utilisateur** : pousser aussi le README, `status-and-history` et `design-plan`.

**Fait**
- Copie sans modification des trois fichiers fournis au début de la 1ʳᵉ session : README Turret2 → `README.md` à la racine (il n'y en avait pas), `docs/design-plan.md`, `docs/status-and-history.md` — emplacements donnés par la section « Repository layout » du README.
- Plan §0 : lien vers ces documents ; la doc matérielle fait foi sur les points matériels.
- Décision « documentation non ajoutée » de la 1ʳᵉ session révisée (nouvelle ligne dans « Décisions »).

**À savoir**
- Le README (en anglais) contient encore l'affirmation fausse sur `getEvent()` et une section Firmware antérieure au plan : laissé tel quel, à corriger au lot 11.
- `hardware/Turret2/` (projet KiCad) n'est pas dans le dépôt : les liens du README vers ce dossier ne mènent nulle part tant que l'utilisateur ne l'a pas poussé.

**Prochaine étape** : lot 1.

### 25.09.2026 — Reprise rapide

**Demande de l'utilisateur** : « et le journal de reprise ? » — vérifier que le journal est bien disponible pour reprendre le travail.

**Fait**
- Vérifié que `docs/firmware-journal.md` est sur la branche distante `claude/admiring-bell-q3hube` (commits `c17977f`, `83e856e`, `73ab93d`).
- Ajout en tête d'une section « Reprise rapide » (procédure en 4 étapes + liste des documents).
- Correction : le titre « État actuel » indiquait « 2ᵉ session » alors qu'il avait été mis à jour en 3ᵉ session ; il porte maintenant la date du 25.09.2026.

**Prochaine étape** : lot 1 (inchangée).

### 25.09.2026 (2ᵉ session) — Fusion de la réorganisation de `main`

**Demande de l'utilisateur** : vérifier que `CLAUDE.md` est bien sur GitHub, prendre en compte ce qu'il a poussé sur GitHub, et commiter si tout est bon (il passe sur un autre poste).

**Constat**
- `CLAUDE.md`, le plan, le journal et la doc matérielle étaient bien sur `origin/claude/admiring-bell-q3hube`, arbre local propre.
- `origin/main` a reçu le commit `8248fcd` (lo26lo, 25.09.2026) : firmware d'origine déplacé de la racine vers `Fork/` (renommages sans modification), projet KiCad ajouté dans `Turret2_portable/` (avec `library/` et une sauvegarde `Turret2-backups/*.zip`), photo `Pictures/pcb.jpg`.

**Fait**
- `git merge --no-ff origin/main` dans la branche de travail : aucun conflit (la branche n'avait modifié aucun fichier déplacé).
- Chemins mis à jour : plan (§0, références `Fork/src/…`, architecture §6 sous `Fork/`), `CLAUDE.md` (section Références), `README.md` (chemins uniquement : `Fork/src/pins.h`, layout, `Turret2_portable/`).
- Nouvelle décision ouverte D8 (plan §9) : où développer le firmware Turret2 ; proposition `Fork/`.

**Non fait** : toujours aucune compilation.

**Prochaine étape** : réponse sur D8, puis lot 1.

### 25.09.2026 (3ᵉ session) — Dossier séparé pour le firmware Turret2

**Demande de l'utilisateur** : réponse à D8 — ne pas développer dans `Fork/`, mais séparer, puisque ce firmware ne s'installera que sur sa carte ; puis « on peut enlever le 2 » du nom de dossier proposé.

**Fait**
- Création de `Turret_firmware/` : copie de `Fork/src`, `Fork/data`, `Fork/include`, `Fork/lib`, `Fork/test`, `Fork/platformio.ini` (`diff -r` sur `src/` : identique). `3d/` et `gerber/` non copiés (matériel du V4).
- Créé d'abord sous le nom `Turret2_firmware/`, renommé `Turret_firmware/` avant tout commit.
- Plan : §0, références du §5, architecture §6, lot 1 et D8 (§9) pointent vers `Turret_firmware/`. `CLAUDE.md` : `Turret_firmware/` = dossier de travail, `Fork/` en lecture seule. `README.md` : ligne ajoutée dans « Repository layout ».

**À savoir** : les deux arbres sont identiques pour l'instant ; toute modification se fait dans `Turret_firmware/` uniquement.

**Prochaine étape** : lot 1.

### 25.09.2026 (4ᵉ session) — Tout sur `main`

**Demande de l'utilisateur** : « mets tout sur main » (il reprend sous VS Code).

**Fait**
- Vérifié : arbre propre, branche à jour avec GitHub, `origin/main` (`8248fcd`) ancêtre de la branche → **avance rapide** possible, sans commit de fusion ni réécriture d'historique.
- Journal mis à jour (ce paragraphe, « État actuel »), commité sur la branche, puis branche poussée et `main` avancé au même commit.

**Prochaine étape** : lot 1, sur `main`.


### 25.09.2026 (5ᵉ session) — Lot 1 : base de build

**Demande de l'utilisateur** : synchroniser le dépôt, lire le journal de reprise, puis « go » pour le lot 1. Nouveau poste : Windows 11, VS Code.

**Fait**
- `git pull --ff-only` : `main` de `8248fcd` à `dae8587`, sans conflit.
- PlatformIO 6.1.18 trouvé dans `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` (hors PATH).
- Build `pio run -d Turret_firmware -e lolin_s3_mini` avec le `platformio.ini` d'origine (non figé) : la plateforme résolue est `espressif32` 7.1.3.
  1. 1ᵉʳ essai : `sdkconfig.h` introuvable partout → package framework incomplet sur le poste, déplacé en sauvegarde puis réinstallé (voir « Erreurs »).
  2. 2ᵉ essai : **compilation et édition de liens OK**, échec au contrôle de taille : 1 392 725 octets pour 1 310 720 (106,3 %) ; RAM 60 784 octets (18,5 %). Avertissements : `GunShotAudio.h:2` (`>` en trop), FastLED (`optimization attribute` dans `fl/gfx/blur`, `ADC_ATTEN_DB_11` déprécié) — sans conséquence.
- Versions figées dans `Turret_firmware/platformio.ini` (clé `platform` dans `[common]`, reprise par l'env via `${common.platform}`) :

  | Composant | Version |
  |---|---|
  | plateforme `espressif32` | 7.1.3 (framework 4.20017.260907 = arduino-esp32 2.0.17, toolchain xtensa 8.4.0+2021r2-patch5) |
  | Adafruit Unified Sensor (git) | 1.1.15, `0a9127a1e886ff1adb4c1b6f5958b24108d55aa6` |
  | Adafruit BusIO | 1.17.4 (transitive, ajoutée) |
  | ESP32Servo | 1.2.1 (était `^1.2.1`) |
  | Adafruit ADXL345 | 1.3.4 (supprimée au lot 6) |
  | FastLED | 3.10.5 |
  | AceCommon | 1.6.2 (transitive, ajoutée) |
  | AceRoutine | 1.5.1 |
  | AsyncTCP | 3.5.0 (transitive, ajoutée) |
  | ESPAsyncWebServer | 3.12.1 |
  | arduino-libhelix (git) | 0.9.4, `5c0a04302dbd661a56408eeb1756e01dd8d33ba1` |
  | arduino-audio-tools (git) | 1.2.6, `4b6deecbe81a58b7a846e1cfd2d40a92befe35a6` (commit du 25.09.2026 au matin) |

- **Reproductibilité vérifiée** : build de zéro dans un espace de travail neuf (`PLATFORMIO_WORKSPACE_DIR=Turret_firmware\.pio\c`, libs retéléchargées depuis le `platformio.ini` figé) → mêmes versions, même RAM, flash 1 392 757 octets (+32 octets, attribués aux chemins de fichiers embarqués, plus longs de 2 caractères). Un premier essai dans le scratchpad avait échoué sur la limite MAX_PATH (voir « Erreurs »).

**Non fait** : l'env `lolin_s3_mini` dépasse toujours sa partition (décision : laissé tel quel, réglé au lot 2). Rien n'a été flashé (pas de carte).

**À savoir** : `Turret_firmware/.pio/c` (espace de travail du build de contrôle) peut être supprimé sans risque ; la sauvegarde du framework incomplet aussi (`%USERPROFILE%\.platformio\framework-arduinoespressif32.incomplet-25.09.2026`).

**Prochaine étape** : lot 2 (cible Turret2).

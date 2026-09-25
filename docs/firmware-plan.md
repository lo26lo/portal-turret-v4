# Plan d'adaptation du firmware à la carte Turret2

Rédigé le 24.09.2026. **Mis à jour le 24.09.2026 (2ᵉ session)** : décisions D1 à D7 tranchées (§9), **plus de compatibilité Wemos / V4** (Turret2 uniquement), page web de configuration ajoutée (§10, lot 12). Le suivi de l'exécution (ce qui est fait, en cours, les erreurs) est dans le journal de bord `docs/firmware-journal.md`, **local (non versionné)** : le lire avant de reprendre le travail.

> Dates au format JJ.MM.AAAA. Les références `fichier:ligne` pointent sur le code tel qu'il était au commit `020a839` (fichiers identiques, déplacés dans `Fork/` par le commit `8248fcd`, puis copiés dans `Turret_firmware/` : les numéros de ligne valent pour les deux tant que la copie n'a pas été modifiée) (firmware upstream de joranderaaff, avant toute modification).

---

## 0. Contexte et sources

- **Firmware** : le firmware d'origine est dans `Fork/` (déplacé depuis la racine le 25.09.2026, commit `8248fcd` de `main`) et **n'est plus modifié** : il sert de référence. Le firmware Turret2 est développé dans **`Turret_firmware/`** (D8), créé le 25.09.2026 comme copie à l'identique du projet PlatformIO de `Fork/` (`src/`, `data/`, `include/`, `lib/`, `test/`, `platformio.ini` ; sans `3d/` ni `gerber/`, propres au V4). Code d'origine dans `Fork/src/` (upstream [joranderaaff/portal-turret-v4](https://github.com/joranderaaff/portal-turret-v4)), écrit pour un Wemos LOLIN S3 mini câblé à la main (« V4 »).
- **Nouvelle carte** : Turret2, carte 4 couches à ESP32-S3-MINI-1-N8 conçue par lo26lo. Sa documentation est dans le dépôt depuis le 24.09.2026 : [README.md](../README.md), [design-plan.md](design-plan.md) (justification de chaque choix matériel) et [status-and-history.md](status-and-history.md) (état de la carte, historique, pièges KiCad). Le projet KiCad est dans `Turret2_portable/` (ajouté le 25.09.2026), les photos dans `Pictures/`. Ce plan reprend tout ce dont le firmware a besoin (§1) et se suffit à lui-même ; en cas de doute sur un point matériel, la doc matérielle fait foi.
- Le pinout de `Turret_firmware/src/pins.h` est **conservé à l'identique** par la carte. Le travail n'est donc pas un remappage, mais :
  1. une **définition de carte** PlatformIO correcte (celle d'aujourd'hui, `lolin_s3_mini`, est dangereuse sur Turret2) ;
  2. le **pilotage des nouveaux signaux** (ampli SD/gain, PWR_FLT, LEDs, boutons, SW1) ;
  3. une **séquence de boot ordonnée** (aujourd'hui tout démarre en vrac) ;
  4. le **portage de l'IMU** (ADXL345 → LSM6DSOX) ;
  5. la correction de **bugs existants qui ressembleront à des pannes matérielles** pendant la mise en service.

---

## 1. Référence matérielle Turret2 (ce que le firmware doit savoir)

### 1.1 Ce qui change par rapport au V4

| Bloc | Wemos S3 mini (V4) | Turret2 | Impact firmware |
|---|---|---|---|
| Module | ESP32-S3FH4R2 : 4 Mo flash, 2 Mo PSRAM | **ESP32-S3-MINI-1-N8** : 8 Mo flash quad, **pas de PSRAM** | nouvelle carte PlatformIO, partitions 8 Mo |
| IMU | ADXL345 | **LSM6DSOX**, I²C 0x6A, WHO_AM_I 0x6C, + gyroscope, INT1 sur IO37 | portage `Motion` |
| Ampli | module MAX98357A, SD/gain câblés en dur | MAX98357A sur 5 V, **SD_MODE sur IO13** (via 2 kΩ), **gain sur IO21 / IO47** | séquence mute/unmute, gain logiciel |
| Alim | 5 V direct | USB-C 5 V → **eFuse TPS259573** (limite 3,86 A, coupure > 6,0 V, soft-start, **FLT sur IO38**) → +5 V en étoile (branche servos / branche logique) ; 3V3 par buck-boost TPS631000 alimenté depuis le +5 V | surveillance défaut, délestage |
| NeoPixels | données en 3,3 V direct (scintillement) | **SN74AHCT125** en 5 V sur les trois lignes | rien ; le scintillement doit disparaître |
| Hall | alimentés en 5 V (bug du V4) | alimentés en **3,3 V** (J17 / J18) | seuils ADC à recalibrer |
| Nouveaux | — | LED verte IO33, LED rouge IO48, boutons IO26 / IO34, SW1 sur IO3, UART0 de secours (J16), port Qwiic (J11), USB déporté (J1) | nouveaux modules |

### 1.2 Table des GPIO

| GPIO | Fonction | Remarque |
|---|---|---|
| IO0 | BOOT (SW2 à la masse) | strap — ne pas utiliser |
| IO1 / IO2 | servo canon gauche / droit | *pins.h* |
| IO3 | SW1 (interrupteur DIP à la masse, pull-up 10 k R9) | strap JTAG_SEL, **sans effet tant que l'eFuse `STRAP_JTAG_SEL` n'est pas brûlé** → lisible comme GPIO. Rôle : **mode banc** (D1, §9) |
| IO4 / IO5 | servo aile gauche / droite (servos à rotation continue : 90 = arrêt) | *pins.h* |
| IO6 / IO7 | servo rotation X / Z (positionnels) | *pins.h* |
| IO8 / IO9 | Hall gauche / droit, ADC1 | *pins.h* — capteurs en 3,3 V |
| IO10 / IO11 / IO12 | I²S DIN / BCLK / LRCLK | *pins.h* |
| IO13 | AMP_SD (SD_MODE via 2 kΩ, pull-down interne 100 k dans l'ampli) | bas = shutdown ; haut = canal gauche |
| IO14 / IO15 / IO16 | NeoPixel anneau (9 LEDs) / canon gauche (2) / canon droit (2), via AHCT125 | *pins.h* |
| IO17 / IO18 | radar HLK-LD2450 UART1 RX / TX, 256000 bauds | *pins.h* |
| IO19 / IO20 | USB natif D− / D+ | ne jamais toucher |
| IO21 | AMP_GAIN (GAIN_SLOT direct) | **jamais à l'état haut** |
| IO26 | bouton A (J12), pull-up 10 k + 100 nF, 1 kΩ série, actif bas | libre uniquement parce que le module est un N8 (sur N4R2 c'est la PSRAM) |
| IO33 | LED verte (470 Ω) | |
| IO34 | bouton B (J13), même montage que A | |
| IO35 / IO36 | I²C SDA / SCL, pull-ups 2,2 k près de l'IMU | *pins.h* — partagé avec le Qwiic |
| IO37 | IMU INT1 | réserve |
| IO38 | PWR_FLT, drain ouvert de l'eFuse, pull-up 10 k vers 3V3 | **bas = défaut** ; entrée uniquement |
| IO39–IO42 | non connectés | (ancien JTAG) |
| IO43 / IO44 | UART0 TXD0 / RXD0 sur J16 | logs de secours, flash de secours |
| IO45 / IO46 | straps, non connectés | ne pas utiliser |
| IO47 | AMP_GAIN_100K (GAIN_SLOT via 100 kΩ) | **jamais à l'état haut** |
| IO48 | LED rouge (470 Ω) | |

Gain de l'ampli selon IO21 / IO47 :

| IO21 | IO47 | Gain |
|---|---|---|
| haute impédance | haute impédance | 9 dB (défaut au reset) |
| bas | haute impédance | 12 dB |
| haute impédance | bas | 15 dB |

---

## 2. Pièges critiques — à lire avant de coder

### 2.1 Définition de carte

- **Ne pas garder `board = lolin_s3_mini`.** Elle déclare 4 Mo, ajoute `-DBOARD_HAS_PSRAM`, et son variant (`variants/lolin_s3_mini/pins_arduino.h` d'arduino-esp32) place `LED_BUILTIN` / `RGB_BUILTIN` sur **IO47 = AMP_GAIN_100K**. Le moindre `digitalWrite(LED_BUILTIN, HIGH)` ou `rgbLedWrite(RGB_BUILTIN, …)`, dans une lib ou un exemple, pousse IO47 à 3,3 V : interdit.
- **Ne pas prendre non plus le variant générique `esp32s3`** (celui d'`esp32-s3-devkitc-1`, pourtant « N8, 8 MB QD, No PSRAM ») : il définit **SDA = 8 et SCL = 9, soit les deux entrées Hall**. Tout `Wire.begin()` sans argument — ce que fait implicitement `accel.begin()` aujourd'hui (`Turret_firmware/src/sensors/Motion.cpp:6`) — mettrait l'I²C sur IO8 / IO9. Son `RGB_BUILTIN` est sur IO48 (LED rouge : sans danger, mais trompeur).
- **Solution retenue** :
  - `boards/turret2.json` : copie de `esp32-s3-devkitc-1.json` (8 Mo, `flash_mode qio`, `memory_type qio_qspi`, partitions `default_8MB.csv`), **sans `BOARD_HAS_PSRAM`**, variant `turret2` ;
  - `variants/turret2/pins_arduino.h` : TX 43, RX 44, SDA 35, SCL 36, `LED_BUILTIN` 33, **pas de `RGB_BUILTIN`** ;
  - et malgré tout **`Wire.begin(PIN_SDA, PIN_SCL)` explicite** partout.
- **Pas de PSRAM activée** : en mode quad, le driver PSRAM configure IO26 en SPICS1 → conflit avec le bouton A.
- **Jamais `opi` en `memory_type`** : le mode octal prend IO33 à IO37 (LED verte, bouton B, I²C, INT1) et le module ne démarre pas.
- Au premier flash, **prouver que c'est un N8** : `esptool.py flash_id` doit annoncer 8 MB ; au boot, `ESP.getFlashChipSize() == 8 Mo` et `ESP.getPsramSize() == 0`, sinon code d'erreur LED.

### 2.2 Broches

| GPIO | Règle |
|---|---|
| IO21, IO47 | configurer en **`OUTPUT_OPEN_DRAIN`** : '1' = haute impédance, '0' = masse. Physiquement incapables de tirer à 3,3 V — plus sûr que de basculer entre `INPUT` et `OUTPUT` |
| IO19 / IO20 | USB natif, ne jamais reconfigurer |
| IO0, IO45, IO46 | straps, ne pas utiliser |
| IO3 | lisible comme GPIO tant que `STRAP_JTAG_SEL` n'est pas brûlé |
| IO38 | entrée seulement (pull-up externe) |
| IO8 / IO9 | ADC1 → compatibles avec le Wi-Fi ; ne jamais déplacer les Hall sur l'ADC2 |

**Aucun eFuse de l'ESP32 à brûler, jamais** (`STRAP_JTAG_SEL`, `DIS_USB_JTAG`, `DIS_PAD_JTAG`, `VDD_SPI_*`…). `espefuse.py summary` uniquement, en lecture seule.

### 2.3 Ampli MAX98357A

- **Ne jamais arrêter LRCLK pendant que BCLK tourne** (tension continue en sortie, haut-parleur grillé). Mettre SD à l'état bas **avant** tout `i2s.end()`, tout changement de fréquence d'échantillonnage, tout `ESP.restart()` (y compris celui de l'OTA, `Turret_firmware/src/web/Ota.cpp`) et tout passage en défaut.
- Le gain n'est lu qu'à la sortie du shutdown : SD bas → réglage du gain → 10 ms → SD haut.
- Fréquences LRCLK autorisées : 8, 16, 32, 44,1, 48, 88,2, 96 kHz ; BCLK = 32, 48 ou 64 × LRCLK. 16 bits stéréo = 32 × fs ✓.
- Mono : avec SD_MODE à ~3,2 V (3,3 V à travers 2 kΩ contre le pull-down de 100 k), l'ampli joue le **canal gauche**. AudioTools, avec `channels = 1`, duplique l'échantillon dans les deux slots (driver legacy, `channel_format` par défaut `I2S_CHANNEL_FMT_RIGHT_LEFT`) ou le place dans le slot gauche (driver IDF 5). Dans les deux cas c'est bon — à confirmer à l'écoute. Ne pas passer en mode (L+R)/2.

### 2.4 IMU LSM6DSOX

- `Adafruit_LSM6DS::getEvent()` prend **trois** pointeurs (accéléro, gyro, température). Le README Turret2 affirme que « `getEvent()` is unchanged » : c'est faux, à corriger. Utiliser `getAccelerometerSensor()->getEvent(&e)` ou la version à trois arguments.
- La carte est montée **verticalement** : les axes ne correspondent plus à ceux de l'ADXL345 du V4 → mapping d'axes calibré (« tourelle debout ») et stocké en NVS.
- L'audio fort et les vibrations des servos provoquent de faux déclenchements : filtrer ou ignorer la détection de mouvement pendant les tirs et les déplacements.
- L'adresse 0x6A est prise sur le bus Qwiic partagé.

### 2.5 Alimentation

- L'ESP32 est alimenté **derrière** l'eFuse (3V3 tiré du +5 V). Si l'eFuse coupe complètement, l'ESP s'éteint aussi. PWR_FLT n'est donc lisible que pendant un affaissement du 5 V (limitation de courant) tant que le buck-boost tient le 3V3. D'où deux mécanismes complémentaires :
  - **interruption sur IO38** (front descendant) → délestage immédiat ;
  - **`esp_reset_reason()` au boot** : `ESP_RST_BROWNOUT`, ou un `ESP_RST_POWERON` inattendu, signale que l'alim a lâché.
- **À vérifier dans la datasheet TI** : TPS259573 en **latch-off ou auto-retry** ? En latch-off, après un défaut la carte reste éteinte jusqu'au débranchement USB ; en auto-retry, on peut voir une boucle de redémarrages (le compteur de brownouts la révélera).
- Sur un port USB de PC (0,5 à 0,9 A), **ne pas brancher les servos** → un mode banc qui n'attache aucun servo est utile (§9, SW1).
- Ne jamais brancher l'USB-C de la carte et l'USB déporté (J1) sur deux hôtes en même temps.

---

## 3. Ordre de boot

### 3.1 Phase 0 — matériel, avant le firmware (≈ 0 à 300 ms)

| t | Ce qui se passe | État des périphériques |
|---|---|---|
| 0 | USB branché ; soft-start de l'eFuse (dVdt 47 nF ≈ 0,9 V/ms, ~1,1 A d'appel) → +5 V en ~6 ms | servos, radar, ampli, LEDs et AHCT alimentés **en même temps** (plus de load switch) |
| ~6 ms | le TPS631000 démarre → 3V3 ; RC sur EN (10 kΩ / 1 µF) | Hall et IMU alimentés |
| ~10–15 ms | reset libéré, straps lus : IO0 = 1 → boot flash ; IO3 ignoré ; IO45 / IO46 = 0 | — |
| → ~300 ms | ROM puis bootloader ; **tous les GPIO en haute impédance** | ampli muet (pull-down interne de SD) ✓ ; gain flottant = 9 dB ✓ ; servos sans impulsion (tressautement possible) ; **entrées de l'AHCT flottantes → LEDs possiblement aléatoires** (vérifier s'il y a des pull-down sur IO14–16) ; le radar démarre |

Conséquence : le firmware doit reprendre la main sur les sorties **dans les premières millisecondes de `setup()`**, avant le `delay(1000)` actuel (`Turret_firmware/src/main.cpp:28`).

### 3.2 Phases firmware

| # | Étape | Pourquoi à cet endroit |
|---|---|---|
| 1 | **État sûr (< 5 ms)** : IO13 = `OUTPUT` bas (mute explicite) ; IO21 / IO47 en open-drain relâchés (9 dB) ; LEDs verte + rouge allumées (« boot » + test des LEDs) ; IO38, IO26, IO34, IO3 en entrée ; lecture de SW1, des boutons et de la raison du reset | fixe tout ce qui flotte ; SW1 et les boutons au boot choisissent le mode (normal / banc / remise à zéro) |
| 2 | **NeoPixels** : `FastLED.addLeds` + trame noire + plafond de puissance | efface les LEDs aléatoires et retire jusqu'à ~0,8 A du 5 V avant les servos |
| 3 | **Console** : `Serial` (USB CDC), miroir optionnel sur UART0 ; attente de l'hôte uniquement en build debug ; bannière : version, raison du reset, SW1, PWR_FLT, taille flash / PSRAM | diagnostics dès le début ; le `delay(1000)` fixe disparaît |
| 4 | **Settings (NVS)**, puis **LittleFS** (monté ici, sans formatage automatique silencieux) | tout le reste en dépend : seuils Hall, gain, volume, luminosité, offsets |
| 5 | **I²C** : `Wire.begin(35, 36, 400000)` + récupération de bus (9 coups d'horloge si SDA reste bas) + scan journalisé | l'IMU peut bloquer SDA après un reset à chaud en pleine transaction ; le scan montre aussi ce qui est branché sur le Qwiic |
| 6 | **IMU** : WHO_AM_I = 0x6C, plages (±4 g, 104 Hz), contrôle de gravité | refuser de déployer les ailes si la tourelle est couchée |
| 7 | **Radar** : `Serial1.begin(256000, SERIAL_8N1, 17, 18)` ; drapeau « radar vivant » à la première trame, alerte après ~3 s | non bloquant ; il démarre de son côté depuis la phase 0 |
| 8 | **Hall** : atténuation 11 dB (défaut), première lecture → état initial des ailes (fermées / ouvertes / inconnu) | aujourd'hui `isOpen = false` d'office, même si les ailes sont ouvertes au boot (reset en plein tir) |
| 9 | **Audio** : I²S démarré (horloges actives, DMA à zéro), gain réglé avec SD bas, 10 ms, **puis SD haut** | horloges stables avant l'unmute → pas de « plop » |
| 10 | **Wi-Fi AP + serveur web + OTA** | la calibration RF tire un pic de courant : on le place **avant** les servos pour ne pas cumuler les pics |
| 11 | **Servos un par un**, ~250 ms d'écart, PWR_FLT vérifié entre chaque : rotation Z → rotation X (au centre) → canon G → canon D (rentrés) → ailes (neutre = arrêt) | quatre servos positionnels qui sautent ensemble à leur consigne tirent 3 à 4 A, soit la limite de l'eFuse |
| 12 | **Homing** : si le Hall indique « pas fermé » → canons rentrés (déjà fait) → fermeture des ailes avec timeout | la mécanique repart d'un état connu |
| 13 | Fin de `BootState` : LED rouge éteinte (ou code de défaut), verte en battement → `Idle` | `BootState` devient réel : il n'est jamais activé aujourd'hui (`Turret_firmware/src/main.cpp:48` passe directement en `Idle`) |

Si PWR_FLT passe bas pendant les étapes 11 ou 12 → arrêt de la séquence, état `Fault`.

### 3.3 Séquence d'arrêt (OTA, reboot, défaut)

1. Ailes à l'arrêt, servos détachés.
2. SD à l'état bas (mute).
3. LEDs éteintes.
4. `i2s.end()`.
5. `ESP.restart()`.

À ajouter dans `Ota::Update` avant le `ESP.restart()`.

---

## 4. Gestion d'énergie en fonctionnement

- Au plus un démarrage de servo par fenêtre d'environ 150 ms (aile gauche puis aile droite, canons décalés).
- `FastLED.setMaxPowerInVoltsAndMilliamps(5, …)` : les 13 LEDs en blanc consomment ~0,8 A.
- Gain par défaut 9 dB, volume logiciel en réglage ; l'ampli peut tirer ~1,3 A crête pendant que les canons bougent.
- PWR_FLT bas (interruption) → délestage immédiat : ailes à l'arrêt, canons détachés, LEDs éteintes, ampli muet, LED rouge, état `Fault` ; reprise après 2 s de FLT haut.
- Raison du reset BROWNOUT → compteur en NVS, affiché sur la console et sur la page web (§10).
- **Détection de boucle de redémarrage (D5)** : un compteur en NVS est incrémenté au boot et remis à zéro après 60 s de fonctionnement stable. À partir de 3 redémarrages de suite (brownout, POWERON inattendu, panic, watchdog), la carte démarre en **mode réduit** : servos non attachés, LEDs à 10 %, gain 9 dB, LED rouge « 1 clignotement ». Cela couvre le cas d'un eFuse en auto-retry. En latch-off, la carte reste éteinte jusqu'au débranchement : le firmware n'y peut rien, mais le compteur de brownouts le montrera au redémarrage.
- **Maintien des servos (D6)** : voir §9.

---

## 5. Bugs existants à corriger en premier

Ils vont se manifester pendant la mise en service et ressembler à des pannes de la carte.

| Où (commit `020a839`) | Problème | Ce qu'on verra sur la carte |
|---|---|---|
| `Turret_firmware/src/audio/AudioLoop.cpp:8` | `sizeof(samplesIn)` = taille d'un pointeur → `totalSampleCount = 2` → la lecture s'arrête après 2 échantillons | **aucun son de tir** → on accusera l'ampli ou SD_MODE |
| `Turret_firmware/src/audio/Audio.cpp:32-33` | `availableForWrite()` peut dépasser les 4096 octets de `sampleBuffer` | débordement mémoire, crashs aléatoires |
| `Turret_firmware/src/states/BootState.cpp:8-11` | LittleFS n'est monté que dans `BootState`, jamais activé ; formatage automatique si le montage échoue | `fire.mp3` introuvable, ou fichiers effacés silencieusement |
| `Turret_firmware/src/audio/ESP32Downloader.cpp:4` | inclut `config.h`, absent du dépôt ; `WiFiClientSecure` sans son include ; fonction jamais appelée | probable erreur de compilation → exclure ou supprimer |
| `Turret_firmware/src/sensors/Motion.cpp:6-15` | échec de `begin()` ignoré, puis lecture I²C à chaque tour de boucle | boucle ralentie → audio haché, servos saccadés |
| `Turret_firmware/src/states/IdleState.cpp:14-15` | trois prints par tour de boucle | USB CDC saturé, boucle ralentie quand un terminal est ouvert |
| `Turret_firmware/src/motion/Wing.cpp:71,79` | seuils Hall 2500 / 1500 codés en dur | ailes arrêtées seulement au timeout de 2 s |
| `Turret_firmware/src/motion/Wing.cpp` (`write(90)`) | avec la plage 500–2400 µs, `write(90)` = 1450 µs, pas 1500 | les ailes (servos continus) peuvent glisser → trim de neutre en réglage |
| `Turret_firmware/platformio.ini` | plateforme `espressif32` et libs non figées (FastLED, ESPAsyncWebServer, audio-tools en HEAD git) | build qui casse sans prévenir ; AudioTools change de driver I²S selon la version d'IDF |
| `Turret_firmware/src/settings/Settings.cpp:10,12` | le paramètre `group` des constructeurs est ignoré (pas de membre `group` dans `SettingsEntry`) | impossible de regrouper les réglages dans la page web (§10) |
| `Turret_firmware/src/settings/Settings.h:77` | `Settings::SetFromString` déclarée mais jamais définie | erreur d'édition de liens dès qu'on l'appelle |
| `Turret_firmware/src/web/TurretWebServer.cpp:60` | la copie locale `SetFromString(Settings settings, …)` prend `Settings` **par valeur** : la modification s'applique à une copie | réglage « enregistré » mais sans effet jusqu'au reboot ; à supprimer au profit de `Settings::SetFromString` |

---

## 6. Architecture proposée

```
Turret_firmware/boards/turret2.json                  carte PlatformIO (8 Mo, QIO, sans PSRAM)
Turret_firmware/variants/turret2/pins_arduino.h      SDA 35 / SCL 36 / TX 43 / RX 44 / LED_BUILTIN 33
Turret_firmware/src/pins.h                           pinout Turret2 complet (broches upstream + IO13, 21, 47, 38, 33, 48, 26, 34, 3, 37), sans #ifdef
Turret_firmware/src/board/Board.{h,cpp}              état sûr, raison du reset, détection de boucle de redémarrage, LEDs d'état, boutons (anti-rebond, appui long), SW1, surveillance PWR_FLT
Turret_firmware/src/audio/Amp.{h,cpp}                SD + gain en open-drain, séquences mute/unmute, SafeShutdown()
Turret_firmware/src/sensors/Motion.*                 LSM6DSOX uniquement (ADXL345 supprimé)
Turret_firmware/src/motion/Gantry.*                  attache échelonnée, homing, trims, politique de maintien des servos (D6)
Turret_firmware/src/states/BootState, FaultState     boot réel + état de défaut
Turret_firmware/src/control/Actions.{h,cpp}          actions de test communes à la console série et à la page web (servo, LED, tonalité, gain…)
Turret_firmware/src/web/TurretWebServer.*            API JSON + authentification (§10)
Turret_firmware/src/web/page/index.html              page de configuration, compilée dans le firmware (gzip, PROGMEM)
```

`Turret_firmware/platformio.ini` : `env:turret2`, `env:turret2_ota`, `env:turret2_bringup` (`CORE_DEBUG_LEVEL=3` + console). **Pas de compatibilité Wemos / V4 (D2)** : les envs `lolin_s3_mini*` et la lib ADXL345 sont supprimés au lot 2.

Partitions `default_8MB.csv` : nvs 20 Ko, app0 / app1 2 × 3,2 Mo (OTA), LittleFS (`spiffs`) 1,5 Mo, coredump 64 Ko.

Nouveaux réglages (clés NVS de 15 caractères maximum, même ordre que l'enum `SettingId`) : `HallOpenL/R`, `HallCloseL/R`, `WingTrimL/R`, `AmpGain` (9 / 12 / 15), `Volume`, `LedBright`, `LedMaxmA`, `ServoStagger`, `ImuUpAxis`, `ServoIdleMs` (D6), `ApSsid`, `ApPassword` (D7, 8 à 31 caractères : minimum WPA2, maximum imposé par `SETTING_STRING_MAX` = 32). Le réglage d'essai `Test` actuel est supprimé.

---

## 7. Lots de travail (un commit chacun, dans cet ordre)

| Lot | Contenu | Critère de fin |
|---|---|---|
| 1 | **Base de build** : installer PlatformIO, compiler `Turret_firmware/` encore identique à l'original, avec l'env `lolin_s3_mini` (référence : erreurs existantes, taille flash / RAM), figer plateforme + libs | build reproductible, versions figées dans `Turret_firmware/platformio.ini` |
| 2 | **Cible Turret2** : `boards/turret2.json`, variant, partitions 8 Mo, envs `turret2*`, `pins.h` complet ; **suppression des envs `lolin_s3_mini*`** (D2) | `pio run -e turret2` passe |
| 3 | **Bugs du §5** | chaque bug corrigé, build des deux cibles |
| 4 | **Module Board** : état sûr, LEDs, boutons, SW1, PWR_FLT, raison du reset | fonctions testables depuis la console |
| 5 | **Séquence de boot** : réécriture de `setup()` + `BootState` réel selon le §3 | ordre du §3.2 respecté, journalisé à la console |
| 6 | **IMU LSM6DSOX** (+ `lib_deps`, ADXL345 retirée), scan I²C, orientation | lecture accéléro / gyro, contrôle « debout » |
| 7 | **Audio** : module Amp, gain / volume en réglages, arrêt sûr avant reboot / OTA | pas de plop au boot, au mute, au reboot |
| 8 | **Mouvement** : attache échelonnée, homing, seuils et trims en réglages, détection Hall incohérent (bloqué à 0 ou 4095, ou aucune variation pendant un mouvement) | cycle ailes / canons fiable |
| 9 | **Énergie** : délestage sur FLT, plafond LEDs, état `Fault`, compteur brownout, `/status` | défaut simulé (IO38 à la masse) → délestage |
| 10 | **Console de mise en service** (reprend l'idée commentée de `ManualState`) : `scan`, `imu`, `hall`, `servo <n> <angle\|off>`, `led <canal> <couleur>`, `tone <Hz>`, `gain 9\|12\|15`, `mute`, `flt`, `reset-reason` | utilisable pour le §8 |
| 11 | **Docs** : section Firmware du README (correction `getEvent`, procédure de flash) | — |
| 12 | **Page web de configuration** (§10) : correction des bugs `Settings` du §5, API JSON, authentification, page HTML embarquée, OTA depuis la page, mot de passe de l'AP | tous les réglages modifiables depuis un téléphone connecté à l'AP |

Ordre conseillé : 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9, puis 10 et 12 ensemble (la console et la page web appellent la même couche `Actions`), puis 11. La partie « réglages » du lot 12 (API + page, sans les tests matériels) peut être avancée juste après le lot 3 si on veut régler la tourelle plus tôt.

---

## 8. Mise en service de la première carte

Périphériques branchés un par un ; chaque étape doit passer avant la suivante.

| Étape | Branché | Ce qu'on vérifie |
|---|---|---|
| A | rien, alimentation via un wattmètre USB | VBUS, 5 V, 3V3 sur les points de test ; courant au repos ; IO38 à 3,3 V |
| B | — | `esptool.py flash_id` = **8 MB** ; `espefuse.py summary` (lecture seule) ; flash via USB natif (sinon BOOT + RESET, sinon UART0 / J16) ; `erase_flash`, puis `upload` + `uploadfs` |
| C | — | firmware de mise en service : LEDs, boutons, SW1, PWR_FLT, bannière |
| D | — | scan I²C → 0x6A ; ~9,8 m/s² sur l'axe vertical ; choix de l'axe |
| E | haut-parleur | tonalité 1 kHz à faible volume ; gains 9 / 12 / 15 dB ; **aucun plop** au boot, au mute, au reboot OTA |
| F | anneau puis canons (LEDs seules) | chaque canal séparément, luminosité plafonnée, plus de scintillement |
| G | capteurs Hall | valeurs brutes aimant près / loin → réglage des seuils |
| H | servos, un par un puis tous | courant crête au wattmètre ; PWR_FLT ; aucun reset ; cas pire : 6 servos + audio + LEDs |
| I | radar | trames reçues, cibles cohérentes |
| J | tout | cycle complet Idle → Activate → Firing → Disengage ; OTA ; USB déporté J1 (jamais deux hôtes à la fois) |

---

## 9. Décisions — D1 à D7 tranchées le 24.09.2026, D8 le 25.09.2026

| # | Sujet | Décision |
|---|---|---|
| D1 | **SW1** (IO3) | **mode banc**, pour usage futur : SW1 fermé au boot → aucun servo attaché automatiquement (on peut en attacher un à la fois depuis la console ou la page web), machine d'états à l'arrêt, console active, logs verbeux. SW1 ouvert = fonctionnement normal. Lu une seule fois au boot. Rappel : ne jamais brûler `STRAP_JTAG_SEL` |
| D2 | **Compatibilité** | **Turret2 uniquement**, pas de compatibilité Wemos / V4 : pas de `#ifdef` de carte, envs `lolin_s3_mini*` et lib ADXL345 supprimés |
| D3 | **Boutons A / B** | A = cycle de démo (Activate → Firing → Disengage) ; B = mute / unmute ; A + B maintenus au boot = remise à zéro des réglages (mot de passe de l'AP compris) ; B appui long (3 s) = Wi-Fi AP on / off |
| D4 | **Codes LED** | verte = battement (boucle vivante), fixe pendant le boot ; rouge = nombre de clignotements répété : 1 brownout / boucle de redémarrage, 2 défaut eFuse, 3 IMU absente, 4 radar muet, 5 Hall incohérent, 6 LittleFS. Priorité au plus petit numéro si plusieurs défauts |
| D5 | **eFuse latch-off ou auto-retry** | « au mieux » : le firmware gère les deux cas sans connaître la variante — détection de boucle de redémarrage et mode réduit (§4). Vérifier la variante dans la datasheet reste utile, mais ne bloque rien |
| D6 | **Servos au repos** | « au mieux » : **ailes** (rotation continue) détachées dès qu'elles sont arrêtées — sans impulsion elles s'arrêtent net, ce qui supprime le glissement dû à un neutre mal réglé ; **canons** détachés ~500 ms après la fin de leur mouvement (rentrés ou sortis, rien ne les charge) ; **rotation X / Z** maintenues tant que les ailes sont ouvertes (visée), détachées après `ServoIdleMs` (défaut 5 s) en `Idle` ailes fermées, puis ré-attachées **sur leur dernière consigne** (pas de saut). Ré-attacher un servo passe par le même échelonnement que le boot |
| D7 | **Sécurité** | **oui** : AP en WPA2 avec mot de passe (`ApPassword`, défaut `stillalive`, la page web affiche un avertissement tant qu'il n'a pas été changé) ; même mot de passe en authentification HTTP Basic sur l'API et sur `/update`. Récupération : A + B au boot remet le mot de passe par défaut. Nom de l'AP réglable (`ApSsid`, défaut « Portal Turret ») |
| D8 | **Emplacement du firmware Turret2** (25.09.2026) | **dossier séparé `Turret_firmware/`** : ce firmware ne s'installe que sur la carte Turret2 ; `Fork/` reste le firmware d'origine, intact, pour référence |

---

## 10. Page web de configuration (lot 12)

### 10.1 Existant

Il n'y a **pas** de page de configuration : `Turret_firmware/src/web/TurretWebServer.cpp` ne sert que `GET /` (`{"status":"OK"}`) et `GET /settings` (liste JSON en lecture seule), plus `POST /update` pour l'OTA (`Turret_firmware/src/web/Ota.cpp`). Aucune écriture de réglage n'est possible, et le code prévu pour (`SetFromString`) est bogué (§5).

### 10.2 Principes

- **Tout est dans le firmware** : une seule page `index.html` (HTML + CSS + JS sans framework), compressée en gzip et embarquée en PROGMEM par un script de pré-compilation PlatformIO (`extra_scripts`). Elle ne dépend donc ni de LittleFS ni d'Internet (l'AP n'a pas d'accès Internet : **aucune ressource externe, aucun CDN**), et elle est toujours à la version du firmware après une OTA. Budget : < 30 Ko gzip.
- **Générée à partir des réglages** : la page construit ses formulaires depuis `GET /api/settings` (clé, libellé, groupe, type, valeur, défaut, min, max, unité, application immédiate ou au reboot). Ajouter un réglage dans `Settings.cpp` suffit à le faire apparaître dans la page.
- **Rien de lourd dans les callbacks web** : ESPAsyncWebServer exécute ses callbacks dans la tâche `async_tcp`, sur l'autre cœur. Toucher aux servos, à l'I²S, aux LEDs ou à NVS depuis là crée des accès concurrents avec `loop()`. Les callbacks **déposent une commande dans une file** (FreeRTOS queue) ; `loop()` l'exécute et la réponse est lue par la page à la requête suivante (ou via `/api/status`).
- **Authentification** HTTP Basic sur tout sauf la page elle-même (identifiant `turret`, mot de passe `ApPassword`).
- **Téléphone d'abord** : mise en page utilisable à 360 px de large.

### 10.3 API

| Méthode et chemin | Rôle |
|---|---|
| `GET /` | la page (gzip) |
| `GET /api/status` | version, uptime, raison du reset, compteurs brownout et boucle de redémarrage, mode (normal / banc / réduit), PWR_FLT, IMU (présente, accélération), radar (vivant, cibles), Hall bruts G / D, état des ailes, état de la machine d'états, mémoire libre, clients Wi-Fi |
| `GET /api/settings` | description complète des réglages (§10.2) |
| `POST /api/settings` | un ou plusieurs `clé=valeur` ; validation, bornage, enregistrement NVS, application immédiate si possible ; renvoie les valeurs effectives |
| `POST /api/settings/reset` | valeurs par défaut (tout, ou un groupe) |
| `POST /api/action` | actions de test, les mêmes que la console série (couche `Actions`) : `servo <n> <angle\|off>`, `wings open\|close`, `guns extend\|retract`, `led <canal> <couleur>`, `tone <Hz> <ms>`, `gain 9\|12\|15`, `mute`, `demo` ; la machine d'états passe en `Manual` pendant les tests |
| `GET /api/log` | les N dernières lignes du journal série (tampon circulaire en RAM) : diagnostic sans câble USB |
| `POST /api/reboot` | redémarrage avec la séquence d'arrêt du §3.3 |
| `POST /update` | OTA existante, désormais authentifiée et précédée de la séquence d'arrêt |

### 10.4 Contenu de la page

| Section | Contenu |
|---|---|
| **État** | rafraîchi chaque seconde ; défauts actifs en tête (mêmes codes que la LED rouge, D4) |
| **Réglages** | par groupe : Mouvement (offsets, trims, échelonnement, maintien), Capteurs (seuils Hall, axe IMU), Audio (gain, volume), Lumière (luminosité, plafond mA), Wi-Fi (SSID, mot de passe), Système ; bouton « défaut » par champ, « Enregistrer » par groupe ; mention « appliqué au redémarrage » quand c'est le cas (Wi-Fi) |
| **Calibration** | Hall : valeurs en direct + boutons « capturer ouvert » / « capturer fermé » qui calculent les seuils ; IMU : « la tourelle est debout, capturer » ; trims des ailes : curseur appliqué en direct, arrêt automatique après 2 s |
| **Tests** | les actions de `POST /api/action` ; en mode banc, un seul servo attaché à la fois |
| **Journal** | `GET /api/log` |
| **Maintenance** | envoi d'un firmware (OTA), redémarrage, remise à zéro des réglages (avec confirmation) |

### 10.5 Pièges

- Modifier `ApSsid` / `ApPassword` coupe la connexion au redémarrage : la page doit l'annoncer et afficher le nouveau nom de réseau avant de redémarrer.
- `SETTING_STRING_MAX` = 32 : mot de passe de 8 à 31 caractères (WPA2 impose au moins 8).
- Les réglages sont mis en cache par les modules à l'initialisation (ex. `Gantry::Initialize` lit les offsets une fois) : chaque module doit exposer une méthode de rechargement, sinon « enregistré » ne veut pas dire « appliqué ».
- Écrire en NVS à chaque mouvement d'un curseur use la flash : n'enregistrer qu'au clic « Enregistrer » ; les curseurs de calibration appliquent en RAM seulement.
- L'OTA arrive pendant que la tourelle tourne : séquence d'arrêt (§3.3) **avant** d'accepter les données, pas seulement avant le reboot.

Les décisions sont aussi reportées dans le journal local (`docs/firmware-journal.md`), section « Décisions ».

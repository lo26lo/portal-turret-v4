# Revue du code du firmware Turret2 — notation par fonctionnalité

Journal de revue, démarré le 10.11.2026. Chaque fonctionnalité du firmware (`Turret_firmware/src/`) reçoit une note de 1 à 5 étoiles. **Règle : en dessous de 4 étoiles, la fonctionnalité est à repenser** (section « À repenser » en fin de document). Une nouvelle revue ajoute une entrée datée dans « Revues » et met à jour le tableau ; on ne réécrit pas une revue passée.

Barème :

| Note | Sens |
|---|---|
| ★★★★★ | Clair, sûr, testé (natif ou sur carte), rien à reprendre. |
| ★★★★ | Solide ; des détails à polir, aucun risque pour la tourelle. |
| ★★★ | Fonctionne, mais fragile, difficile à maintenir ou non testé : **à repenser**. |
| ★★ | Défaut connu qui peut gêner l'usage. |
| ★ | Ne marche pas ou dangereux pour le matériel. |

Ce qui a été lu pour la revue du 10.11.2026 : tout `src/` (hors `GunShotAudio.h`, données, et `page_gz.h`, généré), `src/web/page/index.html`, `test/test_logic/test_main.cpp` (40 tests), `platformio.ini`, `.github/workflows/firmware.yml`. État de la carte : v0.1 sur COM3 ; la v2 n'est pas fabriquée, tout ce qui la concerne n'a jamais tourné sur du matériel.

## Tableau — mis à jour le 10.11.2026 (2ᵉ revue, après la reprise des quatre points)

| Fonctionnalité | Fichiers | Note | Verdict |
|---|---|---|---|
| Brochage et garde-fou de carte | `pins.h`, `board/BoardCheck` | ★★★★★ | garder |
| Ordre de boot et boucle principale | `main.cpp` | ★★★★ | garder |
| Services de carte (LED d'état, boutons, PWR_FLT, boucle de reboot, geste d'usine) | `board/Board` | ★★★★ | garder |
| Bus I²C (reprise, scan) | `board/I2cBus` | ★★★★★ | garder |
| Rails v2 (extendeur) | `board/Power`, `board/Expander` | ★★★★ | garder, à valider sur la v2 |
| Journal RAM / RTC, core dump | `board/Log`, `board/CoreDump` | ★★★★ | garder |
| Réglages (NVS, table, validation) | `settings/Settings` | ★★★★ | garder |
| Machine à états et états | `states/*`, `logic/CycleLogic.h` | ★★★★ (était ★★★) | garder ; visée `AimZ` à valider sur la tourelle |
| Mécanique (attache différée, rampe, ailes, canons) | `motion/*` | ★★★★ | garder |
| LED NeoPixel | `light/Light` | ★★★★ | garder |
| Radar (analyse des trames) | `sensors/Radar`, `logic/RadarLogic.h` | ★★★★ (était ★★★) | garder ; à voir sur la carte (défaut 4) |
| IMU | `sensors/Motion` | ★★★★ | garder |
| Encodeur rotatif I²C | `sensors/RotaryEncoder` | ★★★★ | garder |
| Ampli MAX98357A | `audio/Amp` | ★★★★★ | garder |
| Décodeur MP3 image par image | `audio/Mp3FrameDecoder` | ★★★★★ | garder |
| Audio (I²S, tonalités, tir, voix) | `audio/Audio`, `audio/AudioLoop` | ★★★★ | garder |
| Console et commandes | `control/Actions*`, `control/CommandTable.h`, `scripts/check_commands.py` | ★★★★★ (★★★ le 10.11, ★★★★ puis ★★★★★ le 12.11) | garder |
| Auto-test | `control/SelfTest` | ★★★★ | garder |
| Veilles (radar, profonde, patrouilles) | `control/Sleep` | ★★★★ | garder, à valider sur la v2 |
| Statistiques | `control/Stats` | ★★★★★ | garder |
| Serveur web et API | `web/TurretWebServer`, `web/Ota` | ★★★★ | garder |
| Wi-Fi (point d'accès, réseau maison, NTP, mDNS) | `web/AccessPoint`, `web/Station` | ★★★★ | garder |
| Page web | `web/page/index.html`, `scripts/embed_page.py` | ★★★★ (était ★★★) | garder ; traduction à faire |
| Menu de debug (logique pure) | `logic/MenuLogic.h`, `ui/MenuTree.h` | ★★★★★ | garder |
| Écran OLED et pages d'information | `ui/DebugUi`, `ui/Display` | ★★★★ | garder |
| Logique pure et tests natifs | `logic/*.h`, `test/test_logic` | ★★★★★ | garder |
| Build, CI, scripts | `platformio.ini`, `scripts/`, `.clang-format`, workflow | ★★★★★ (★★★★ le 10.11 ; style imposé le 12.11) | garder |

Bilan au 12.11.2026 : 27 fonctionnalités, **9 à cinq étoiles, 18 à quatre, aucune en dessous**. (1ʳᵉ revue du 10.11 : 7 / 16 / 4 à trois.)

## Détail par fonctionnalité

### Brochage et garde-fou de carte — ★★★★★

- `pins.h` : une seule source de vérité, les deux cartes dans un `#if`, chaque broche commentée avec sa résistance et son interdit (« NEVER drive high »). `PIN_ABSENT` évite les `#ifdef` partout.
- `BoardCheck::Run()` est bien la première chose de `setup()` ; il ne touche que l'I²C, coupe les rails dès qu'il voit l'extendeur, et `HaltWrongBoard()` offre `rollback` vers l'autre partition OTA. Rien à reprendre.

### Ordre de boot et boucle principale — ★★★★

- Les 13 étapes du plan sont numérotées dans `setup()`, chaque sous-système a son `Update()` dans `loop()`, le chien de garde de 8 s est armé en fin de boot.
- À polir : vingt objets globaux et une struct `Turret` de références, c'est l'héritage Arduino, lisible mais tout dépend de tout ; `HandleEncoder()` et `HandleButtons()` dupliquent la traduction touche → commande ; `UpdateFaults()` mélange statistiques radar et détection de défaut.

### Services de carte — ★★★★

- Les sorties sont mises en sécurité avant tout (ampli muet, broches de gain en drain ouvert), le compteur de reboots suspects, la LED verte en sigma-delta pour laisser les timers LEDC aux servos, le geste SW1 + PROG : tout est là et commenté avec la date du piège qu'il corrige.
- À polir : `Board::Begin()` bloque jusqu'à 1 s (A + B tenus) avant que la console existe ; `IsSuspicious()` compte `POWERON` comme suspect, donc trois appuis sur RESET en une minute passent en mode réduit (documenté, mais surprenant pour l'utilisateur) ; `Board.h` a une section `public:` au milieu du `private:` pour `IsGreenModulated()`.

### Bus I²C — ★★★★★

- Reprise d'un bus bloqué (9 coups d'horloge puis STOP), `Wire.begin(PIN_SDA, PIN_SCL)` explicite, scan qui s'arrête au premier timeout pour ne pas déclencher le chien de garde. Exemplaire pour la taille.

### Rails v2 — ★★★★

- Ordre d'allumage, écart de 10 ms, temps de montée par rail, lecture de contrôle après chaque écriture de l'extendeur, `static_assert` entre `pins.h` et `OUTPUT0_ALL_OFF`. Le v0.1 compile en fonctions vides.
- Réserve : `Power::On()` fait des `delay()` (jusqu'à 25 ms) et peut être appelé depuis `loop()` (réveil, `Hall Always`) ; acceptable mais à mesurer sur la v2. **Jamais exécuté sur du matériel.**

### Journal et core dump — ★★★★

- Anneau de 4 Ko sous section critique, copie de 2 Ko en RTC qui survit à un plantage, `Tail()` sur le tas (tâche web). Le core dump est résumé au boot et téléchargeable.
- À polir : `Log.write` est appelé depuis la tâche web (OTA) et `loop()` ; `Serial.write` (USB CDC) n'est pas sous le verrou, c'est le pilote qui l'absorbe.

### Réglages — ★★★★

- Table unique, `COUNT` qui force une ligne par id, bornes, options « A|B|C », validation des mots de passe et SSID, `SetFromString` qui refuse « off » pour un entier non-option. Le JSON de la page en découle sans code à la main.
- À polir : `SettingId` est un enum non scopé ; `SettingValue` est une union de 64 octets, donc ~13 Ko de RAM pour 52 réglages (4 copies par entrée) ; `GetFloat`/`Set(float)` n'ont aucun réglage client ; `ResetToDefaults()` fait 52 écritures NVS d'un coup.

### Machine à états — ★★★ (à repenser)

Ce qui tient : `BootState` (attache une par une, homing avec temps limite), `FaultState` (délestage, reprise après 2 s de PWR_FLT haut), `IdleState` (repos, zone de détection, tourelle debout) sont propres et commentés.

Ce qui pèche :

- `ManualState.cpp` est un corps vide avec 15 lignes de code commenté et des membres (`buffer`, `index`, `BUFFER_SIZE`) inutilisés.
- `FIringState.cpp` (faute de casse dans le nom du fichier, piège Linux que la CI ne voit pas car personne ne l'inclut sous ce nom) : durée de tir **3000 ms codée en dur**, comparaison `millis() < start + 3000` non protégée du débordement, alors que le reste du firmware utilise des différences signées.
- `StateMachine::GetState()` journalise le nom de l'état avec des `break` après `return`, et `IdleState::OnActivate()` journalise une seconde fois (« IdleState » sort deux fois).
- Le cycle est figé : Activate → Firing → Disengage. **La tourelle ne vise jamais** : `SetRotationX/Z` ne sont appelés qu'avec 0 (boot et fermeture) ; la voix `07_search` n'est jamais jouée ; aucun état « cible perdue, je cherche ». C'est la partie « vraie tourelle Portal » du catalogue (B) qui n'a pas été faite, et la structure actuelle (une coroutine par état, sans donnée partagée sur la cible) ne l'aide pas.
- Aucun test natif : la logique de transition est enfouie dans des classes Arduino.

### Mécanique — ★★★★

- `ServoChannel` (attache différée, rampe, dernier pulse mémorisé), `Gantry::RunAttachSchedule` (un servo par fenêtre), `Wing` (seuils Hall à polarité libre, diagnostic « au rail » et « pas bougé », temps limite compté à partir de l'attache réelle), différences de temps signées partout avec la date du bug corrigé. La logique pure (rampe, Hall) est testée nativement.
- À polir : `Gantry.h` a des « constantes » en MAJUSCULES qui sont des membres modifiés par les réglages (`ANGLE_OFFSET_X`) ; `force || a && b` sans parenthèses ; `Gun.cpp` est resté au style Allman de l'upstream ; après le temps limite d'ouverture la Wing se déclare ouverte (hypothèse documentée, mais une aile bloquée passe pour ouverte).

### LED NeoPixel — ★★★★

- Petit, lisible, trame noire au boot, plafond de courant, test, motif un bit, arc-en-ciel, effet réglé. La correction FastLED 3.9.20 est tracée dans `platformio.ini`.
- À polir : l'échange rouge/vert de la 9ᵉ LED (`Show()`) est un correctif matériel hérité, à vérifier à l'œil puis à documenter ou retirer ; `Update(ulong deltaTime)` ignore `deltaTime`.

### Radar — ★★★ (à repenser)

Ce qui tient : `DecodeLd2450` et `IsInZone` sont en logique pure et testés contre l'analyseur upstream ; `End()` remet la ligne TX en entrée avant la coupure du rail v2 ; les cibles sont oubliées après 1 s sans trame.

Ce qui pèche, hérité de l'upstream :

- L'analyseur (`UpdateSerialData`) fait tourner **deux automates sur chaque octet** (trame de données et acquittement) qui écrivent dans le même `messageBuffer` avec le même `writeIndex`.
- **La fin de trame est reconnue sur `0x55 0xCC` n'importe où** dans les données, sans vérifier que 24 octets ont été reçus. Rare en pratique (les coordonnées plausibles ne produisent pas cette suite), mais une trame courte ou décalée est décodée sur des octets périmés sans aucune erreur signalée.
- `writeIndex` est un `uint8_t` sur un tampon de 256 : la protection contre le débordement est un accident, pas une intention.
- `radarTargetCount` et `previousRadarTargetCount` ne sont pas initialisés dans la classe (ils le sont par l'objet global) ; `lastSensorUpdateTime = millis()` est répété dans la boucle des trois cibles ; le journal « Radar Target Count Changed » sort à chaque variation.
- Le défaut 4 (radar muet) actuel sur la carte v0.1 est pré-existant depuis le 21.10.2026 et non élucidé : un analyseur strict (longueur, en-tête, pied) dirait tout de suite si des octets arrivent ou pas.

### IMU — ★★★★

- Filtre passe-bas, axe dominant, `CanDeploy()` qui ne bloque pas une tourelle sans IMU, réveil par mouvement avec interruption verrouillée (registres hors bibliothèque documentés).
- À polir : indentation à 4 espaces et accolades Allman dans un fichier, 2 espaces dans le reste du dépôt ; `ArmWakeup()` non testé sur carte (v2).

### Encodeur rotatif — ★★★★

- Trois modèles détectés dans l'ordre, identification par registre (seesaw) ou par taille de réponse (Soldered), arrêt du polling après 10 échecs, deux emplacements d'événement pour le double clic.
- À polir : `SOLDERED_COUNTS_PER_STEP = 1` marqué « à vérifier » ; `Probe()` existe ici et dans `I2cBus`.

### Ampli — ★★★★★

- La seule chose du firmware qui peut abîmer la carte est ici (IO21/IO47 jamais hauts) et c'est traité avec soin : `pinMode(OUTPUT_OPEN_DRAIN)` refait à chaque écriture, gain verrouillé par passage en shutdown, `muted`/`started`/`running` distingués. Logique du gain testée.

### Décodeur MP3 — ★★★★★

- API C de Helix utilisée directement pour décoder exactement ce qui va être joué, saut de l'étiquette ID3v2 (testé), tolérance aux faux mots de synchronisation, mémoire vérifiée avant `Begin()` parce que l'allocateur de Helix attend à l'infini. Sans Arduino : compilable sur PC.

### Audio — ★★★★

- Le flux I²S est bien compris et documenté (tampons DMA entiers, alignement, `auto_clear`), voix sur-échantillonnées en logique pure testée, le tir et les tonalités priment sur la voix.
- À polir : `Audio.h` garde `sampleReadIndex`, `loopCounter`, `isLooping` qui ne servent plus (ils sont dans `AudioLoop`) ; `GunShotAudio.h` définit 2057 lignes de données dans un en-tête inclus par `Turret.h`, donc par tout le programme (le compilateur n'émet le tableau que là où il est utilisé, mais c'est une `.cpp` avec `extern` qui devrait porter ces données) ; `StopVoice()` peut bloquer ~90 ms (`portMAX_DELAY`) ; `AudioLoop` reste au style upstream.

### Console et commandes — ★★★ (à repenser)

Ce qui tient : une seule implémentation pour la console, la page web et le menu OLED ; tout ce qui touche le matériel passe par `loop()` (file FreeRTOS) ; les garde-fous (`EnterTestMode`, « un servo à la fois », refus en défaut) sont systématiques ; les aides de labo (`sweep`, `loadtest`, `pins <n>`) sont précieuses.

Ce qui pèche :

- `Actions.cpp` fait **1345 lignes**, dont `Execute()` est une chaîne de 400 lignes de `if (command == "...")`. Chaque commande est déclarée quatre fois à la main : le test de nom, le code, la ligne de `Help()`, et la ligne du README. Rien ne vérifie qu'elles concordent (le README a déjà eu une ligne cassée le 04.11.2026).
- Trois listes de noms de servos (`SERVO_NAMES` dans `Actions.cpp`, `SERVO_NAMES` dans `SelfTest.cpp`, `names[]` dans `DebugUi.cpp`).
- `PinReport()` crée une tâche à priorité maximale sur le cœur 0 qui tourne en boucle fermée pendant 6 ms : outil de labo utile, mais il n'a rien à faire dans le même fichier que `mute`.
- Les réponses sont des `String` concaténées (fragmentation du tas), pas de test natif possible puisque tout touche `Turret`.

### Auto-test — ★★★★

- Passif puis actif, pas à pas depuis `loop()`, abandon sur PWR_FLT, résultats OK/FAIL/SKIP/CHECK bilingues, repli sur `resume` en mode normal.
- À polir : les étapes actives passent par des chaînes de commandes (`"servo rotz 100"`) ré-analysées par `Actions` ; `RunPassiveChecks()` allume le rail Hall sans l'éteindre (le `Gantry` s'en charge 2 s plus tard, c'est voulu mais implicite).

### Veilles — ★★★★

- La décision est en logique pure (`SleepLogic.h`, testée : fenêtre de nuit, niveaux), l'exécution dans `Sleep.cpp` suit le même ordre que le redémarrage (ampli, rails, radio, IMU), les patrouilles passent par la RTC avec un nombre magique.
- Réserve : la veille profonde, les patrouilles et le réveil IMU n'ont **jamais tourné** (carte v2 absente) ; `rtcNightEndEpoch` est comparé à `time(nullptr)` au boot, avant que NTP ait pu régler l'heure (l'horloge RTC continue en veille profonde, donc c'est probablement bon, à vérifier).

### Statistiques — ★★★★★

- Trente lignes, une écriture NVS par cycle, remise à zéro. Rien à dire.

### Serveur web et API — ★★★★

- Basic auth sur toutes les routes, portail captif pour les sondes de connectivité, les écritures passent par la file de `loop()`, les mots de passe ne sortent jamais, l'OTA vérifie les identifiants dans le gestionnaire d'upload (le middleware arrive trop tard) et arrête la tourelle avant d'accepter des données.
- À polir : aucune protection CSRF (un onglet malveillant sur le téléphone connecté au point d'accès pourrait poster `/api/action` avec les identifiants mémorisés ; risque faible sur un réseau local, à noter) ; JSON construit à la main en `String` ; `/api/status` lit depuis la tâche web des champs écrits par `loop()` sans verrou (documenté « valeurs en cache », acceptable pour de l'affichage).

### Wi-Fi — ★★★★

- AP + STA, NTP avec fuseau, mDNS, scan asynchrone protégé par mutex, déduplication des SSID, repli sur le mot de passe d'usine si NVS est corrompu.
- À polir : `AccessPoint::Stop()` fait `WiFi.mode(WIFI_OFF)`, ce qui coupe aussi la station si elle tourne (l'ordre `station->Stop(); accessPoint->Stop()` dans `wifi off` le masque).

### Page web — ★★★ (à repenser)

Ce qui tient : une seule page sans framework, embarquée gzippée, six onglets, tout ce que la console sait faire est accessible, `tools/check_web.py` la contrôle en CI contre une tourelle simulée.

Ce qui pèche :

- **Le source est écrit déjà compacté** : lignes de 300 caractères, `$`/`$$`, `kv()`, variables d'une lettre, CSS sur une ligne. C'est un fichier de 267 lignes qu'il faut dérouler mentalement pour toucher une chose.
- `throw 0` sert de contrôle de flux dans `saveGroup()` ; `alert()`/`confirm()` partout ; l'état est dans des globales (`st`, `cap`, `wifiEdit`).
- Les listes sont dupliquées avec le firmware : `FAULTS`, `AXES`, les noms de voix du `<select>`, les noms de servos. `/api/settings` sert déjà de source de vérité pour les réglages, mais pas pour ces listes.
- Pas de traduction, alors que le firmware et l'OLED sont bilingues (`Language`).

### Menu de debug (logique pure) — ★★★★★

- `MenuNav` : navigation, pages d'info, ajustement, confirmation, assistant, traduction des touches de l'encodeur, rendu en six lignes, le tout sans Arduino et couvert par une quinzaine de tests dont un sur le vrai `MENU_TREE`. `WrapText` gère l'UTF-8. C'est la meilleure partie du dépôt.

### Écran OLED et pages — ★★★★

- Tâche d'affichage sur l'autre cœur (la boucle ne paie que la copie de l'écran), une trame seulement si l'écran a changé (`memcmp`), U8g2 sans broches pour ne pas casser le bus (piège du 19.10.2026 documenté), QR codes, courbes Hall, vue radar, alerte de défaut 5 s, veille écran avec différence signée.
- À polir : `DebugUi::InfoLine()` est un `switch` de 320 lignes ; `nav` est alloué par `new` et jamais libéré (il vit toute l'exécution, c'est voulu, mais un membre direct ferait pareil) ; `qrModules`/`qrSize` sont des globales de fichier.

### Logique pure et tests natifs — ★★★★★

- `logic/*.h` : radar, Hall, boutons, LED, rampe, audio, veille, menu — tout ce qui se décide sans matériel est extrait et testé (40 tests Unity, lancés en CI). C'est ce qui a permis de corriger les bugs de l'OLED et des servos avec une date et un test.

### Build, CI, scripts — ★★★★

- Versions figées avec la raison de chaque épinglage, cinq environnements, variantes Arduino propres, version git injectée, page web embarquée au build, CI qui compile v0.1 / bring-up / v2, lance les tests natifs, vérifie la casse des `#include` et la page web.
- À polir : pas de `clang-format` imposé (trois styles cohabitent : 2 espaces, 4 espaces, Allman) ; la CI ne vérifie pas que `Help()` et le README listent les mêmes commandes.

## À repenser (< 4 étoiles) — propositions

### 1. Machine à états (★★★)

Objectif : un cycle de « vraie tourelle » lisible et testable.

- Sortir la décision dans `logic/CycleLogic.h` : une fonction pure `NextState(state, inputs, now)` avec en entrée la cible (position, âge), l'état des ailes, la fin de la voix, les temps ; testée nativement comme `SleepLogic`.
- Ajouter un état **Searching** (cible perdue : rotation Z balaie, voix `07_search`, retour Idle après `SearchMs`) et faire **viser** `Firing` (rotation Z vers `atan2(x, y)` de la cible, bornée) : ce sont les lots B du catalogue.
- Rendre réglables `FireMs` (3000) et `SearchMs` ; remplacer `millis() < start + n` par une différence signée.
- Supprimer `ManualState.cpp` (le corps vide suffit dans l'en-tête), corriger `FIringState.cpp` → `FiringState.cpp`, garder une seule trace par entrée d'état (`GoToState`).

### 2. Analyseur radar (★★★)

Objectif : une trame est acceptée entière ou rejetée, et on le sait.

- Réécrire l'analyseur comme automate à longueur fixe dans `logic/RadarLogic.h` : en-tête 4 octets → exactement 24 octets → pied 2 octets ; tout écart remet à zéro et incrémente un compteur `badFrames` visible dans `status` et sur la page Radar de l'OLED.
- Séparer l'acquittement (`FD FC FB FA … 04 03 02 01`) qui n'est utile qu'en configuration : le parser de données l'ignore.
- Tests natifs : trame nominale, trame avec `55 CC` dans les données, octet parasite entre deux trames, flux coupé.
- Ce travail est aussi le bon moment pour comprendre le défaut 4 de la carte v0.1 (compteur d'octets reçus bruts).

### 3. Console et commandes (★★★)

Objectif : une commande = une ligne de table.

- Table `static const Command COMMANDS[] = {{"mute", "mute [on|off]", "…", &Actions::Mute, Needs::Nothing}, …}` : nom, usage, aide, méthode, condition (`Needs::TestMode`, `Needs::FullPower`, `Needs::NotFault`). `Execute()` devient une boucle, `Help()` et le tableau du README sont **générés** depuis la table (script dans `scripts/`, vérifié en CI).
- Découper `Actions.cpp` : `control/commands/Lab.cpp` (`pins`, `sweep`, `loadtest`, `led pattern`), `Hardware.cpp` (servo, wing, gun, led, tone), `System.cpp` (settings, wifi, reboot, factory), `Status.cpp` (`StatusJson`, `LoadSnapshot`).
- Une seule liste de noms de servos, dans `Gantry` (`ServoChannel::GetName()` existe déjà ; ajouter `Gantry::ChannelAlias(i)`), reprise par `SelfTest` et `DebugUi`.

### 4. Page web (★★★)

Objectif : un source lisible, une sortie compacte.

- Garder `index.html` lisible (CSS et JS indentés, noms complets, une fonction par onglet) et laisser `scripts/embed_page.py` compacter au build (suppression des espaces et commentaires suffit ; pas de dépendance npm).
- Les listes (défauts, axes, voix, servos) viennent de l'API : `/api/status` donne déjà `faults` en nombres, ajouter un `/api/meta` (ou étendre `/api/settings`) avec les libellés, en anglais et en français selon `Language`.
- Remplacer `throw 0` par un retour explicite, les `alert()` par une zone de message dans la page.
- `check_web.py` reste la garantie ; lui ajouter un test de l'onglet Réglages (sauvegarde d'un groupe).

## Revues

### 10.11.2026 — Première revue complète

- Demande : relire tout le code, noter chaque fonctionnalité, repenser ce qui est sous 4 étoiles.
- Méthode : lecture intégrale de `src/` (14 000 lignes dont 2 600 de données et de page générée), de la page web, des tests natifs et de la CI ; notes attribuées sur la clarté, la sûreté matérielle, la robustesse et la présence de tests ; pas d'exécution sur carte pendant la revue.
- Résultat : 4 fonctionnalités à repenser (machine à états, analyseur radar, console, page web), propositions ci-dessus. Les parties qui protègent le matériel (ampli, garde-fou de carte, rails) sont au niveau attendu.
- Points transverses relevés : trois styles de code cohabitent (upstream non reformaté : `Gun.cpp`, `AudioLoop.cpp`, `Motion.cpp`, `Radar.cpp`), aucune revue ne remplace un essai de la v2 sur du matériel.
- Rien n'a été modifié dans le code pendant cette revue.

### 10.11.2026 (2ᵉ entrée) — Les quatre points repris

Demande : « fais au mieux pour remettre bien ». Les quatre fonctionnalités à ★★★ ont été reprises selon les propositions ci-dessus, compilées pour les deux cartes (`turret2` : RAM 27,5 %, flash 35,7 % ; `turret2_v2` : 27,6 % / 36,0 %), contrôlées par `check_web.py` (tout passe) et `check_commands.py` (45 commandes, 45 dans le README). Les tests natifs (6 nouveaux, 46 au total) ne tournent qu'en CI : pas de compilateur hôte sur ce poste. **La carte n'était pas branchée** (aucun port série) : rien n'a été essayé sur le matériel.

**Machine à états — ★★★ → ★★★★.** `logic/CycleLogic.h` (pur, testé) : `AimAngleDeg`, `SmoothAim`, `FiringDone` (différence signée, testée au débordement), `SearchDecision`, `SearchSweepDeg`. Nouvel état **Searching** entre Firing et Disengage : voix `07_search`, une cible revenue dans la zone → Firing de nouveau, `SearchMs` sans cible → Disengage. `FireMs` (3000) et `SearchMs` (4000) sont des réglages du groupe Detection. **Visée** (`AimZ`, bool, **faux par défaut**) : en Firing la rotation Z suit la première cible de la zone (lissée, ±60°), en Searching elle balaie ±30° toutes les 4 s ; le sens de rotation contre l'axe x du radar n'a jamais été vérifié, d'où le défaut à faux. `FIringState.cpp` renommé, `ManualState.cpp` supprimé (classe vide dans l'en-tête), un seul journal « State: … » dans `GoToState`, le compteur de cycles passé dans Activate (un cycle = un déploiement, quel que soit le nombre de rafales). Pas cinq étoiles : la visée est à valider sur la tourelle, et Activate / Disengage restent des coroutines non testées.

**Analyseur radar — ★★★ → ★★★★.** `logic::RadarParser` à longueur fixe (en-tête 4, charge 24, pied 2) : le pied n'est cherché qu'après les 24 octets, un pied faux compte un `badFrames` et resynchronise sur l'en-tête suivant, les acquittements et les octets parasites sont ignorés. Trois tests natifs : trame nominale, `55 CC` au début des données, acquittement + trame tronquée + octet parasite. `Radar.cpp` tombe à 95 lignes ; `status` / `/api/status` donnent `radar.bytes`, `frames`, `badFrames` (octets sans trame = débit ou protocole), repris sur la page (« silent (N bytes, no frame: baud rate?) »). Pas cinq étoiles : pas encore vu une vraie trame passer ; le défaut 4 de la v0.1 reste à élucider avec ces compteurs.

**Console — ★★★ → ★★★★.** Table `Actions::COMMANDS` (nom, usage, aide, catégorie, méthode, besoins `TestMode` / `FullPower`) ; `Execute()` est une recherche dans la table plus les deux garde-fous ; `help` est généré, groupé par catégorie. Fichier découpé : `Actions.cpp` (table, console, file web, réglages, système, audio, réseau, statut, 560 lignes), `ActionsHardware.cpp` (servos, ailes, canons, LED, voix, calibration), `ActionsLab.cpp` (`pins`, rails, balayages, test de charge). Une seule liste d'alias de servos : `Gantry::ChannelAlias` / `ChannelIndex`, reprise par `SelfTest`, le statut et `/api/meta`. `scripts/check_commands.py` compare la table et la section 11 du README (ajouté à la CI ; a trouvé la ligne `help` manquante). Pas cinq étoiles : réponses toujours en `String`, pas de test natif de l'interpréteur (il touche `Turret`).

**Page web — ★★★ → ★★★★.** Source réécrit lisible (CSS et JS indentés, noms complets, une section par onglet, 540 lignes) ; `scripts/embed_page.py` compacte au build (commentaires, indentation, lignes vides : 35 Ko → 29,5 Ko → 9,8 Ko gzip, l'ancien faisait ~9,5 Ko). `throw 0` et les `alert()` remplacés par une barre de message ; les `confirm()` restent pour les actions dangereuses. Les listes viennent de **`GET /api/meta`** (`Actions::MetaJson` : noms des défauts dans la langue de l'écran, axes, voix, servos, carte, version) : la page n'en garde aucune copie, `check_web.py` le vérifie, le simulateur les lit dans les sources. `check_web.py` passe aussi `node --check` sur le script **compacté**. Pas cinq étoiles : la page reste en anglais (les libellés de `/api/meta` sont déjà bilingues, le reste du texte non).

Erreurs de la session : la première version de `SearchSweepDeg` avait une onde triangulaire fausse (corrigée avant compilation, testée) ; un test de débordement de `FiringDone` était faux de ma part (5120 ms et non 1264) ; `check_commands.py` coupait les cellules sur les `\|` échappés du README ; `check_web.py` cherchait encore `command == "…"` dans `Actions.cpp`.

### 12.11.2026 — Relecture de la reprise du 10.11

Relecture du code écrit le 10.11, avant tout essai sur la carte.

**Trouvé et corrigé :**
- **Voix `search` jamais jouée.** `Audio::PlayVoice` refuse une voix tant que `ShootAudio.IsPlaying()`, et la queue du bruit de tir (la fin de l'échantillon après `Stop()`) joue encore à l'entrée de `Searching` : la ligne était refusée à chaque fois. Même piège pour `SearchMs = 0` : `Disengage` lançait *retire* sur la queue du tir (l'ancien `Firing` attendait la fin du son avant de passer la main). `SearchingState` joue maintenant la voix une fois le tir terminé et ne cède à `Disengage` qu'à ce moment-là.
- **Saut de la rotation Z** entre tir et recherche : le balayage partait de 0° alors que la visée pouvait être à 40°. `FiringState::LastAimDeg()` sert de point de départ au balayage, atteint en douceur (`SmoothAim`), et la rafale suivante repart de là.
- `check_commands.py` vérifie aussi que chaque usage commence par le nom, que l'aide et la catégorie ne sont pas vides, et que les catégories sont contiguës (sinon `help` imprime deux fois le titre).

**Vu, laissé tel quel :**
- `help` fait ~4 Ko ; le journal RAM lu par la page en fait 4 Ko aussi : sur la page, le début de l'aide est coupé. Idée ci-dessous (`help <catégorie>`).
- `DebugUi` garde ses noms de servos à 4 lettres (`wngl`) pour tenir sur 21 colonnes : une 2ᵉ liste, assumée.
- Les coroutines d'`Activate` et `Disengage` ne sont pas testées nativement (AceRoutine).

Notes inchangées. `turret2` compile (35,7 %). Toujours rien d'essayé sur la carte.

### 12.11.2026 (2ᵉ entrée) — Les sept idées faites

À la demande de l'utilisateur (« fais 1 à 7 »). Compilé pour les deux cartes, contrôles passés (voir la fin de l'entrée), rien d'essayé sur la carte.

1. **`aim`** (catégorie Status) : chaque cible du radar avec x / y, vitesse, angle depuis l'axe, côté, angle du servo de rotation Z correspondant (`Gantry::ServoAngleZ`, nouveau) et « in zone » ; rien ne bouge. `/api/status` a `aim.on` / `aim.deg` ; la page l'affiche à côté du radar.
2. **`help [catégorie]`** : une catégorie tient dans le journal de 4 Ko lu par la page ; `/api/meta` sert la liste des catégories, l'onglet Tests a un sélecteur + bouton « Aide » qui bascule sur le journal.
3. **Vitesse radar (B10, premier pas)** : réglage `MaxSpeedCms` (0 = sans limite) ; `logic::IsPlausibleSpeed` testé ; la zone (distance, angle, vitesse) est maintenant **dans `Radar`** (`ApplySettings`, `FirstTargetInZone()` sans argument) : les quatre appelants ne répètent plus les lectures de réglages. La page radar de l'OLED montre la vitesse (`v-25`).
4. **Œil vivant (B6)** : `LedEffect` = 2 ; `Light::Mood` posé par `main` depuis la machine à états : respiration rouge au repos (`BreathLevel` de la LED verte, réutilisé), rouge fixe en acquisition, clignotement 12 Hz au tir avec éclairs des canons, blanc froid en recherche.
5. **Page bilingue** : `web/PageText.h` (≈70 textes en / fr, table pure testée : clés uniques, rien de vide), servie dans `/api/meta` → `text` selon `Language` ; la page applique `data-i18n` / `data-i18n-placeholder` et `t(clé, repli)` pour les libellés d'état ; l'anglais reste écrit dans la page comme repli. Le simulateur lit la table dans les sources.
6. **`.clang-format` + CI** : style LLVM à 2 espaces, 120 colonnes ; `scripts/check_format.py` (`--fix` pour formater, utilise le `clang-format` du PATH ou celui de `pip install clang-format`) ; **tout `src/` et `test/` reformaté** (98 fichiers, hors données et générés) : les trois styles n'en font plus qu'un. La CI installe clang-format 19.1.7 et refuse un fichier non formaté.
7. **Table de commandes testable** : `control/CommandTable.h` est de la donnée pure (`commands::TABLE`, `Id`, `Needs`, `Find`, `HasCategory`), sans Arduino ; `Actions::HANDLERS[]` fait correspondre un `Id` à une méthode avec un `static_assert` sur la taille ; test natif `test_command_table` (ids dans l'ordre, usage commençant par le nom, noms uniques, catégories contiguës, `Find` insensible à la casse). `check_commands.py` et `check_web.py` lisent cet en-tête.

Piège de la session : le reformatage a plié les lignes longues de la table, et les deux scripts qui la lisaient à la ligne ont cessé de trouver cinq commandes ; leurs expressions acceptent maintenant une ligne pliée (`\s*` entre les champs, ancrage sur `Id::`). Mon erreur : `python` dans le shell est le Python système, sans clang-format ; les scripts de style se lancent avec celui de PlatformIO (`~/.platformio/penv/Scripts/python.exe`), noté dans le journal.

Notes : la console passe à **★★★★★** (table pure testée, `help` paginé, trois fichiers, CI), la page web reste ★★★★ (bilingue, mais textes libres des `small` et messages encore en anglais), le radar reste ★★★★ (vitesse exploitée, toujours pas vu de trame réelle), le build/CI passe à **★★★★★** (format imposé).

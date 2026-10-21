# Carte Turret2 v2 — plan matériel

> **Statut : plan, rien n'est fabriqué.** Rédigé le 21.10.2026 à partir de [AMELIORATIONS_V2.md](AMELIORATIONS_V2.md) (propositions), des décisions de l'utilisateur et du projet KiCad v0.1 du dépôt (`Turret2_portable/`, commit `8248fcd`). Les valeurs marquées **[DS]** ont été relues dans la datasheet le 21.10.2026 (sources en fin de document) ; celles marquées **[à vérifier]** n'ont pas pu l'être.
>
> Si le projet KiCad maître (hors dépôt) a changé depuis le 25.09.2026, les numéros de composants v0.1 cités ici (R4, C9…) sont à revérifier.
>
> **Mis à jour le 21.10.2026 (soir)** : brochage refait pour faciliter le routage, et extendeur de GPIO intégré au plan principal (§4.9, §5). L'étude est reproductible avec `Turret2_portable/tools/gpio_study/`.

---

## 0. Résumé

**Objectif** : pouvoir couper ce qui consomme (servos, LEDs, radar, capteurs Hall) et mettre l'ESP32 en deep sleep sous 1 mA, sans perdre de fonction.

| Décision | Choix | Pourquoi |
|---|---|---|
| Radar | **RD-03D seul** (Ai-Thinker, déjà monté), **pas de radar sentinelle** | choix de l'utilisateur (21.10.2026) ; le LD2410S n'est pas acheté |
| Coupure des rails | 3 × **TPS22965** (servos, anneau LED, radar) + 1 × **AO3401A** (capteurs Hall) | 6 A pour les servos ; ON compatible 3,3 V ; coupés au reset grâce aux pull-downs |
| Protection d'entrée | eFuse **TPS259533** à la place du TPS259573 (même boîtier, même brochage) | sortie **écrêtée à 5,2–5,7 V** : protège les switches (5,7 V max) et les servos sans toucher au diviseur |
| GPIO | **brochage refait pour le routage** (étude §5.1) ; **extendeur I²C TCAL6416 au dos**, à côté de l'IMU, pour les 4 commandes de rails | 4 lignes de moins à sortir du module ; le côté gauche du module, le plus mal placé, est presque vide ; bus groupés ; ampli face à l'ampli. Variante sans extendeur en §9 |
| Boutons | **A et B supprimés**, **SW1 gardé**, PROG (SW2) gardé | interface par la page web ; le knob et l'OLED ne sont que des outils de debug sur le Qwiic |
| Face arrière | IMU (U6), ampli (U9) et extendeur au verso ; switches de puissance au recto | le chemin de puissance des servos reste au recto |
| Corrections v0.1 | entrées Hall et NeoPixel tirées, pull-up sur IO0, résistances série sur les lignes qui sortent de la carte, TVS à faible fuite | broches flottantes, protection des GPIO, budget de veille |

---

## 1. Modes de veille avec le RD-03D seul

Sans radar sentinelle, la carte ne peut pas « voir » quelqu'un en deep sleep : le RD-03D consomme ~110 mA en continu et n'a pas de mode veille (voir B9 dans [firmware-improvements.md](firmware-improvements.md)). D'où trois modes :

| Mode | Allumé | Coupé | Réveil / sortie | Consommation (5 V) |
|---|---|---|---|---|
| **Actif** | tout | — | — | selon les mouvements (pointes de servos ~3 A) |
| **Veille radar** | RD-03D, ESP32 (Wi-Fi coupé ou non, CPU ralenti) | servos, anneau LED, capteurs Hall, ampli (SD bas) | une cible du RD-03D, la page web, PROG | **≈ 130–150 mA [estimation]** (radar ~110 mA + ESP32 20–40 mA) |
| **Veille profonde** | IMU en mode réveil, ESP32 en deep sleep | tout le reste, radar compris | **IMU** (on tape ou on bouge la tourelle) sur IO15 ; **minuterie RTC** ; option **touche tactile** sur IO2 (§5) | **≈ 0,3–0,5 mA** (§1.1) |

**Option « ronde » en veille profonde** : réveil par minuterie toutes les *T* secondes, radar allumé pendant *t* secondes (le temps de démarrage du RD-03D est à mesurer), puis retour au deep sleep si personne. Moyenne ≈ (110 mA + ~40 mA d'ESP32) × *t* / *T*. Exemple : *t* = 2 s, *T* = 60 s → **≈ 5 mA** ; *T* = 30 s → ≈ 10 mA. Le temps de réaction est alors de *T* au pire.

Exemple d'usage : veille radar le jour, veille profonde la nuit (heure NTP), réveil par l'IMU à tout moment.

### 1.1 Bilan de la veille profonde

| Poste | Courant | Source |
|---|---|---|
| eFuse TPS259533, quiescent | ~175–250 µA | [DS] valeur des variantes x0/x1/x5 ; celle des variantes x3 est vide dans l'extraction : **[à vérifier]** |
| Diodes TVS D1, D5 (SMAJ5.0A) sur VBUS | **jusqu'à 800 µA chacune** (IR max à 5,0 V) | [DS] Vishay ; en pratique souvent bien moins : **à mesurer**. Remplacement proposé en §7 |
| 3 × TPS22965 coupés | 3 × 2 µA max | [DS] |
| AO3401A coupé (Hall) | fuite < 1 µA | [DS] |
| Extendeur TCAL6416 au repos | ~1 µA typ. | [DS] |
| MAX98357A, SD_MODE bas | 0,6 µA typ., 2 µA max | [DS] — attention : **340 µA** si SD_MODE est haut sans horloge (mode standby) |
| SN74AHCT125, entrées à la masse | 2 µA max à 25 °C | [DS] |
| ESP32-S3, deep sleep, réveil EXT1 | ~10–20 µA | ordre de grandeur Espressif **[à mesurer]** |
| TPS631000 (3,3 V) en mode PFM (MODE = GND) | quelques dizaines de µA ramenés au 5 V | **[à vérifier]** |
| LSM6DSOX en mode réveil | quelques µA à quelques dizaines selon l'ODR | **[à vérifier]** |
| Résistances de tirage | 0 (EN bas, bus I²C au repos haut) | — |
| SW1 fermé (R9 10 kΩ) | 330 µA | à éviter : **laisser SW1 ouvert** en usage normal |
| LEDs d'état D3, D4 | 0 (GPIO non RTC, flottantes en deep sleep) | — |
| Capteurs Hall | **0 si coupés** ; sinon plusieurs mA (capteurs linéaires, modèle à identifier) | d'où le P-MOS `HALL_EN_N` (§2) |
| Modules Qwiic (OLED, knob) | 0 : **débranchés en usage normal** (debug seulement) | décision de l'utilisateur |
| **Total** | **≈ 0,25–0,35 mA** avec TVS à faible fuite ; jusqu'à ~1,9 mA dans le pire cas des SMAJ5.0A | |

---

## 2. Architecture d'alimentation v2

```
USB-C / J1 ──┬─ D1, D5 TVS (§7) ──► IC1 eFuse TPS259533 ──► +5V (sortie ≤ 5,7 V, limite ≈ 3,9 A)
             │                      R3 1 MΩ / R4 249 kΩ : OVLO 5,67–6,37 V (garde-fou)
             │
 +5V ───┬── SW_SERVO  TPS22965, CT 4,7 nF ─► 5V_SERVO ─► C9, C18 (2 × 220 µF) ─► J7–J10 servos
        │                                                                     └► J3, J4 canons (servo + 2 LEDs, même broche +5 V)
        ├── SW_LED    TPS22965, CT 1 nF ───► 5V_LED ───► 10 µF ─► J14 anneau (9 LEDs)
        ├── SW_RADAR  TPS22965, CT 1 nF ───► 5V_RADAR ─► 10 µF + 100 nF ─► J5 RD-03D
        ├── U9 MAX98357A (non commuté ; SD_MODE bas en veille = 0,6 µA)
        ├── IC2 SN74AHCT125 (non commuté ; entrées tirées à la masse)
        └── U1 TPS631000 ─► +3V3 ─┬─ ESP32-S3, LSM6DSOX, extendeur TCAL6416, pull-ups I²C, J11 Qwiic (debug)
                                  └─ AO3401A (HALL_EN_N) ─► 3V3_HALL ─► C31, C32 ─► J17, J18

Commandes des rails : SERVO_EN, LED_EN, RADAR_EN, HALL_EN_N = sorties P00–P03 de l'extendeur (I²C), au dos.
```

**Le connecteur des canons (J3, J4) n'a qu'une broche +5 V**, partagée par le servo et les 2 LEDs du canon (relevé dans `Turret2.kicad_pcb` : 1 = +5V, 2 = PWM, 3 = données LED, 4 = GND). Les LEDs des canons sont donc sur `5V_SERVO`. Pour les séparer, il faudrait un connecteur à 5 broches et un câble refait ; ce n'est pas nécessaire : en veille, tout est coupé de toute façon.

**Courants d'appel** (I ≈ C × 5 V / t_R, t_R lu dans le tableau 1 de la datasheet [DS], VIN = 5 V) :

| Switch | Capacité en aval | CT | t_R | Pointe |
|---|---|---|---|---|
| SW_SERVO | ~440 µF (C9 + C18) + 6 × 100 nF | 4,7 nF | 8,2 ms | ≈ 0,27 A |
| SW_LED | ~10 µF + découplage de l'anneau | 1 nF | 2,0 ms | ≈ 25 mA |
| SW_RADAR | ~10 µF + entrée du module (inconnue) | 1 nF | 2,0 ms | < 0,1 A |

La limite de l'eFuse est **≈ 3,9 A** (R5 = 523 Ω ; déduit des points de la datasheet : 487 Ω → 4,17 A typ. [DS]). Toujours allumer les switches **un par un**, à ≥ 10 ms d'écart.

**Ordre de mise sous tension et de coupure** (firmware, §8) :

- Coupure : arrêter le PWM des servos et passer leurs broches à l'état bas → LEDs à noir puis lignes de données à l'état bas → `SERVO_EN`, `LED_EN` bas → UART du radar arrêté, sa broche TX (IO35) en entrée → `RADAR_EN` bas → `HALL_EN_N` haut → ampli : SD (IO37) bas avant d'arrêter l'I²S.
- Allumage : dans l'ordre inverse, un switch à la fois.

Raison des étapes « lignes à l'état bas » : une ligne de signal à l'état haut vers un module non alimenté le **rétro-alimente** par ses diodes de protection (servo, LED, radar).

---

## 3. Liste d'achat

### 3.1 À acheter

| Qté (+ réserve) | Composant | Référence | Boîtier | Rôle |
|---|---|---|---|---|
| 3 (+2) | Load switch 6 A, 16 mΩ, **avec QOD** | TI **TPS22965DSGR** (pas la version N) | WSON-8 2 × 2 mm, pad thermique | SW_SERVO, SW_LED, SW_RADAR |
| 1 (+1) | eFuse 2,7–18 V, écrêtage 5,7 V, EN actif bas | TI **TPS259533DSGR** | WSON-8 2 × 2 mm (comme IC1) | remplace TPS259573 |
| 1 (+2) | P-MOS 30 V, 4 A | AOS **AO3401A** | SOT-23 | alimentation des capteurs Hall |
| 1 (+1) | Extendeur I²C 16 E/S, RESET, 1,08–3,6 V | TI **TCAL6416** (TSSOP-24, PW) ou NXP **PCAL6416A** | TSSOP-24 | commandes des rails (§4.9) |
| 2 (+2) | 100 nF 25 V X7R | — | 0603 | découplage VCCI et VCCP de l'extendeur |
| 1 (+2) | 4,7 nF **25 V X7R** | — | 0603 | CT de SW_SERVO (25 V minimum [DS]) |
| 2 (+2) | 1 nF **25 V X7R** | — | 0603 | CT de SW_LED et SW_RADAR |
| 3 (+2) | 1 µF 25 V X7R | — | 0603 | CIN des switches (au ras de VIN) |
| 3 (+2) | 100 nF 25 V X7R | — | 0603 | VBIAS des switches |
| 3 (+2) | 10 µF 25 V X7R | — | 0805 ou 1206 | sortie 5V_LED, 5V_RADAR ; 10 µF du MAX98357A s'il ne suit pas |
| 12 (+5) | 100 kΩ 1 % | — | 0603 | pull-downs : 3 × EN, 3 × entrées AHCT, 2 × Hall ; pull-up : grille Hall ; réserve |
| 1 (+2) | 10 kΩ 1 % | — | 0603 | pull-up de IO0 (recommandé par Espressif) |
| 9 (+3) | 330 Ω | — | 0603 | série : 6 PWM servos, 3 données NeoPixel |
| 2 (+2) | 1 kΩ | — | 0603 | série : UART du radar |
| 2 (+1) | TVS 6,5 V | Vishay / Littelfuse **SMAJ6.5A** | SMA (comme D1, D5) | remplace SMAJ5.0A (§7) |
| 1 | Pâte basse température **Sn42Bi58** | — | seringue | brasage du verso (§6) |
| 1 | Second pochoir (verso) | JLCPCB | — | brasage du verso |

### 3.2 Retirés de la v0.1

J12, J13 (boutons A et B), R13, R17 (1 kΩ série), R19, R20 (pull-ups 10 kΩ), C11, C12 (100 nF).

### 3.3 Prévus par AMELIORATIONS_V2.md mais **à ne pas acheter**

| Pièce | Pourquoi |
|---|---|
| HLK-LD2410S + AO3401A + cavalier + JST-SH | pas de radar sentinelle (RD-03D seul) |
| 2N7002 + 10 kΩ (`LED_OE_N`) | inutile : les pull-downs sur les entrées de l'AHCT suffisent (§4.6) |
| 274 kΩ (R4) | rejeté : avec la tolérance de l'eFuse, coupure possible dès 5,25 V (§4.2) |
| 100 kΩ sur `AMP_SD` | le MAX98357A a déjà un pull-down interne de 100 kΩ sur SD_MODE [DS] |

---

## 4. Fiches composants : découplage et résistances

### 4.1 TPS22965 (× 3) — load switches

Brochage (WSON-8, vue de dessus) [DS] : 1, 2 = VIN ; 3 = ON ; 4 = VBIAS ; 5 = GND ; 6 = CT ; 7, 8 = VOUT ; pad thermique = GND.

| Broche | Raccordement | Règle de la datasheet [DS] |
|---|---|---|
| VIN (1 + 2) | +5V, en aval du point d'étoile ; **1 µF X7R au ras des broches** | « 1 µF usually sufficient » ; idéal CIN ≈ 10 × CL, non obligatoire |
| VBIAS (4) | **+5V** (même rail que VIN), 100 nF | 2,5–5,7 V ; R_ON spécifié pour VBIAS = 5 V ; garder VIN ≤ VBIAS |
| ON (3) | sortie de l'extendeur + **100 kΩ vers GND** | « Do not leave floating » ; VIH 1,1 V, VIL 0,5 V : compatible 3,3 V ; fuite 0,5 µA. Le pull-down tient le switch coupé quand les ports de l'extendeur sont en entrée (mise sous tension, RESET) |
| CT (6) | condensateur **X7R 25 V** vers GND, piste très courte | la tension sur CT peut monter à 12 V |
| VOUT (7 + 8) | rail commuté | avec QOD : 225 Ω de décharge quand le switch est coupé |
| Pad thermique | GND, vias (Ø 0,2–0,3 mm) | 3 A × 3 A × 16 mΩ ≈ 0,14 W |

- **Valeurs** : abs. max 6 V, recommandé 5,7 V ; 6 A continu, 8 A en impulsion ; IQ 50 µA (allumé), 2 µA (coupé) [DS].
- **CL > CIN** sur SW_SERVO (440 µF en aval) : la datasheet l'admet mais conseille alors **un CT plus grand** pour limiter la chute de VIN → 4,7 nF.
- **QOD** : choisir TPS22965 et non TPS22965N, pour que les rails coupés retombent vraiment à 0 V (pas de servo ni de radar à moitié alimenté).
- **Alternative plus facile à souder** pour SW_LED et SW_RADAR : TPS22918 (SOT-23-6, 2 A, 52 mΩ, IQ 8,7 µA) [DS], mais VIN recommandé 5,5 V max (5,7 V possible pendant un écrêtage de l'eFuse, sous les 6 V abs. max). Un seul modèle (TPS22965) simplifie la commande.

### 4.2 TPS259533 — eFuse (remplace le TPS259573, IC1)

- **Même boîtier et même brochage** : la broche 2 est EN/OVLO, active basse, sur toutes les variantes x3 [DS]. Le diviseur R3 / R4 reste utilisable.
- **Écrêtage** : se déclenche à 5,5–5,9 V et maintient la sortie à **5,2–5,7 V** [DS], en ~5 µs. En écrêtage, l'eFuse dissipe (VIN – 5,45 V) × I, puis coupe en surchauffe et réessaie toutes les 93 ms.
- **Diviseur OVLO** : garder **R4 = 249 kΩ** (seuil 1,13–1,27 V [DS] → coupure à 5,67–6,37 V) comme seconde ligne. La valeur de 274 kΩ proposée dans AMELIORATIONS_V2.md (attribuée par erreur à R6, qui est le pull-up de PWR_FLT) donnerait 5,25–5,91 V : **coupure possible sur un chargeur USB à 5,25 V**.
- **Limite de courant** : R5 = 523 Ω → ≈ 3,9 A ; R_ILM minimum 487 Ω [DS]. dVdt : C2 47 nF inchangé.
- **Entrée** : abs. max 20 V [DS] ; aucune contrainte nouvelle pour les TVS.

### 4.3 AO3401A — alimentation des capteurs Hall

- Source sur +3V3, drain sur 3V3_HALL (J17.1, J18.1, avec C31, C32 déplacés côté commuté, au ras des connecteurs).
- Grille sur **P03 de l'extendeur (`HALL_EN_N`)** + **100 kΩ vers +3V3** : coupé par défaut. Les ports de l'extendeur sont des entrées à la mise sous tension et après un RESET [DS] : la résistance tient alors le P-MOS coupé. Elle le tient aussi en deep sleep.
- V_GS = –3,3 V : R_DS(on) < 85 mΩ à –2,5 V, seuil –0,5 à –1,3 V [DS]. Aucune résistance de grille nécessaire (charge de grille ~7 nC, ligne lente).
- Firmware : allumer, attendre ~1 ms **[durée de démarrage du capteur à vérifier]**, lire, et couper quand les ailes ne bougent pas, si la consommation le justifie.

### 4.4 MAX98357A — ampli (déplacé au verso)

- **Découplage** : 10 µF + 100 nF au ras des broches VDD (7, 8), à déplacer avec l'ampli ; le bulk peut rester au recto, au pied du via. **[valeurs du schéma d'application : à vérifier]**
- **SD_MODE** : pull-down interne de **92–108 kΩ** [DS]. Garder **R10 = 2 kΩ** en série depuis la GPIO : 3,3 V × 100 / 102 ≈ 3,24 V → canal gauche. GPIO flottante → arrêt. **Aucun pull-down externe nécessaire.**
- **GAIN_SLOT** : même montage, mais sur **IO38 en direct et IO40 par R11 = 100 kΩ** (v0.1 : IO21 et IO47), **toujours en drain ouvert**. Gains [DS] : non connecté 9 dB, GND 12 dB, GND par 100 kΩ 15 dB, VDD 6 dB, VDD par 100 kΩ 3 dB. Garder R11 près de l'ampli. IO38 et IO40 n'ont ni pull-up ni glitch au démarrage (Espressif).
- **Courants** : arrêt 0,6 µA ; **standby 340 µA** si SD_MODE est haut sans BCLK [DS] → toujours mettre SD bas avant d'arrêter l'I²S (règle déjà appliquée).
- **Pad exposé** : non relié en interne, à mettre au GND par des vias pour la dissipation [DS].
- **Sorties HP** : courtes et en paire jusqu'à J15. Option CEM : emplacements pour des ferrites (0603) en série sur OUTP / OUTN si le câble du HP est long.

### 4.5 LSM6DSOX — IMU (déplacée au verso)

- **Découplage** : 100 nF au plus près de VDD (8) et 100 nF au plus près de VDDIO (5) ; un 10 µF sur VDD est souvent recommandé par ST. **[datasheet ST non téléchargeable le 21.10.2026 : à vérifier]**
- **Broches** : inchangées et correctes sur la v0.1. SDO/SA0 au GND (adresse 0x6A) ; CS au +3V3 (mode I²C) ; SDx, SCx au GND ; INT2, OCS_Aux, SDO_Aux non connectés (tirages internes actifs par défaut).
- **Pastilles 9 et 10** : soigner le pochoir ; un pont de soudure entre elles a été vu sur la deuxième carte (sans conséquence, voir firmware-improvements.md, 21.10.2026).
- **Mécanique** : au centre de la carte, loin des vis M2, des connecteurs et des sources de chaleur (ampli, TPS631000, switches). **Ni via ni piste sous le boîtier LGA** (contraintes, et bonne pratique ST).
- **Axes** : retournée au verso, Z s'inverse ainsi que X ou Y → matrice de correction dans `Motion` (§8).
- **INT1** → IO15 (réveil EXT1). Configurer l'interruption de réveil en mode **verrouillé** (latched), pour qu'une impulsion courte ne soit pas manquée.

### 4.6 SN74AHCT125 — tampon NeoPixel (inchangé)

- VCC sur +5V non commuté, 100 nF au ras de la broche 14 [DS : « good bypass capacitor »].
- **Entrées 1A, 2A, 3A : 100 kΩ vers GND** (nouveau). Sur la v0.1, elles flottent pendant le boot et en deep sleep (GPIO en haute impédance) [DS : « all unused inputs must be held at VCC or GND »]. Avec les pull-downs, les sorties restent à l'état bas : pas de rétro-alimentation des LEDs coupées, pas de consommation parasite.
- /OE : **restent à la masse** comme sur la v0.1 (l'inverseur 2N7002 proposé devient inutile).
- 4ᵉ porte : 4A et 4/OE au GND, 4Y en l'air : déjà correct sur la v0.1.
- **Sorties 1Y, 2Y, 3Y : 330 Ω en série** près de l'AHCT (nouveau) : amortit les réflexions sur les câbles et limite le courant dans la diode d'entrée d'une LED non alimentée. Bonne pratique courante pour les WS2812 / SK6812.
- ΔICC = 1,35 mA par entrée à 3,4 V [DS] : sans importance tant que l'ESP32 tient la ligne à l'état bas au repos (protocole NeoPixel).
- **Attribution des portes libre** : n'importe quelle porte peut piloter n'importe quelle chaîne de LEDs. Les entrées 1A / 2A sont d'un côté du boîtier, 3A / 4A de l'autre : choisir les portes au routage pour éviter les croisements.

### 4.7 ESP32-S3-MINI-1-N8 — rappels

- EN : 10 kΩ + 1 µF (R14, C13) = valeurs recommandées par Espressif ✓.
- 3V3 : 10 µF + 100 nF au ras de la broche 3 ✓ (C15, C16).
- **IO0** : ajouter **10 kΩ vers +3V3**. Espressif : « It is recommended to place a pull-up resistor at the GPIO0 pin » (*Schematic Checklist*, section *Strapping Pins*). La v0.1 n'a que le pull-up interne, de **45 kΩ** [DS, datasheet ESP32-S3], actif au reset ; il suffit en pratique (boots normaux sur les deux cartes), la résistance externe n'est qu'une marge contre les parasites au moment du reset. « Do not add high capacitors at GPIO0, or the chip may enter download mode » : pas de condensateur d'anti-rebond sur PROG.
- Glitchs au démarrage : IO1–IO14, IO17, IO18 font une impulsion basse d'environ 60 µs à la mise sous tension (Espressif). En v2, ça concerne surtout **SCL / SDA (IO11, IO12)** : récupération du bus I²C au boot (§10). Sans effet sur les PWM et les NeoPixel, dont les rails sont coupés à ce moment-là.

### 4.8 Lignes qui sortent de la carte

| Ligne | Ajout | Raison |
|---|---|---|
| PWM servos (IO1, IO17, IO26, IO33, IO34, IO36) | 330 Ω en série, **près du connecteur** | un +5V touchant la broche de signal (J3, J4 : +5V voisin du PWM) ne détruit plus la GPIO (courant limité à quelques mA) ; moins de rétro-alimentation quand le rail est coupé |
| UART du radar (RX IO48, TX IO35) | 1 kΩ en série, **près de J5** | le radar coupé n'est plus alimenté par sa ligne RX à l'état haut (≤ 3,3 mA) ; protection ESD |
| Hall (IO8, IO10) | 100 kΩ vers GND, près de J17 / J18 | capteur débranché ou coupé → lecture ≈ 0, détectée par l'auto-test (sur la v0.1, une entrée en l'air passe l'auto-test, vu le 21.10.2026). Option : RC 1 kΩ / 10 nF contre le bruit des moteurs |

Les résistances série vont **près des connecteurs**, pas au pied du module : la limitation de courant est la même, et le pourtour du module est la zone la plus chargée de la carte.

### 4.9 TCAL6416 — extendeur de GPIO (au dos, à côté de l'IMU)

Brochage TSSOP-24 [DS] : 1 INT, 2 VCCI, 3 RESET, 4–11 P00–P07, 12 GND, 13–20 P10–P17, 21 ADDR, 22 SCL, 23 SDA, 24 VCCP.

| Broche | Raccordement | Règle de la datasheet [DS] |
|---|---|---|
| VCCI (2) | +3V3, **100 nF** au ras | alimentation côté bus I²C ; les pull-ups de SCL, SDA et RESET vont à VCCI |
| VCCP (24) | +3V3, **100 nF** au ras | alimentation des ports |
| SDA, SCL (23, 22) | bus I²C de la carte | pull-ups communs R15 / R16, aucun en plus |
| RESET (3), actif bas | **relié à EN de l'ESP32** (`S3En`, déjà tiré par R14) | « connect to VCCI through a pull-up resistor, if no active connection is used » : un reset de l'ESP32 par EN remet aussi l'extendeur à zéro |
| ADDR (21) | GND | « connect directly to VCCP or ground ». Adresse attendue 0x20 (0x21 avec ADDR à VCCP), comme le PCAL6416A **[à vérifier : la figure de l'adresse n'est pas lisible dans l'extraction]** |
| INT (1) | non relié (ou pastille) | drain ouvert ; pull-up à prévoir seulement si on l'utilise un jour |
| P00 | `SERVO_EN` → ON de SW_SERVO (100 kΩ vers GND) | |
| P01 | `LED_EN` → ON de SW_LED (100 kΩ vers GND) | |
| P02 | `RADAR_EN` → ON de SW_RADAR (100 kΩ vers GND) | |
| P03 | `HALL_EN_N` → grille de l'AO3401A (100 kΩ vers +3V3) | |
| P04–P17 | libres : quelques-unes sur des pastilles de test | 12 E/S de réserve |

- **Ports en entrée** à la mise sous tension et après un RESET [DS] : les pull-downs et le pull-up tiennent alors tous les rails coupés.
- **Piège** : un redémarrage logiciel de l'ESP32 (`ESP.restart()`, panic, watchdog) ne touche pas EN, donc pas l'extendeur, qui garde ses sorties. Le firmware doit écrire « tout coupé » dans l'extendeur **au tout début du boot**, et dans la séquence d'arrêt avant un redémarrage volontaire.
- **Sorties** : push-pull par défaut, sortie haute = VCCP = 3,3 V, compatible avec les entrées ON des TPS22965 (VIH 1,1 V).
- **Consommation** : ~1 µA au repos [DS].

---

## 5. Tableau des GPIO (ESP32-S3-MINI-1-N8) — brochage optimisé pour le routage

État au reset d'après le *Schematic Checklist* d'Espressif : **IE** = entrée activée, **WPU** = pull-up faible, **WPD** = pull-down faible, — = rien. RTC = utilisable en deep sleep et pour le réveil (IO0–IO21). Tactile = IO1–IO14. ADC1 = IO1–IO10 (l'ADC2 est inutilisable quand le Wi-Fi tourne).

Côtés du module (antenne en haut, au bord de la carte) : **gauche** IO0–IO11, **bas** IO12–IO18, USB, IO21, IO26, IO47, IO33, IO34, IO48 (de gauche à droite), **droite** IO35–IO42 (de bas en haut), puis UART0, IO45, IO46 et EN. Pourquoi ce brochage : §5.1.

| GPIO | Côté | v0.1 | **v2** | Sens | Reset | RTC | Tactile | Composants v2 | Remarques |
|---|---|---|---|---|---|---|---|---|---|
| IO0 | gauche | PROG (SW2) | PROG (SW2) | entrée | IE, WPU | oui | — | **+ 10 kΩ → 3V3** | strap de boot ; lu seulement après le boot (geste SW1 + PROG) |
| IO1 | gauche | GUN_LEFT | **ROTATE_X** | PWM | IE | oui | oui | 330 Ω série près de J10 | via au pied du module, puis au dos au-dessus de J16 (couloir y ≈ 91,5–93,6 mm) |
| IO2 | gauche | GUN_RIGHT | **libre** | — | IE | oui | oui | pastille | réserve : **touche tactile de réveil** |
| IO3 | gauche | SW1 | **libre** | — | IE | oui | oui | pastille de test | strap JTAG : rien qui tire la broche au reset ; ne jamais graver `STRAP_JTAG_SEL` |
| IO4 | gauche | WING_LEFT | **libre** | — | — | oui | oui | pastille de test | réserve ADC1 / tactile |
| IO5 | gauche | WING_RIGHT | **libre** | — | — | oui | oui | pastille de test | réserve ADC1 / tactile |
| IO6 | gauche | ROTATE_X | **libre** | — | — | oui | oui | pastille de test | réserve ADC1 / tactile |
| IO7 | gauche | ROTATE_Z | **libre** | — | — | oui | oui | pastille de test | réserve ADC1 / tactile |
| IO8 | gauche | HALL_LEFT | HALL_LEFT | ADC1 | — | oui | oui | 100 kΩ → GND près de J17 | au dos sous le module |
| IO9 | gauche | HALL_RIGHT | **SW1** | entrée | IE | oui | oui | R9 10 kΩ → 3V3 (suit SW1) | SW1 fermé = 330 µA |
| IO10 | gauche | I²S DIN | **HALL_RIGHT** | ADC1 | IE | oui | oui | 100 kΩ → GND près de J18 | au dos sous le module |
| IO11 | gauche | I²S BCLK | **SCL** | I²C | IE | oui | oui | R15 2,2 kΩ → 3V3 | glitch bas ~60 µs au démarrage : récupération du bus au boot (§10) |
| IO12 | bas | I²S LRCLK | **SDA** | I²C | IE | oui | oui | R16 2,2 kΩ → 3V3 | idem |
| IO13 | bas | AMP_SD | **LED verte** | sortie | IE | oui | oui | R18 1 kΩ, D4 juste en dessous | |
| IO14 | bas | NEOPIXEL_CENTER | **NEOPIXEL_RIGHT** | sortie | IE | oui | oui | 100 kΩ → GND à l'entrée de l'AHCT | au dos, sous l'USB |
| IO15 | bas | NEOPIXEL_LEFT | **IMU_INT1** | entrée | — | oui | — | — (INT1 en push-pull) | **réveil EXT1** (ANY_HIGH) ; broche XTAL_32K_P (pas de quartz 32 kHz) |
| IO16 | bas | NEOPIXEL_RIGHT | **NEOPIXEL_LEFT** | sortie | — | oui | — | 100 kΩ → GND à l'entrée de l'AHCT | broche XTAL_32K_N |
| IO17 | bas | RADAR_RX | **ROTATE_Z** | PWM | IE | oui | — | 330 Ω série près de J9 | au dos, sous l'USB |
| IO18 | bas | RADAR_TX | **NEOPIXEL_CENTER** | sortie | IE | oui | — | 100 kΩ → GND à l'entrée de l'AHCT | |
| IO19 | bas | USB D– | USB D– | — | — | oui | — | USBLC6 | ne jamais utiliser ; **aucune piste ne croise la paire USB en face avant** |
| IO20 | bas | USB D+ | USB D+ | — | — | oui | — | USBLC6 | ne jamais utiliser |
| IO21 | bas | AMP_GAIN | **PWR_FLT** | entrée | — | oui | — | R6 10 kΩ → 3V3 | interruption directe ; descend droit vers IC1 |
| IO26 | bas | bouton A | **GUN_RIGHT** | PWM | IE, **WPU** | non | — | 330 Ω série près de J4 | le pull-up du reset est sans effet : le rail des servos est coupé pendant le boot |
| IO47 | bas | AMP_GAIN_100K | **LED rouge** | sortie | IE | non | — | R12 680 Ω, D3 (à rapprocher de IO47) | |
| IO33 | bas | LED verte | **GUN_LEFT** | PWM | IE | non | — | 330 Ω série près de J3 | face avant |
| IO34 | bas | bouton B | **WING_LEFT** | PWM | IE | non | — | 330 Ω série près de J7 | face avant |
| IO48 | bas | LED rouge | **RADAR_RX** | entrée | IE | non | — | 1 kΩ série près de J5 | face avant |
| IO35 | droite | SDA | **RADAR_TX** | sortie | IE | non | — | 1 kΩ série près de J5 | face avant ; passer en entrée avant de couper le radar |
| IO36 | droite | SCL | **WING_RIGHT** | PWM | IE | non | — | 330 Ω série près de J8 | face avant |
| IO37 | droite | IMU_INT1 | **AMP_SD** | sortie | IE | non | — | R10 2 kΩ série | pas de pull-down externe (100 kΩ interne à l'ampli) ; **ampli coupé (IO37 bas) avant tout `i2s.end()`** |
| IO38 | droite | PWR_FLT | **AMP_GAIN** | drain ouvert | IE | non | — | — | **jamais à l'état haut** |
| IO39 | droite | libre | **I²S DIN** | sortie | IE | non | — | — | via au pied du module, puis au dos vers l'ampli |
| IO40 | droite | libre | **AMP_GAIN_100K** | drain ouvert | IE | non | — | R11 100 kΩ | **jamais à l'état haut** |
| IO41 | droite | libre | **I²S BCLK** | sortie | IE | non | — | — | |
| IO42 | droite | libre | **I²S LRCLK** | sortie | IE | non | — | — | |
| IO43 | droite | U0TXD (J16) | U0TXD (J16) | sortie | IE, WPU | non | — | — | console de secours |
| IO44 | droite | U0RXD (J16) | U0RXD (J16) | entrée | IE, WPU | non | — | — | |
| IO45 | droite | non connecté | non connecté | — | IE, WPD | oui | — | — | strap : ne jamais utiliser |
| IO46 | droite | non connecté | non connecté | — | IE, WPD | oui | — | — | strap : ne jamais utiliser |

**Extendeur TCAL6416 (au dos, à côté de l'IMU)** : P00 `SERVO_EN`, P01 `LED_EN`, P02 `RADAR_EN`, P03 `HALL_EN_N` ; P04–P17 libres (§4.9).

**Réveil du deep sleep** : EXT1 sur IO15 (IMU, ANY_HIGH) ; minuterie RTC ; option tactile sur IO2.

**Contraintes respectées** : capteurs Hall sur l'ADC1 ; IMU_INT1 et la touche de réserve sur des broches RTC (et tactile pour la touche) ; lignes de gain et `AMP_SD` sur des broches sans pull-up au reset (et sans glitch au démarrage pour le gain) ; aucune commande active à l'état haut sur IO26 ; IO0, IO19, IO20, IO43–IO46 inchangées.

### 5.1 Pourquoi ce brochage : l'étude de routage

**Ce que montre la v1** (relevé de `Turret2.kicad_pcb`) :

- presque toutes les destinations (servos, capteurs Hall, ampli, IMU, AHCT, radar) sont **à droite** du module, qui est dans le coin en haut à gauche, antenne au bord ;
- pourtant les servos sortaient du **côté gauche** (IO1–IO7) et faisaient le tour : 106 mm de piste pour WING_LEFT, 70 mm au dos pour ROTATE_X et ROTATE_Z, 2 vias chacun ;
- les quatre broches les mieux placées (IO39–IO42, côté droit) étaient **inutilisées** ;
- une vingtaine de pistes passaient **au dos sous le module**, entre la rangée de pastilles, les vias de masse du pad central et J16 (le connecteur UART0, au dos sous le côté droit du module) : c'est ce couloir qui bouchonnait ;
- les signaux avaient 1 à 5 vias chacun (5 pour HALL_LEFT et I²S DIN).

**Méthode** (outil `Turret2_portable/tools/gpio_study/`) : chaque ligne est modélisée par le chemin le plus simple depuis sa pastille :

- **face avant en ligne droite** quand c'est possible ;
- sinon **via au pied du module puis face arrière**, avec un second via si l'arrivée est un CMS de face avant.

Le modèle compte les croisements sur une même face, les vias, la longueur, les passages par J16 ou la zone d'antenne, et les bus éclatés. Un recuit simulé cherche le brochage qui minimise le tout, en respectant les contraintes ci-dessus. Les nouvelles pièces sont à des positions supposées : switches, P-MOS, extendeur, IMU et ampli au dos à l'endroit de la v0.1.

| Brochage | Croisements (même face) | Vias | Longueur | Passages bloqués (J16, antenne) | Lignes sous le module | Bus éclatés (trous) |
|---|---|---|---|---|---|---|
| Broches v1 reprises (plan du 21.10.2026 matin) | **150** | 32 | 1057 mm | 6 | 16 | 8 |
| Variante A, sans extendeur, optimisée | 8 | 25 | 1002 mm | 4 | 13 | **28** (ampli sur deux côtés) |
| Variante B, extendeur en face avant | 27 | 24 | 954 mm | 1 | 10 | 2 |
| **Variante C, extendeur au dos près de l'IMU (retenue)** | 22 | 25 | **939 mm** | **1** | **10** | **2** |

Pour la variante C, 16 recherches indépendantes ont abouti au même brochage, à la répartition près des broches libres. Le modèle est grossier (lignes droites, pas de vrai routeur) : il sert à **comparer** des brochages, pas à prédire le routage final.

![Avant : broches v1 reprises](img/gpio-v2-avant.png)

![Après : variante C](img/gpio-v2-apres.png)

*Rouge = face avant, bleu = face arrière, violet = sorties de l'extendeur, gris = lignes fixes (USB, UART0, PROG, EN), cercle = croisement sur la même face.*

**Le brochage côté par côté** :

- **Côté droit**, face aux destinations : les **6 lignes de l'ampli** (IO37–IO42, broches « calmes » obligatoires pour le gain), puis le radar TX et l'aile droite, en face avant.
- **Bas, à droite de l'USB** : les destinations du quart inférieur droit en éventail sur la face avant, **sans croisement** (canons, aile gauche, radar RX), plus PWR_FLT qui descend droit vers l'eFuse et la LED rouge.
- **Bas, à gauche de l'USB** : I²C, LED verte, NeoPixel, IMU_INT1, ROTATE_Z. Elles plongent au dos juste sous le module et passent sous la paire USB.
- **Côté gauche**, le plus mal placé : seulement les **capteurs Hall** (l'ADC1 n'existe que là), SW1 (qui est à gauche), SCL, ROTATE_X et **6 broches libres** (IO2–IO7 : toutes ADC1, tactiles et RTC ; IO3 est un strap).
- **Extendeur** : les 4 commandes de rails ne sortent plus du module ; elles partent de l'extendeur, au dos, près des switches.

**Les croisements restants** sont au dos, surtout ceux des capteurs Hall (imposés à gauche) avec l'I²C et les NeoPixel. Au routage : passer les lignes Hall au ras du bord bas du module, et remonter I²C et NeoPixel en face avant dès qu'elles ont passé l'USB.

### 5.2 Règles de placement qui vont avec ce brochage

1. **Dégager le flanc droit du module** : sur la v0.1, SW3 (RESET), R14 et C13 sont juste devant IO35–IO42 (x ≈ 92–94 mm). Ces 8 broches servent toutes en v2. Mettre R14 / C13 près de la broche EN (en haut à droite) et SW3 ailleurs, sur la ligne EN (signal lent).
2. **J16 au dos sous le module** : ROTATE_X passe au-dessus (couloir de ~2 mm entre la zone d'antenne et J16). Si ce couloir manque, décaler J16 juste à droite du module : le couloir sous le module se libère.
3. **Vias de sortie au pied du module**, comme sur la v0.1 : pour les lignes qui partent au dos, entre la rangée de pastilles et le pad central, ou juste à l'extérieur.
4. **Résistances série près des connecteurs** (§4.8), pas au pied du module.
5. **AHCT** : choisir l'attribution des portes au routage (§4.6).
6. **Extendeur au dos, à côté de l'IMU** : le bus I²C va du module à l'IMU, puis à l'extendeur, puis à J11 ; sa broche RESET va à EN.
7. **Paire USB** : jamais croisée en face avant ; les lignes de IO12–IO18 passent dessous, au dos.
8. **Lignes Hall** (analogiques) à l'écart des horloges I²S : de préférence sur des faces différentes là où elles se croisent.
9. **Relancer l'étude** dès que le placement v2 existe dans KiCad (`TURRET_PCB=…`, voir le README de l'outil) : les positions des nouvelles pièces ne sont ici que des hypothèses.

---

## 6. Bus I²C

| Élément | Adresse | Présent en usage normal |
|---|---|---|
| LSM6DSOX (U6) | 0x6A | oui |
| OLED SSD1306 / SSD1309 (Qwiic) | 0x3C | non, debug seulement |
| Encodeur seesaw (Qwiic) | 0x36 | non, debug seulement |
| Extendeur TCAL6416 (au dos, près de l'IMU) | 0x20 attendu (ADDR au GND) **[à vérifier]** | oui |

- **Pull-ups** : garder une seule paire, **R15 / R16 = 2,2 kΩ vers +3V3**. 1,5 mA quand une ligne est basse ; temps de montée ≈ 0,85 × 2,2 kΩ × 50 pF ≈ 95 ns, bien sous les 300 ns du mode 400 kHz. Les modules Qwiic ont leurs propres pull-ups (souvent 10 kΩ) qui se mettent en parallèle : ≈ 1,8 kΩ, encore dans la norme (≤ 3 mA).
- **Broches** : v0.1 = IO35 / IO36 (`Wire.begin(35, 36)`) ; **v2 = SDA IO12, SCL IO11** (`Wire.begin(12, 11)`, §5). Toujours explicites, jamais les broches par défaut 8 / 9 (capteurs Hall en v0.1, HALL_LEFT et SW1 en v2).
- **Ordre du bus** (v2) : module → IMU → extendeur (tous deux au dos) → J11. Pull-ups à n'importe quel endroit du bus, une seule paire.
- **En deep sleep** : bus au repos à l'état haut, aucun courant ; l'IMU reste alimentée.
- **Qwiic** : brancher et débrancher les modules **hors tension** (un branchement à chaud peut bloquer le bus).
- **Nouveau** : deux pastilles de test SDA / SCL pour l'oscilloscope.

---

## 7. Broches flottantes et corrections reprises de la v0.1

| Point | v0.1 (relevé dans KiCad) | Risque | v2 |
|---|---|---|---|
| Entrées Hall (IO8, IO9 en v0.1 ; IO8, IO10 en v2) | aucun tirage | lecture aléatoire si débranché ; l'auto-test passe à tort (vu le 21.10.2026) | 100 kΩ → GND |
| Entrées 1A–3A de l'AHCT | aucun tirage | entrées CMOS flottantes au boot et en deep sleep : courant parasite, sorties indéfinies | 100 kΩ → GND |
| IO0 | pull-up interne seulement | démarrage en mode téléchargement sur un parasite | 10 kΩ → 3V3 |
| ON des switches | — | « do not leave floating » | 100 kΩ → GND |
| SD_MODE de l'ampli | 2 kΩ série + 100 kΩ interne | aucun | inchangé |
| IO26, IO34 (boutons retirés) | pull-ups 10 kΩ R19, R20 | — | R19, R20 retirées ; IO26 → GUN_RIGHT, IO34 → WING_LEFT (§5) |
| Broches libres v2 (IO2–IO7) | — | entrées flottantes | pastilles de test ; le firmware les configure en entrée avec pull-down interne |
| IO45, IO46 | non connectées, WPD interne | aucun | inchangé |
| LSM6DSOX INT2, OCS_Aux, SDO_Aux | non connectées, tirages internes | aucun | inchangé |
| MAX98357A NC (5, 6, 12, 13) | non connectées | aucun | inchangé |
| 4ᵉ porte de l'AHCT | entrée et /OE au GND | aucun | inchangé |
| Diodes TVS D1, D5 | **SMAJ5.0A** : VR 5,0 V, IR ≤ 800 µA, VC 9,2 V | fuite permanente jusqu'à 1,6 mA (2 diodes) | **SMAJ6.5A** : VR 6,5 V, IR ≤ 500 µA **à 6,5 V** (donc très faible à 5 V), VC 11,2 V < 20 V max de l'eFuse [DS]. **Mesurer d'abord** la fuite réelle sur la v0.1 |
| Pull-ups I²C | 2,2 kΩ | aucun | inchangé |
| Lignes PWM, UART, données LED vers les connecteurs | directes | GPIO détruite par un faux contact avec le +5V ; rétro-alimentation | résistances série (§4.8) |

---

## 8. Implantation, assemblage, tests

Reprend le §3 d'AMELIORATIONS_V2.md (IMU et ampli au verso, switches de puissance au recto, verso brasé à l'air chaud en Sn42Bi58, recto à la plaque), avec ces ajouts :

- **Switches au plus près du point d'étoile du +5V**, avant les bulks : SW_SERVO avant C9 / C18.
- **CT** collé à sa broche ; **pad thermique** de chaque WSON relié au GND par des vias de 0,2–0,3 mm, ouvertures de pochoir en damier.
- **Pas de via ni de piste sous l'IMU.** Zone d'antenne de l'ESP32 libre sur toutes les couches.
- **Sérigraphie** : broche 1 de chaque WSON, TQFN et LGA ; nom des rails commutés.
- **Points de test** : TP sur 5V_SERVO, 5V_LED, 5V_RADAR, 3V3_HALL, SDA, SCL, IMU_INT1 (en plus de VBUS, 5V, 3V3 existants).
- **Mesure du courant de veille** : un pont à souder large (ou une résistance de 0 Ω en 2512) en série sur l'entrée de l'eFuse, entouré de deux pastilles : on l'ouvre pour insérer le PPK2 ou un multimètre. Il doit supporter les 3 A des servos une fois refermé.
- **Version sur la carte** : sérigraphier « v0.2 » (la v0.1 porte « Turret V0.1 »).

---

## 9. Variante sans extendeur (A)

Retenue seulement si l'on veut éviter un circuit de plus. Les quatre commandes (`SERVO_EN`, `LED_EN`, `RADAR_EN`, `HALL_EN_N`) sortent alors de l'ESP32, sur des broches de réserve du côté gauche. Elles partent au dos sous le module, avec 100 kΩ vers GND sur chaque EN et 100 kΩ vers +3V3 sur la grille du P-MOS ; **pas sur IO26**, à cause de son pull-up au reset.

Dans le modèle (§5.1), sa meilleure version a moins de croisements (8 contre 22), mais :

- **4 passages bloqués** au lieu de 1 (J16, zone d'antenne) ;
- **13 lignes sous le module** au lieu de 10 ;
- **63 mm de piste en plus** ;
- l'**ampli sur deux côtés du module** (bus éclaté) ;
- **2 broches libres seulement** sur l'ESP32 (IO3, IO4), au lieu de 6.

La variante C garde 6 broches libres sur l'ESP32 (toutes ADC1) et 12 E/S libres sur l'extendeur. Le brochage optimisé de la variante A est reproductible avec l'outil (`python gpio_study.py A …`).

---

## 10. Firmware à prévoir

1. **`pins.h` conditionnel** (`TURRET2_V2`) avec le brochage du §5 : presque toutes les broches changent ; plus de boutons A / B. La v0.1 garde son brochage.
2. **Pilote de l'extendeur** (TCAL6416, 0x20) : écrire « tout coupé » (P00–P02 bas, P03 haut, puis configuration en sorties) **au tout début du boot**, juste après `Wire.begin(12, 11)` et une **récupération du bus I²C** (9 coups d'horloge si SDA est bloquée : IO11 / IO12 font un glitch au démarrage) ; même écriture dans la séquence d'arrêt avant un redémarrage volontaire.
3. **Module d'alimentation** : séquences du §2, switches un par un via l'extendeur, lignes de signal à l'état bas avant de couper un rail.
4. **Veille radar et veille profonde** (§1) : entrée après N minutes sans cible, ou à heure fixe ; réveil EXT1 (IMU sur IO15), minuterie, option tactile sur IO2.
5. **IMU** : matrice de passage des axes (accéléromètre **et** gyroscope) pour le montage au verso ; interruption de réveil verrouillée sur INT1.
6. **Capteurs Hall** : alimentés à la demande ; l'auto-test signale « capteur absent » quand la lecture reste à ≈ 0.
7. **Fonctions des boutons A / B** : vers la page web et PROG (tableau §2.7 d'AMELIORATIONS_V2.md). Le knob reste un outil de debug.
8. **Règles de sécurité de la v0.1 à transposer** (documentation et `CLAUDE.md`, pour la v2 seulement) :
   - « ampli coupé avant tout `i2s.end()` » porte sur **IO37** (v0.1 : IO13) ;
   - « jamais à l'état haut » porte sur **IO38 et IO40** (v0.1 : IO21 et IO47) ;
   - le bus I²C se démarre avec `Wire.begin(12, 11)` (v0.1 : 35, 36).

---

## 11. Points à valider

- [ ] Fuite réelle de D1 / D5 (SMAJ5.0A) sur la v0.1, au PPK2 : décide du remplacement par SMAJ6.5A.
- [ ] Courant de veille de l'eFuse TPS259533 (variante x3 : valeur non extraite de la datasheet).
- [ ] Modèle et consommation des capteurs Hall ; temps de démarrage après mise sous tension.
- [ ] Temps de démarrage du RD-03D après mise sous tension (option « ronde » du §1).
- [ ] Capacité d'entrée du module RD-03D et de l'anneau LED (courants d'appel).
- [ ] Courant des servos au repos, sans PWM (bilan de la veille radar).
- [ ] À l'oscilloscope, à l'allumage de SW_SERVO : pas de chute du +3V3, PWR_FLT reste haut.
- [ ] Courant total en deep sleep, à comparer au §1.1.
- [ ] Découplage du LSM6DSOX et du MAX98357A : à confirmer dans les datasheets ST et Analog Devices.
- [ ] Axes de l'IMU après retournement : vérifiés sur les 3 axes, dans les 2 sens.
- [ ] Brasage du verso : tenue du recto pendant la seconde passe.
- [ ] Bruit de commutation de l'ampli au verso : HP et Wi-Fi.
- [ ] **Relancer l'étude des GPIO** (`Turret2_portable/tools/gpio_study/`) sur le placement v2 dès qu'il existe, avant de router.
- [ ] Adresse I²C du TCAL6416 (0x20 attendu avec ADDR au GND) : figure de la datasheet.
- [ ] Flanc droit du module dégagé (SW3, R14, C13 déplacés) et passage de ROTATE_X au-dessus de J16 (§5.2).
- [ ] Après un `ESP.restart()` et après un plantage : rails bien coupés (écriture « tout coupé » au début du boot).

---

## 12. Écarts avec AMELIORATIONS_V2.md

| AMELIORATIONS_V2.md | Ce plan | Raison |
|---|---|---|
| Radar sentinelle LD2410S (§2.4) | **abandonné** | RD-03D seul (choix de l'utilisateur) ; plus besoin d'une broche de réveil pour la sentinelle |
| R6 249 kΩ → 274 kΩ (§2.2) | **TPS259533**, R4 inchangé | la résistance concernée est **R4** ; 274 kΩ couperait dès 5,25 V au pire |
| 2N7002 + `LED_OE_N` (§2.3) | /OE au GND + pull-downs sur les entrées | plus simple, même protection |
| 100 kΩ sur `AMP_SD` (§2.5) | rien | pull-down interne de l'ampli |
| `LED_EN` sur IO26 (§2.7) | IO26 = GUN_RIGHT (PWM, sans risque) ; `LED_EN` sur l'extendeur | pull-up de IO26 au reset |
| Extendeur en variante (§2.9), réveil par son INT sur IO3 | **adopté**, au dos près de l'IMU, pour les 4 commandes ; IMU_INT1 reste en direct sur IO15 | brochage refait pour le routage (§5.1) ; plus de sentinelle, donc l'INT de l'extendeur ne sert pas au réveil |
| Brochage v1 conservé hors ampli (§2.6) | **brochage entièrement refait** | 150 → 22 croisements, 1057 → 939 mm de piste dans le modèle (§5.1) |
| Capteurs Hall non traités | coupés par AO3401A | plusieurs mA permanents sinon |
| LEDs d'état « ~3 mA chacune » | ~1–2 mA (680 Ω et 1 kΩ sous 3,3 V) | calcul sur R12 / R18 |
| — | résistances série, pull-up IO0, TVS, points de test, pont de mesure | protection et mesure |

---

## Sources

- TI, [TPS22965 datasheet (SLVSBJ0F)](https://www.ti.com/lit/ds/symlink/tps22965.pdf) — brochage, CT, CIN / CL, QOD, courants.
- TI, [TPS22918 datasheet](https://www.ti.com/lit/ds/symlink/tps22918.pdf) — alternative SOT-23.
- TI, [TPS2595 datasheet (SLVSE57C)](https://www.ti.com/lit/ds/symlink/tps2595.pdf) — variantes, seuils OVLO, écrêtage, limite de courant.
- TI, [TCAL6416 datasheet](https://www.ti.com/lit/ds/symlink/tcal6416.pdf) — extendeur : brochage, RESET, VCCI / VCCP, ports en entrée à la mise sous tension.
- TI, [SN74AHCT125 datasheet](https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf) — entrées inutilisées, ICC.
- AOS, [AO3401A datasheet](https://www.aosmd.com/res/datasheets/AO3401A.pdf).
- Maxim / Analog Devices, [MAX98357A/B datasheet](https://cdn-shop.adafruit.com/product-files/3006/MAX98357A-MAX98357B.pdf) (copie Adafruit) — SD_MODE, gains, courants.
- Vishay, [SMAJ5.0A–SMAJ188CA datasheet](https://www.vishay.com/docs/88390/smaj50a.pdf) — fuite des TVS.
- Espressif, [ESP32-S3 Schematic Checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html) — état des broches au reset, EN, IO0, glitchs, domaine RTC.
- Espressif, [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) — pull-up / pull-down internes 45 kΩ, valeurs par défaut des straps.
- Projet KiCad v0.1 : `Turret2_portable/Turret2.kicad_pcb` (relevé des composants et des nets le 21.10.2026).

# Turret2 — améliorations prévues pour la v2

> **Statut : propositions.** Rien de ce document n'est encore implémenté ni testé.
> **Le plan retenu est [carte-v2-plan.md](carte-v2-plan.md)** (21.10.2026) : il fait foi là où les deux documents diffèrent (radar sentinelle abandonné, eFuse TPS259533 au lieu de R4 = 274 kΩ, pas de 2N7002, capteurs Hall commutés, **brochage entièrement refait pour le routage et extendeur de GPIO adopté** ; liste des écarts en §12 du plan). Les tableaux de GPIO de ce document (§2.6, §2.9) sont donc périmés.
> Base : carte Turret2 v0.1 (fabriquée en septembre 2026). Seules les broches I²S et `AMP_SD` de l'ampli sortent de `src/pins.h` (voir 2.5) ; le reste du brochage firmware est inchangé.
> Radar monté : **Ai-Thinker RD-03D V2** (identifié le 20.10.2026), même protocole que le HLK-LD2450 cité dans les premières versions de ce document. Firmware : `Turret_firmware/`.
> Relu le 21.10.2026 : IO3 reste à **SW1** (on garde l'interrupteur) ; IO26 et IO34 deviennent libres avec la suppression des boutons A et B (2.6, 2.7) ; fonctions des boutons à reporter (2.7). Variante avec **extendeur de GPIO I²C** en 2.9 (l'ampli ne bouge plus ; SW1 passe sur l'extendeur).

## 1. Objectif : un vrai mode veille

Sur la v0.1, tout le 5 V passe par l'eFuse IC1, qui alimente aussi l'ESP32 à travers le 3,3 V (U1). Le load switch AP22811 a été retiré pendant la conception. Résultat : impossible de couper les servos, le radar ou les LEDs sans éteindre toute la carte.

Une veille purement logicielle (servos détachés, LEDs à noir, ampli en shutdown, ESP32 en light sleep) laisse donc **environ 160–190 mA** consommés en permanence.

**Objectif v2** : passer en deep sleep sous 1 mA, avec deux sources de réveil :

- la présence d'une personne, via un radar sentinelle basse consommation ;
- un mouvement de la tourelle, via l'IMU.

### Bilan de consommation sous 5 V (estimations)

| Poste | v0.1, veille logicielle | v2, deep sleep |
|---|---|---|
| Radar RD-03D V2 | ~110 mA (manuel Ai-Thinker ; LD2450 : 120 mA moyen) | 0 (coupé) |
| 6 servos sans signal PWM | ~30–60 mA (~5–10 mA chacun, à mesurer) | 0 (coupés) |
| 13 SK6812 éteintes | ~7–13 mA | 0 (coupées) |
| MAX98357A, SD_MODE bas | < 1 µA | < 1 µA |
| ESP32-S3 via TPS631000 | ~0,3 mA (light sleep) | ~10–20 µA |
| Radar sentinelle LD2410S | — | ~0,1 mA |
| eFuse, IMU low-power, AHCT125, 3 load switches coupés | quelques centaines de µA | quelques centaines de µA |
| Modules Qwiic sur J11 (OLED, encodeur) | OLED allumé 10–20 mA, encodeur seesaw quelques mA | **plusieurs mA si J11 reste alimenté** : à couper (`QWIIC_EN`, 2.9) |
| **Total** | **~160–190 mA (≈ 0,8–1 W)** | **≈ 0,5 mA** (J11 coupé) |

Les LED d'état LED2 et LED3 ajoutent environ 3 mA chacune si elles restent allumées.

---

## 2. Modifications matérielles

### 2.1 Load switches sur le 5 V des périphériques

Ajouter trois **TPS22965**. Caractéristiques : VIN 0,8–5,7 V, 6 A en continu, 16 mΩ, 50 µA passant, 2 µA coupé, WSON-8 2×2 mm avec pad thermique. L'entrée ON est compatible avec une logique 3,3 V.

| Switch | Alimente | Commande |
|---|---|---|
| SW_SERVO | branche servos (6 connecteurs) et son bulk | `SERVO_EN` → IO12 |
| SW_LED | alimentation de toutes les NeoPixels (anneau + canons) | `LED_EN` → IO13 |
| SW_RADAR | radar RD-03D (J5) | `RADAR_EN` → IO37 |

Câblage commun aux trois switches :

- **VIN** sur le +5 V, en aval du point d'étoile, sur la branche concernée. La séparation physique servos / logique de la v0.1 est conservée. Le bulk des servos reste où il est, mais passe sur le net commuté : le switch doit se trouver entre le point d'étoile et ce bulk.
- **VBIAS** sur le +5 V. Le Ron de 16 mΩ est spécifié pour VBIAS = 5 V.
- **ON** relié à la GPIO, avec un **pull-down de 100 kΩ**. Chaque switch reste ainsi coupé au boot, pendant un reset et en deep sleep. Avantage annexe : les servos ne sursautent plus à la mise sous tension.
- **CT** dimensionné pour la rampe de démarrage (voir ci-dessous).
- **Pad thermique** relié au GND par des vias. Dissipation à 3 A : 3² × 0,016 ≈ 0,14 W.
- Condensateurs d'entrée et de sortie selon la datasheet.

**Rampe à l'allumage.** Le courant d'appel dans le bulk en aval vaut environ :

```
I ≈ C_aval × 5 V / tR
exemple : 470 µF × 5 V / 2 ms ≈ 1,2 A
```

Ce courant doit rester nettement sous la limite réglée sur l'eFuse (broche ILM), car l'ESP32 partage la même source. Si la limite est dépassée, l'eFuse coupe et la carte redémarre. Choisir CT dans le tableau tR de la datasheet pour obtenir une rampe de quelques millisecondes.

**Protection.** Le TPS22965 n'a ni limite de courant ni coupure thermique : la protection reste assurée par l'eFuse en amont. Le **TPS22975**, compatible broche à broche, ajoute une coupure thermique si on la souhaite.

### 2.2 Seuil de surtension de l'eFuse

Le TPS22965 accepte au plus 5,7 V en entrée. Or l'OVLO actuel coupe à 6,02 V (R3 = 1 MΩ, R6 = 249 kΩ). Entre ces deux valeurs, les switches seraient hors spécification.

**R6 : 249 kΩ → 274 kΩ**, ce qui donne 1,2 × (1000 + 274) / 274 ≈ **5,58 V**. Il reste de la marge au-dessus des 5,25 V qu'un chargeur USB peut légitimement fournir.

### 2.3 Isolation des lignes de données NeoPixel

Le SN74AHCT125 (IC2) reste sur le 5 V non commuté. Deux problèmes apparaissent quand SW_LED est ouvert :

- ses sorties ne doivent pas piloter des LEDs non alimentées, sinon elles les rétroalimentent par leurs diodes de protection ;
- en deep sleep, les GPIO de l'ESP32 passent en haute impédance et laissent les entrées de l'AHCT125 flottantes. Sur une porte CMOS, une entrée flottante peut faire passer un courant de l'ordre du mA, ce qui suffirait à ruiner le budget de veille.

**/OE commandé par `LED_EN`, sans GPIO supplémentaire :**

- Les entrées /OE des trois portes utilisées sont reliées à un net `LED_OE_N`, avec un pull-up de 10 kΩ vers **+3V3**. Pas vers +5 V : 3,3 V suffit à dépasser le VIH de l'AHCT (2 V).
- Un **2N7002** sert d'inverseur : grille sur `LED_EN`, drain sur `LED_OE_N`, source au GND.
- `LED_EN` haut : le rail LED s'allume et les sorties sont actives. `LED_EN` bas ou flottant : rail coupé, sorties en haute impédance.

**Pull-downs de 100 kΩ sur les trois entrées de l'AHCT** (`NEOPIXEL_CENTER/LEFT/RIGHT`, côté ESP32), si la v0.1 n'en a pas déjà. Ils évitent les entrées flottantes en deep sleep et au boot.

### 2.4 Radar sentinelle LD2410S

Le radar principal (RD-03D) fournit les coordonnées x/y utilisées pour viser, il reste donc indispensable. On ajoute un radar de présence très basse consommation qui veille à sa place.

**HLK-LD2410S** : alimentation 3,0–3,6 V, environ 0,1 mA à 1 Hz de rafraîchissement, détection jusqu'à ~8 m en mouvement et ~4 m en présence statique, champ de ±60°, sortie GPIO de présence.

- **Alimentation** : sur +3V3, à travers un P-MOS high-side logic-level (ex. AO3401A).
- **Grille du P-MOS sur `RADAR_EN`**, qui porte déjà le pull-down de 100 kΩ du SW_RADAR. Les deux radars sont donc toujours en alternance, sans GPIO supplémentaire :
  - `RADAR_EN` bas ou flottant (veille) : RD-03D coupé, sentinelle allumée ;
  - `RADAR_EN` haut (actif) : RD-03D allumé, sentinelle coupée. Cela évite une éventuelle interférence entre les deux radars 24 GHz.
- **Cavalier à souder** entre la grille et le GND, ouvert par défaut. Fermé, il force la sentinelle allumée en permanence, pour tester les interférences avec les deux radars actifs.
- **Sortie présence** : `SENTINEL_OUT` → IO11, qui est une GPIO RTC (voir 2.5).
- **Connecteur** : JST-SH. TX et RX y sont sortis pour une configuration ponctuelle avec un adaptateur USB-UART, mais **ne sont pas reliés à l'ESP32**. Le brochage est à reprendre de la fiche du module.

### 2.5 Sources de réveil et déplacement de l'ampli

Sur l'ESP32-S3, **seules IO0 à IO21 peuvent réveiller la puce du deep sleep**. Sur la v0.1, `IMU_INT1` est sur IO37 : elle peut réveiller depuis le light sleep, mais pas depuis le deep sleep. Toutes les GPIO RTC sont occupées.

L'ampli libère les GPIO RTC dont on a besoin : l'I²S passe par la matrice GPIO et fonctionne sur n'importe quelle broche.

- **I²S et `AMP_SD`** passent sur IO39 à IO42, qui sont libres depuis le retrait du JTAG.
- **Réveil** : `IMU_INT1` → IO10, `SENTINEL_OUT` → IO11.
- **Commandes** : `SERVO_EN` → IO12, `LED_EN` → IO13, `RADAR_EN` → IO37.
- **IO3** reste à **SW1** (mode debug, et geste de remise à zéro SW1 + PROG) : on garde l'interrupteur. C'est un strap JTAG : ne jamais graver l'eFuse `STRAP_JTAG_SEL`. Le gain reste piloté (IO21 / IO47).

Réveil en **EXT1**, masque IO10 | IO11, mode `ANY_HIGH`. INT1 du LSM6DSOX est actif haut par défaut. La polarité de la sortie du LD2410S est à vérifier.

**Ampli en deep sleep.** `AMP_SD` est désormais sur une GPIO non RTC. On ajoute un **pull-down de 100 kΩ côté ESP32**, avant la résistance série de 2 kΩ. Quand la broche est flottante, SD_MODE est tiré à l'état bas et l'ampli reste en shutdown, sans dépendre d'un maintien logiciel de la broche.

**Firmware.** Les définitions I²S et `AMP_SD` de `src/pins.h` doivent être modifiées pour cette carte. Il vaut mieux le faire par une définition conditionnelle spécifique à Turret2 que par une modification en dur.

### 2.6 Récapitulatif des GPIO modifiées

| GPIO | v0.1 | v2 |
|---|---|---|
| IO10 | `DIN` (I²S) | `IMU_INT1` (réveil RTC) |
| IO11 | `BCLK` (I²S) | `SENTINEL_OUT` (réveil RTC) |
| IO12 | `LRCLK` (I²S) | `SERVO_EN` |
| IO13 | `AMP_SD` | `LED_EN` |
| IO37 | `IMU_INT1` | `RADAR_EN` |
| IO39 | libre | `BCLK` |
| IO40 | libre | `LRCLK` |
| IO41 | libre | `DIN` |
| IO42 | libre | `AMP_SD` (+ pull-down 100 kΩ) |

L'attribution IO39 à IO42 peut être permutée librement selon le routage.

**Deviennent libres** : **IO26 et IO34**, libérées par la suppression des boutons A et B (voir 2.7). Ce ne sont pas des GPIO RTC (pas de réveil du deep sleep, pas de tactile) : elles conviennent à des sorties de commande ou à des entrées lues en marche. Exemple d'usage : `LED_EN` sur IO26 pour libérer IO13 (RTC) pour l'INT du knob (2.7). IO26 n'est libre que parce que le module est un N8 sans PSRAM. **IO3 n'est pas libre** : elle garde SW1.

| GPIO | v0.1 | v2 |
|---|---|---|
| IO26 | bouton A (J12) | libre (ou `LED_EN`, voir 2.7) |
| IO34 | bouton B (J13) | libre |
| IO3 | SW1 | SW1 (inchangé) |

### 2.7 Suppression des boutons A et B

Les boutons sur J12 (IO26) et J13 (IO34) disparaissent. L'interface locale passe par un **knob I²C** sur le connecteur Qwiic (J11), avec éventuellement une touche tactile. Tout le reste se fait par l'interface web. SW1 et SW2 (PROG) restent.

**Le firmware actuel prend déjà en charge un encodeur I²C** (Q1, 19.10.2026 : Adafruit seesaw 4991 / 5752 à 0x36, ou Soldered 333188 ; aucune adresse en conflit avec l'IMU 0x6A ni l'écran 0x3C), avec les touches `cw`, `ccw`, `push`, `hold` dans les menus.

**Fonctions des boutons A et B à reporter** (README du firmware, §7) :

| Aujourd'hui | Proposition v2 |
|---|---|
| A : cycle de démo (depuis `Idle`, tourelle debout) | appui long sur le knob en mode normal, et bouton de la page web (existe) |
| B court : sourdine | appui court sur le knob en mode normal ; la rotation règle déjà le volume |
| B maintenu 3 s : Wi-Fi marche / arrêt | appui long sur PROG en marche (sans SW1) ; commande `wifi` |
| A + B maintenus au démarrage : réglages d'usine | **déjà doublé** par SW1 fermé + PROG 5 s (fait le 19.10.2026) |
| A / B dans les menus du mode debug | rotation / appui du knob (fait) ; à défaut, la page web (onglet Tests, boutons A / B) |

Sans knob branché, il reste la page web, la console USB et SW1 + PROG : la carte n'est jamais inaccessible.

**Retirés** : J12, J13, leurs résistances série de 1 kΩ, R19, R20 (rappels 10 kΩ) et C11, C12 (100 nF). Cela libère de la place au verso, à l'endroit même où vont les nouveaux composants.

**Réveil par le knob : pas nécessaire.** Quiconque s'approche pour tourner le knob est déjà détecté par la sentinelle, qui réveille la carte. Si on veut tout de même que le knob réveille l'ESP32, il faut deux choses :

- sortir sa ligne INT, absente du connecteur Qwiic standard (4 points), sur un point de plus ;
- la relier à une GPIO RTC. Il suffit de déplacer `LED_EN` de IO13 vers IO26, ce qui libère IO13. L'INT d'un knob I²C est en général active basse à drain ouvert : prévoir un pull-up et un réveil séparé de l'EXT1 (`ANY_HIGH`) de l'IMU et de la sentinelle.

**Touche tactile.** Sur l'ESP32-S3, le tactile n'existe que sur IO1 à IO14, **toutes occupées** (IO3 garde SW1 ; IO26 et IO34 ne sont pas tactiles). Une touche passe donc par un **MPR121 sur le Qwiic** (Q3 du plan d'améliorations, ruban de cuivre comme électrode), sans GPIO.

**Secours sans réseau.** Sans boutons, une configuration Wi-Fi ratée laisse la carte inaccessible. SW2 (BOOT, IO0) peut servir de bouton utilisateur une fois le firmware démarré : un appui long pourrait par exemple relancer le point d'accès de configuration. Encore faut-il que SW2 reste accessible dans le boîtier ; sinon, prévoir le même mécanisme avec un appui long sur le knob. **Existe déjà** : SW1 fermé + PROG maintenu 5 s remet tous les réglages d'usine, Wi-Fi compris (point d'accès « Portal Turret » rétabli), sans écran ; vérifié sur la carte le 19.10.2026.

### 2.8 Composants ajoutés et retirés

| Qté | Composant | Rôle |
|---|---|---|
| 3 | TPS22965 (WSON-8 2×2) | SW_SERVO, SW_LED, SW_RADAR |
| 3 | condensateur CT | rampe des switches |
| 3 + 3 | condensateurs d'entrée et de sortie des switches | selon datasheet |
| 3 | 100 kΩ | pull-downs sur `SERVO_EN`, `LED_EN`, `RADAR_EN` |
| 1 | AO3401A (SOT-23) | coupure de la sentinelle |
| 1 | cavalier à souder | sentinelle forcée allumée |
| 1 | 2N7002 (SOT-23) + 10 kΩ | inverseur `LED_EN` → `LED_OE_N` |
| 3 | 100 kΩ | pull-downs sur les entrées de l'AHCT125 (si absents) |
| 1 | 100 kΩ | pull-down sur `AMP_SD` |
| 1 | JST-SH | connecteur LD2410S |
| — | R6 : 249 kΩ → 274 kΩ | seuil OVLO de l'eFuse |
| −2 | J12, J13 | boutons A et B retirés |
| −2 | 1 kΩ série, R19, R20, C11, C12 | entourage des boutons retiré |

### 2.9 Variante : extendeur de GPIO I²C (proposée le 21.10.2026)

**Idée** : mettre les signaux lents sur un extendeur I²C au lieu de réaffecter des GPIO de l'ESP32. Seule la sortie INT de l'extendeur a besoin d'une GPIO RTC pour le réveil. L'ampli peut alors **rester sur IO10–IO13** (pas de déplacement de l'I²S, moins de routage à reprendre). Les sections 2.5 et 2.6 décrivent la variante sans extendeur.

**Composant conseillé : PCAL6416A** (NXP / TI, 16 E/S, 1,65–5,5 V, adresse 0x20 ou 0x21 selon ADDR) :

- broche **RESET** (voir « Piège du reset ») ;
- rappels internes programmables (haut ou bas, ~100 kΩ) : pas de résistances externes sur les entrées ;
- **mémorisation des entrées** (input latch) : une impulsion courte de l'IMU ou de la sentinelle n'est pas perdue, et le registre d'état d'interruption dit qui a réveillé la carte ;
- ~1 µA au repos.

Alternative plus courante : MCP23017 (RESET aussi, pas de mémorisation des impulsions). Aucune adresse en conflit sur le bus : IMU 0x6A, OLED 0x3C, encodeur seesaw 0x36, MPR121 0x5A.

**Ce qui va sur l'extendeur, et ce qui n'y va pas**

| Signal | Sur l'extendeur ? | Raison |
|---|---|---|
| `SERVO_EN`, `LED_EN`, `RADAR_EN` | oui (sorties) | lents ; l'état est tenu pendant le deep sleep de l'ESP32 |
| `QWIIC_EN` (nouveau) | oui (sortie) | coupe le 3,3 V de J11 en veille (voir « Consommation ») |
| `SENTINEL_OUT`, `IMU_INT1` | oui (entrées, interruption) | réveil par l'INT de l'extendeur ; `IMU_INT1` peut rester aussi sur IO37 pour la lecture rapide en marche |
| SW1 | oui (entrée, rappel interne haut) | lu au démarrage et en marche ; libère IO3 |
| PWM des servos, NeoPixels, I²S, UART radar, Hall (ADC) | **non** | trop rapides ou analogiques |
| `PWR_FLT` | **non** | doit rester une interruption directe |
| `AMP_SD` | **non** (reste IO13) | l'ampli doit pouvoir être coupé avant un reset même si le bus I²C est bloqué |

**Réveil** : INT de l'extendeur (drain ouvert, actif bas) → **IO3**, avec rappel vers le 3,3 V (R9, déjà présente pour SW1). Réveil EXT0 sur IO3 au niveau bas. Au démarrage, lire le registre d'état d'interruption pour savoir si c'est l'IMU ou la sentinelle. IO3 est un strap JTAG : sans effet tant que l'eFuse `STRAP_JTAG_SEL` n'est pas gravé (ne jamais le graver). Au reset, INT est haut (pas d'interruption en attente après le reset de l'extendeur).

**Piège du reset.** Quand l'ESP32 redémarre (plantage, watchdog, brownout), ses GPIO passent en haute impédance et les pull-downs de 100 kΩ coupent les switches. **Un extendeur garde ses sorties** : sans précaution, les servos resteraient alimentés pendant le redémarrage. Donc :

1. **RESET de l'extendeur relié à EN (CHIP_PU) de l'ESP32** : un reset matériel remet ses broches en entrées, et les **pull-downs de 100 kΩ sur `*_EN` restent indispensables** ;
2. `ESP.restart()` ne touche pas EN : le firmware écrit « tout coupé » dans l'extendeur **au tout début du démarrage**, avant toute autre chose.

**Consommation** :

| Source | Courant | Quand |
|---|---|---|
| PCAL6416A | ~1 µA | toujours |
| pull-down 100 kΩ d'un `*_EN` à l'état haut | 33 µA chacun | carte éveillée seulement (EN bas en veille) |
| SW1 fermé, rappel 100 kΩ | 33 µA | mode debug seulement |
| INT, rappel 10 kΩ | 330 µA pendant l'interruption | quelques ms au réveil |
| **modules Qwiic sur le 3,3 V de J11** | **plusieurs mA** (OLED allumé 10–20 mA, encodeur seesaw quelques mA) | **même en deep sleep** si rien ne les coupe |

L'extendeur lui-même est négligeable ; le vrai risque pour le budget de 0,5 mA est **J11**. D'où `QWIIC_EN` : P-MOS (AO3401A) entre le 3,3 V et J11, grille tirée vers le GND par 100 kΩ (Qwiic **allumé par défaut** au démarrage, l'écran et l'encodeur en ont besoin), l'extendeur met la grille à l'état haut pour couper en veille. Avant de couper : passer l'OLED en power save, puis ne plus rien envoyer sur le bus vers J11. Vérifier que les modules coupés ne tirent pas SDA / SCL vers le bas par leurs diodes de protection (sinon, petit buffer / switch I²C, ou couper seulement les modules gourmands).

**Récapitulatif des GPIO avec l'extendeur**

| GPIO | v0.1 | v2 avec extendeur |
|---|---|---|
| IO3 | SW1 | **INT de l'extendeur** (réveil EXT0) |
| IO10–IO13 | I²S + `AMP_SD` | inchangé |
| IO26, IO34 | boutons A, B | libres |
| IO37 | `IMU_INT1` | inchangé (et copie sur l'extendeur) |
| IO39–IO42 | libres | libres |
| extendeur | — | `SERVO_EN`, `LED_EN`, `RADAR_EN`, `QWIIC_EN`, SW1, `SENTINEL_OUT`, `IMU_INT1`, 9 E/S de réserve |

**Composants en plus** : PCAL6416A (TSSOP-24) + 100 nF ; AO3401A + 100 kΩ pour `QWIIC_EN`. **En moins** par rapport à 2.5 : pas de déplacement de l'ampli ni du pull-down de `AMP_SD`. Les pull-downs de 100 kΩ sur `SERVO_EN`, `LED_EN`, `RADAR_EN` restent.

**Firmware** : petit pilote I²C (registres de sortie, de configuration, de rappels, masque et état d'interruption, mémorisation des entrées : adresses à prendre dans la datasheet) ; écriture « tout coupé » en phase 1 du démarrage (avant les réglages) ; SW1 lu par I²C (`Wire.begin(35, 36)` doit donc précéder la lecture de SW1).

---

## 3. Implantation : nouveaux composants en face arrière

Le recto manque de place. Plutôt que de mettre les load switches au verso, ce qui ferait traverser la carte deux fois au courant des servos, on **déplace au verso l'IMU et le MAX98357A**. La place ainsi libérée au recto accueille les switches de puissance, qui restent dans le chemin du +5 V sans aucun via.

| Face | Composants |
|---|---|
| **Recto** | SW_SERVO et SW_LED, avec leur CT et leurs condensateurs, à la place de l'IMU et de l'ampli |
| **Verso** | MAX98357A et son découplage, IMU (U6), SW_RADAR, AO3401A, 2N7002, pull-downs, connecteur LD2410S |

La place libérée par J12 et J13, qui étaient déjà au verso, accueille une partie de ces composants.

**Recto : switches de puissance**

- **SW_SERVO et SW_LED** au plus près du point d'étoile du +5 V et du départ de leur branche. Le chemin de puissance reste entièrement au recto.
- **Condensateur CT** collé à la broche CT de son switch.
- **SW_RADAR** peut aller au verso : 120 mA, deux vias suffisent.

**Verso : MAX98357A**

- **Découplage** : 10 µF et 100 nF déplacés avec lui, au ras de ses broches d'alimentation. Le bulk peut rester au recto, au pied du via d'alimentation.
- **Alimentation** : ~0,7 A crête. Prévoir deux ou trois vias, contre plusieurs rangées pour les servos.
- **Sorties HP** : courtes et en paire jusqu'à J15. Si J15 reste au recto, une paire de vias rapprochés suffit.
- **Pad thermique** (boîtier TQFN) au GND par des vias.
- Le plan GND interne s'intercale entre l'ampli classe D et l'ESP32, ce qui est plutôt favorable.

**Verso : IMU**

- **Loin de la chaleur** (MAX98357A, TPS631000, switches) et **loin des contraintes mécaniques** : vis M2, connecteurs sur lesquels on tire. Le centre de la carte est l'idéal.
- **Axes inversés** : au verso, la puce est tournée de 180° autour d'un axe du plan de la carte. Z s'inverse, ainsi que X ou Y selon le sens du retournement. Il faut le corriger dans le firmware (section 4).

**Contraintes communes**

- **Zone d'antenne de l'ESP32** : la zone d'exclusion vaut pour toutes les couches. Aucun composant au verso sous l'antenne.
- **AO3401A** près du connecteur LD2410S, **2N7002** près de l'AHCT125.
- **Encombrement** : vérifier la hauteur des composants du verso par rapport au boîtier de la tourelle et aux vis M2 (H1/H2). Des connecteurs sont déjà au verso.
- Les liaisons I²S, I²C, `AMP_SD` et `IMU_INT1` traversent par des vias simples : ce sont des signaux lents.

**Assemblage**

La carte devient double face, ce qui change la méthode de brasage à la plaque chauffante.

- **Recto en premier**, à la plaque chauffante, avec la pâte habituelle.
- **Verso ensuite, à l'air chaud**, idéalement avec une **pâte basse température** (Sn42Bi58, fusion à 138 °C). Le recto ne refond pas pendant cette seconde passe.
- **Ne jamais remonter la plaque à la température de refusion** pour le verso : les composants du recto, tournés vers la plaque, refondraient.
- **Préchauffer.** Les pads thermiques reliés aux plans GND de la carte 4 couches pompent la chaleur. Sans préchauffage, il faut chauffer longtemps au pistolet et le recto finit par souffrir. La plaque peut servir de préchauffeur, entre 100 et 120 °C, carte posée sur des entretoises. C'est bien en dessous de la fusion de la soudure du recto.
- **Tout le verso en une seule fois.** Passer la pâte au pochoir sur toute la face, poser tous les composants, puis chauffer zone par zone. Ainsi on ne repose jamais de pâte à côté de joints déjà faits, qui refondent dès 138 °C.
- **Ordre** : l'ampli en premier (grosse masse thermique), l'IMU en dernier, pour qu'elle subisse le moins de cycles de chauffe. Débit d'air faible, sinon les petits composants s'envolent. Ruban Kapton sur les connecteurs plastiques voisins.
- **Connecteurs du verso en pâte standard ou au fer.** Le Sn42Bi58 est plus cassant : il convient aux circuits intégrés, mais pas aux connecteurs JST qui subissent les efforts des câbles.
- **Vias dans les pads thermiques** : diamètre de 0,2 à 0,3 mm et ouvertures de pochoir en damier, pour limiter la soudure aspirée par les vias.
- Prévoir un **second pochoir** pour le verso, avec la même astuce de centrage sur H1/H2.
- **Boîtiers au verso** : TQFN pour l'ampli, LGA pour l'IMU, au pas de 0,5 mm. Ils passent bien à l'air chaud avec un pochoir. La pâte basse température ménage aussi le MEMS de l'IMU.
- **Sérigraphie** : marquer clairement la broche 1 de chaque boîtier sans pattes (WSON, TQFN, LGA). Ils sont petits et presque symétriques.

---

## 4. Firmware

Il faut ajouter un module d'alimentation au firmware (`Turret_firmware/`). En plus des broches de l'ampli (voir 2.5), il gère les nouvelles lignes de commande. Les boutons A et B disparaissent de `pins.h` pour cette carte : leurs fonctions passent au knob, à PROG et à la page web (tableau en 2.7).

**Orientation de l'IMU.** L'IMU étant au verso, ses axes ne correspondent plus à ceux de la v0.1 (voir section 3). Il faut appliquer une correction d'axes dans la classe `Motion`. Le réglage actuel `ImuUpAxis` (choix de l'axe « haut » parmi ±X, ±Y, ±Z) ne suffit pas : il faut une permutation / inversion des trois axes de l'accéléromètre **et** du gyroscope, par exemple une définition de compilation propre à la carte v2. Pour la valider, poser la carte à plat, composants du recto vers le haut : Z doit lire +1 g après correction. Puis basculer la carte sur chaque bord pour vérifier X et Y.

**Entrée en veille**, par exemple après N minutes sans cible détectée par le RD-03D :

1. Détacher les servos (arrêt du PWM).
2. Mettre les LEDs à noir, puis `LED_EN` à l'état bas (cela coupe le rail et passe les sorties de l'AHCT en haute impédance).
3. Mettre `SERVO_EN` à l'état bas.
4. Mettre `AMP_SD` à l'état bas.
5. Configurer l'IMU en low-power / wake-up, interruption routée sur INT1.
6. Mettre `RADAR_EN` à l'état bas : le RD-03D est coupé et la sentinelle s'allume.
7. `esp_sleep_enable_ext1_wakeup((1ULL << 10) | (1ULL << 11), ESP_EXT1_WAKEUP_ANY_HIGH)`, puis `esp_deep_sleep_start()`.

**Réveil.** Le deep sleep repart d'un reset : la RAM est perdue et l'USB-CDC se reconnecte.

1. Configurer les trois sorties NeoPixel à l'état bas **avant** d'activer `LED_EN`.
2. Mettre `SERVO_EN` à l'état haut et attendre la fin de la rampe.
3. Mettre `RADAR_EN` à l'état haut : la sentinelle se coupe. Attendre le démarrage du RD-03D.
4. Mettre `LED_EN` à l'état haut.
5. Reprendre le fonctionnement normal.

Ordre important : allumer les switches un par un, jamais tous ensemble, pour ne pas cumuler les courants d'appel sur l'eFuse.

---

## 5. Points à valider sur prototype

- [ ] Courant réel des servos au repos, sans signal PWM.
- [ ] Temps de démarrage du RD-03D après mise sous tension (délai avant la première cible).
- [ ] Interférences entre le LD2410S et le RD-03D quand les deux sont actifs (cavalier fermé).
- [ ] Rail qui alimente les NeoPixels des canons (branche servos ou branche logique). Cela détermine quel switch les coupe.
- [ ] Variante exacte du TPS22965 en stock : le suffixe change le comportement de décharge de la sortie.
- [ ] Polarité de la sortie de présence du LD2410S et configuration nécessaire.
- [ ] À l'oscilloscope, à l'activation de `SERVO_EN` : le +3V3 ne doit pas chuter et `PWR_FLT` doit rester à l'état haut.
- [ ] Courant total mesuré en deep sleep, à comparer à l'estimation de la section 1.
- [ ] Brasage du verso : tenue du recto pendant la seconde passe à l'air chaud.
- [ ] Axes de l'IMU après retournement : correction appliquée et vérifiée sur les trois axes.
- [ ] Audio : absence de bruit de commutation classe D dans le HP et sur le Wi-Fi, avec l'ampli au verso.
- [ ] Knob I²C : modèle retenu, adresse I²C sans conflit avec le LSM6DSOX (0x6A), et décision sur son INT (réveil ou non).
- [ ] Mécanisme de secours sans réseau (appui long sur BOOT ou sur le knob) et accessibilité de SW2 dans le boîtier.
- [x] Le firmware lit-il les boutons A et B ? Oui (démo, sourdine, Wi-Fi, réglages d'usine, menus) : report prévu en 2.7.
- [ ] Reporter dans le firmware les fonctions de A et B (tableau 2.7) avant de recevoir la carte v2.
- [ ] Choisir entre la variante 2.5 (ampli déplacé sur IO39–IO42) et la variante 2.9 (extendeur PCAL6416A, ampli inchangé).
- [ ] Variante 2.9 : après un plantage ou un `ESP.restart()`, les switches sont bien coupés (RESET relié à EN + écriture « tout coupé » au démarrage).
- [ ] Courant de J11 en veille, modules branchés et coupés par `QWIIC_EN` ; pas de fuite par SDA / SCL vers un module non alimenté.

## Références

- [TPS22965 — page produit TI](https://www.ti.com/product/TPS22965) · [datasheet (SLVSBJ0)](https://www.ti.com/lit/pdf/slvsbj0)
- [HLK-LD2450 — manuel (même protocole que le RD-03D monté)](https://www.tinytronics.nl/product_files/006000_HLK-LD2450-Instruction-Manual.pdf)
- [HLK-LD2410S — fiche produit](https://openelab.com/products/hilink-hlk-ld2410s-presence-radar)
- [HLK-LD2410 — manuel V1.02](https://www.elemon.com.ar/images/industry_news/11-HLK-LD2410-user-manual-V1.02.pdf)

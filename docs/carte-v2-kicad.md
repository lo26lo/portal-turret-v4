# Carte Turret2 v2 — guide pas à pas dans KiCad

> **Rôle** : faire la carte v2 dans KiCad, étape par étape. Le **quoi** et le **pourquoi** sont dans [carte-v2-plan.md](carte-v2-plan.md) (les « §… » renvoient à ce plan) ; ici, le **comment** et **dans quel ordre**.
>
> Rédigé le 22.10.2026 pour **KiCad 10.0** (10.0.6 installé), à partir du projet v0.1 du dépôt (`Turret2_portable/`). Les références de composants, les noms de nets et les numéros de broches cités ici sont relevés dans ce projet (netlist exportée avec `kicad-cli` le 22.10.2026).

---

## Comment s'en servir

- **Une étape à la fois, dans l'ordre.** Chaque phase finit par un **point de contrôle** : enregistrer, lancer l'ERC (ou le DRC), faire un commit (`v2 : étape N — …`), puis me dire « étape N faite ».
- **À chaque point de contrôle, je vérifie sans ouvrir KiCad** : `kicad-cli` exporte la netlist et lance l'ERC / le DRC ; je compare au plan et je te dis ce qui manque ou ce qui est faux. On ne passe à la suite que quand c'est propre.
- **Utiliser les noms de nets de ce guide** (5V_SERVO, SERVO_EN, etc.) : mes vérifications s'appuient dessus.
- **Ce que je peux faire pour toi** : préparer le symbole du TPS22965 (étape 2.0), vérifier chaque point de contrôle, relancer l'étude des GPIO sur ton placement (étape 5.4), tenir le plan et le journal à jour.

| Phase | Où | Contenu | Taille |
|---|---|---|---|
| 0 | dépôt, KiCad | copie de travail v2, état de départ | petite |
| 1 | schéma | retraits, valeurs, résistances simples | moyenne |
| 2 | schéma | nouvelle feuille « Rails » : 3 switches, P-MOS, extendeur ; rails branchés | grosse |
| 3 | schéma | nouveau brochage de l'ESP32 | moyenne, **délicate** |
| 4 | schéma | finitions : points de test, annotation, empreintes, nomenclature | moyenne |
| 5 | PCB | mise à jour, placement, verso, étude des GPIO | grosse |
| 6 | PCB | routage, DRC, sérigraphie | grosse |
| 7 | fabrication | fichiers, contrôle final | moyenne |

**Pourquoi cet ordre** : tout le schéma d'abord, pour ne mettre le PCB à jour qu'une fois ; dans le schéma, les changements faciles en premier ; les nouveaux circuits **avant** le rebrochage de l'ESP32, pour que tous les nets existent quand on pose les labels ; sur le PCB, placer, vérifier l'étude des GPIO, **puis** router.

---

## Phase 0 — Préparer

### 0.1 Choisir où vit la v2 (décision)

**Recommandé : un dossier `Turret2_v2/` dans le dépôt**, copie de `Turret2_portable/` :

- versionné : une étape = un commit, retour en arrière facile ;
- autonome (librairies `Turret_*` dans `library/`) : je peux vérifier chaque étape ;
- la v0.1 reste intacte dans `Turret2_portable/`, pour les cartes actuelles.

**Avant la copie** : rafraîchir `Turret2_portable/` depuis le projet maître (son PCB porte encore R18 = 1 kΩ), puis marquer la v0.1 dans git (`git tag hw-v0.1`).

**Sur le PC du projet maître** (choix du 22.10.2026) : cloner le dépôt sur ce PC et créer `Turret2_v2/` **à partir du projet maître** (pas de la copie portable, qui peut être en retard). Si le projet maître utilise des librairies globales, copier celles qui servent dans `library/`, comme pour la copie portable, pour que le dossier soit autonome. Pousser à chaque point de contrôle : je récupère les fichiers sur GitHub pour les vérifier. Une session Claude sur ce PC peut aussi le faire, si KiCad 10 y est installé (`kicad-cli`).

*Autre possibilité* : travailler dans le projet maître, hors dépôt, et recopier à chaque point de contrôle. Plus lourd, et je ne vois que les copies.

### 0.2 Créer la copie

- Copier le dossier dans l'Explorateur → `Turret2_v2/`, **sans** `Turret2-backups/`. Garder les noms de fichiers (`Turret2.kicad_pro`, `mcu.kicad_sch`…) : les différences restent lisibles dans git.
- Ne pas versionner les sauvegardes automatiques de KiCad pour la v2 (`Turret2_v2/Turret2-backups/` dans `.gitignore`) : un zip par sauvegarde alourdit vite le dépôt.
- Ouvrir `Turret2_v2/Turret2.kicad_pro` dans KiCad 10 ; vérifier qu'aucune librairie ne manque.

### 0.3 État de départ

- **ERC** (Inspecter > Contrôle des règles électriques) : la v0.1 donne **28 avertissements, aucun bloquant** :
  - 18 « librairie de symboles '' absente » : symboles importés de LCSC, intégrés au schéma ;
  - 5 « symbole différent de la copie en librairie » (SMAJ5.0A × 2, MAX98357A, TC2050-IDC, ESP32-S3-MINI-1) ;
  - 4 « broche non connectée reliée » sur IO39–IO42 : croix au bout des fils JTAG, supprimées à l'étape 3 ;
  - 1 pad de l'ampli (type « non spécifié ») relié à GND.

  À chaque point de contrôle, seuls les **nouveaux** avertissements comptent.
- **Cartouche** de chaque feuille (Fichier > Paramètres de la page) : révision « v0.2 », date.

**Point de contrôle 0** : commit « v2 : étape 0 — copie de la v0.1 ».

---

## Phase 1 — Schéma : retraits, valeurs, résistances simples

Le schéma a trois feuilles : **racine** (`Turret2.kicad_sch` : USB-C, TVS, eFuse, TPS631000), **MCU** (`mcu.kicad_sch` : ESP32, IMU, ampli, AHCT, LEDs, connecteurs des servos) et **IO** (`untitled.kicad_sch` : les autres connecteurs).

### 1.1 Retirer

| Feuille | À supprimer | Remarque |
|---|---|---|
| MCU | R13, R17 (1 kΩ série), R19, R20 (pull-ups), C11, C12 (100 nF), et leurs fils jusqu'à IO26 / IO34 | les broches restent libres, reprises à l'étape 3 |
| IO | J12, J13 (boutons A et B) | |
| MCU | J2 (TC2050-IDC) | relié à rien dans la netlist et absent du PCB ; à garder seulement si tu comptes t'en servir |

Les labels JTAG_* et les croix au bout des fils de IO39–IO42 partent à l'étape 3 : les fils resteront pour l'I²S.

### 1.2 Changer des valeurs

| Réf. | v0.1 | v2 | Attention |
|---|---|---|---|
| IC1 | TPS259573DSGT | **TPS259533DSGR** | même symbole, même empreinte : changer la valeur et les champs fabricant / Mouser (§4.2) |
| R7 | 56 kΩ | **511 kΩ 1 %** | symbole importé de LCSC : changer aussi « Manufacturer Part » et « Supplier Part », sinon la nomenclature commande encore la 56 kΩ (§4.10) |
| R8 | 10 kΩ | **91 kΩ 1 %** | idem |
| D1, D5 | SMAJ5.0A | inchangées pour l'instant | SMAJ6.5A seulement après la mesure de fuite (§11) ; même empreinte |

### 1.3 Ajouter les résistances « simples »

Elles se branchent sur des nets dont le nom ne change pas : on peut les poser dès maintenant. Pour une résistance **en série**, couper le fil entre le label et la broche du connecteur et l'y insérer ; le label reste côté ESP32.

| Ajout | Net | Où | Plan |
|---|---|---|---|
| 10 kΩ → +3V3 | S3PRG (IO0) | MCU, près de SW2 | §4.7 |
| 100 kΩ → GND × 2 | HALL_LEFT, HALL_RIGHT | IO, près de J17 / J18 | §4.8 |
| 100 kΩ → GND × 3 | NEOPIXEL_CENTER, NEOPIXEL_LEFT, NEOPIXEL_RIGHT | MCU, entrées 1A–3A de IC2 | §4.6 |
| 330 Ω série × 3 | sorties de IC2 : 1Y → J14.2, 2Y → J3.3, 3Y → J4.3 | MCU, à la sortie de IC2 | §4.6 |
| 330 Ω série × 6 | GUN_LEFT → J3.2, GUN_RIGHT → J4.2, WING_LEFT → J7.3, WING_RIGHT → J8.3, ROTATE_Z → J9.3, ROTATE_X → J10.3 | MCU, près des connecteurs | §4.8 |
| 10 kΩ → GND | GUN_RIGHT, côté ESP32 de sa 330 Ω | MCU | §5 |
| *option* 100 kΩ → GND × 5 | ROTATE_X, ROTATE_Z, GUN_LEFT, WING_LEFT, WING_RIGHT | à côté des 330 Ω ; attribut DNP (« ne pas monter ») tant que la place n'est pas confirmée | §4.8 |
| 1 kΩ série × 2 | RADAR_TX → J5.2, RADAR_RX → J5.3 | IO, près de J5 | §4.8 |
| 0 Ω 2512 (≥ 4 A) | entre VBUS (USB-C, J1, D1, D5, TP1) et l'entrée de IC1 (broches 3, 4) | racine ; nommer le côté eFuse `VBUS_IN`, et y mettre R3 (diviseur OVLO) | §8 |

**Point de contrôle 1** : ERC (rien de nouveau), commit. Je vérifie : pièces retirées absentes, valeurs, chaque nouvelle résistance sur les bons nets.

---

## Phase 2 — Schéma : nouvelle feuille « Rails »

### 2.0 Les symboles

| Pièce | Symbole | Empreinte |
|---|---|---|
| TPS22965 × 3 | **absent des librairies de KiCad 10** : je le prépare dans une librairie du projet (`library/symbols/Turret_v2.kicad_sym`), ou tu le dessines (ci-dessous) | celle de IC1 : `Turret_Package_SON:Texas_DSG0008A_WSON-8-1EP_2x2mm_P0.5mm_EP0.9x1.6mm_ThermalVias` (même boîtier TI « DSG » ; vérifier le pad central dans le dessin mécanique de la datasheet du TPS22965) |
| Extendeur | `Interface_Expansion:PCAL6416APW` (même brochage TSSOP-24 que le TCAL6416 [DS]) ; valeur = la pièce achetée (TCAL6416PWR ou PCAL6416APW) | `Package_SO:TSSOP-24_4.4x7.8mm_P0.65mm` |
| P-MOS | `Transistor_FET:AO3401A` | `Package_TO_SOT_SMD:SOT-23` |

Si tu dessines le TPS22965 : broches 1 et 2 VIN (entrée d'alimentation), 3 ON (entrée), 4 VBIAS (entrée d'alimentation), 5 GND (entrée d'alimentation), 6 CT (passive), 7 et 8 VOUT (**sortie d'alimentation** : l'ERC voit alors les rails comme alimentés), 9 = pad thermique (GND).

### 2.1 Créer la feuille

Sur la feuille racine : Placer > Feuille hiérarchique, nom « Rails », fichier `rails.kicad_sch`. Dedans :

- symboles d'alimentation existants : +5V, +3V3, GND ;
- **labels globaux** existants : SDA, SCL, S3En (EN de l'ESP32) ;
- **labels globaux nouveaux** : 5V_SERVO, 5V_LED, 5V_RADAR, 3V3_HALL ;
- **labels locaux** (internes à la feuille) : SERVO_EN, LED_EN, RADAR_EN, HALL_EN_N.

### 2.2 SW_SERVO (un circuit à la fois)

TPS22965 et autour (§4.1, valeurs §3.1) :

| Broche | Raccordement |
|---|---|
| VIN (1, 2) | +5V, **1 µF** au ras |
| VBIAS (4) | +5V, **100 nF** |
| ON (3) | SERVO_EN, **100 kΩ → GND** |
| CT (6) | **4,7 nF 25 V X7R** → GND |
| VOUT (7, 8) | 5V_SERVO |
| GND (5), pad | GND |

### 2.3 SW_LED et SW_RADAR

Copier-coller le bloc SW_SERVO deux fois, puis changer : **CT 1 nF** ; ON → LED_EN / RADAR_EN ; VOUT → 5V_LED / 5V_RADAR ; **10 µF** sur chaque sortie.

### 2.4 P-MOS des capteurs Hall (AO3401A)

Source → +3V3 ; drain → 3V3_HALL ; grille → HALL_EN_N, avec **100 kΩ grille → +3V3** (coupé par défaut, §4.3).

### 2.5 Extendeur

| Broche | Raccordement |
|---|---|
| 2 VDD_I2C, 24 VDD_P | +3V3, **100 nF** chacune, au ras |
| 12 VSS | GND |
| 22 SCL, 23 SDA | SCL, SDA (pull-ups existants R15 / R16, rien de plus) |
| 3 ~RESET | S3En |
| 21 ADDR | **GND en direct** (adresse 0x20 [DS]) |
| 1 ~INT | croix « non connecté », ou point de test |
| 4 P0_0 | SERVO_EN |
| 5 P0_1 | LED_EN |
| 6 P0_2 | RADAR_EN |
| 7 P0_3 | HALL_EN_N |
| 8–11, 13–20 | croix « non connecté » ; quelques points de test si tu veux |

Les pull-downs des ON (étapes 2.2, 2.3) et le pull-up de la grille (2.4) sont déjà posés.

### 2.6 Brancher les charges sur les rails

| Rail | Passent de +5V / +3V3 au rail | Feuille |
|---|---|---|
| 5V_SERVO | J3.1, J4.1, J7.2, J8.2, J9.2, J10.2 ; C9, C18 (220 µF) ; C10, C19, C20, C21, C22, C23 (100 nF des connecteurs) | MCU |
| 5V_LED | J14.1 ; C30 | IO |
| 5V_RADAR | J5.1 ; C29 | IO |
| 3V3_HALL | J17.1, J18.1 ; C31, C32 | IO |

**Restent sur +5V** : sortie de IC1, U1 (TPS631000), U9 (ampli) avec C24, C25, C26, IC2 (AHCT), C3, C4, C8, TP2, TP3.

Sur la feuille MCU, le +5V est dessiné à partir de **trois symboles +5V** et de fils : repérer la branche qui dessert les connecteurs de servos et leurs condensateurs, la séparer du reste (AHCT, ampli) et y mettre le label global 5V_SERVO.

**Point de contrôle 2** : ERC. Si l'ERC signale une entrée d'alimentation non alimentée sur un rail, poser un PWR_FLAG sur ce rail (ou vérifier que VOUT est bien une sortie d'alimentation dans le symbole). Commit. Je vérifie : la liste exacte de chaque rail, ce qui reste sur +5V, les broches de l'extendeur, les tirages.

---

## Phase 3 — Schéma : nouveau brochage de l'ESP32

**Délicat** : deux labels identiques sur deux broches relient deux fonctions entre elles, et l'ERC ne le voit pas toujours. D'où trois passes, et ma vérification par la netlist à la fin.

### 3.1 Effacer

Sur U7 (feuille MCU), effacer les labels de toutes les GPIO, **sauf** : IO0 (S3PRG), USB D– / D+, TXD0 / RXD0, EN (S3En), et les croix de IO45 / IO46. Garder les bouts de fil. Effacer aussi les labels JTAG_TCK / TDO / TDI / TMS et les **4 croix** au bout des fils de IO39–IO42.

### 3.2 Poser les labels v2

Mêmes noms qu'aujourd'hui (l'autre bout de chaque net ne bouge pas) ; type : **G** = label global, **L** = label local de la feuille MCU, comme sur la v0.1.

| Broche du module | GPIO | Label | Type | | Broche du module | GPIO | Label | Type |
|---|---|---|---|---|---|---|---|---|
| 5 | IO1 | ROTATE_X | L | | 25 | IO21 | PWR_FLT | G |
| 6–11 | IO2–IO7 | libres (3.3) | — | | 26 | IO26 | GUN_RIGHT | L |
| 12 | IO8 | HALL_LEFT | G | | 27 | IO47 | FaultLed (LED rouge, R12) | L |
| 13 | IO9 | JTAG_SEL (SW1) | L | | 28 | IO33 | GUN_LEFT | L |
| 14 | IO10 | HALL_RIGHT | G | | 29 | IO34 | WING_LEFT | L |
| 15 | IO11 | SCL | G | | 30 | IO48 | RADAR_RX | G |
| 16 | IO12 | SDA | G | | 31 | IO35 | RADAR_TX | G |
| 17 | IO13 | ESP32Debug (LED verte, R18) | L | | 32 | IO36 | WING_RIGHT | L |
| 18 | IO14 | NEOPIXEL_RIGHT | L | | 33 | IO37 | AMP_SD | L |
| 19 | IO15 | IMU_INT1 | L | | 34 | IO38 | AMP_GAIN | L |
| 20 | IO16 | NEOPIXEL_LEFT | L | | 35 | IO39 | DIN | L |
| 21 | IO17 | ROTATE_Z | L | | 36 | IO40 | AMP_GAIN_100K | L |
| 22 | IO18 | NEOPIXEL_CENTER | L | | 37 | IO41 | BCLK | L |
| | | | | | 38 | IO42 | LRCLK | L |

*Facultatif* : renommer JTAG_SEL en DEBUG_SW, aux deux bouts (SW1 et IO9) : SW1 ne sert plus au JTAG.

### 3.3 Broches libres IO2–IO7

Un point de test sur chacune (symbole et empreinte comme TP1–TP4 : `Turret_TestPoint:TestPoint_Pad_D1.0mm`), sinon une croix « non connecté ». IO2 : pastille de la future touche tactile de réveil.

**Point de contrôle 3** : ERC (les 4 croix de IO39–IO42 ont disparu), commit. Je vérifie par la netlist : chaque fonction sur **une seule** broche de U7, la bonne ; plus rien sur les anciennes broches ; aucune broche de U7 partagée entre deux nets.

---

## Phase 4 — Schéma : finitions

1. **Points de test** (§8) sur 5V_SERVO, 5V_LED, 5V_RADAR, 3V3_HALL, SDA, SCL, IMU_INT1.
2. **Annoter** (menu Outils) en **conservant les annotations existantes** : seules les nouvelles pièces reçoivent un numéro, la v0.1 garde les siens.
3. **Empreintes** des nouvelles pièces (Outils > Assigner les empreintes) :

   | Pièce | Empreinte |
   |---|---|
   | TPS22965 × 3 | celle de IC1 (étape 2.0) |
   | Extendeur | `Package_SO:TSSOP-24_4.4x7.8mm_P0.65mm` |
   | AO3401A | `Package_TO_SOT_SMD:SOT-23` |
   | R et C 0603 | comme la v0.1 : `Turret_Resistor_SMD:R_0603_1608Metric`, `Turret_Capacitor_SMD:C_0603_1608Metric` |
   | 10 µF | `Turret_Capacitor_SMD:C_0805_2012Metric` (comme C15, C25) |
   | 0 Ω 2512 | `Resistor_SMD:R_2512_6332Metric` |
   | Points de test | `Turret_TestPoint:TestPoint_Pad_D1.0mm` |

4. **Champs pour la nomenclature** : valeur et référence fabricant de chaque nouvelle pièce ; attribut DNP sur les pull-downs PWM optionnels si tu n'as pas tranché. Au passage, **des références de la v0.1 ne collent pas** :
   - **C27** : valeur 1 µF, mais référence fabricant CL05B104KO5NNNC, qui est un **100 nF** en 0402 : à trancher ;
   - R1, R2, R8, C14, C17 : références LCSC en **0402**, alors que leurs empreintes sont en **0603**.

   Sans importance si tu soudes d'après les valeurs ; à corriger si la nomenclature sert à commander.
5. **ERC final** : il ne doit rester que les avertissements de départ (étape 0.3), moins les 4 croix.
6. **Nomenclature** : je la compare à la liste d'achat (§3.1).

**Point de contrôle 4** : commit et `git tag sch-v0.2` : schéma v2 terminé.

---

## Phase 5 — PCB : mise à jour et placement

### 5.1 Mettre à jour le PCB depuis le schéma (F8)

- Cocher « Supprimer les empreintes sans symbole » : J12, J13, R13, R17, R19, R20, C11, C12 s'en vont (J2 n'a jamais été sur le PCB). H1, H2 (trous de fixation) et le logo sont marqués « carte uniquement » : ils restent. **Relire la liste dans la boîte de dialogue avant de valider.**
- Les nouvelles empreintes arrivent à côté de la carte.

### 5.2 Faire de la place

Presque toutes les GPIO changent de broche : les pistes de signal de la v0.1 relient maintenant les mauvais pads, le DRC les signale. Le plus simple :

- **Garder** : contour, H1 / H2, **connecteurs à leur place** (les câbles de la tourelle en dépendent), **module ESP32 à sa place** (antenne au bord), la zone USB-C / eFuse / TPS631000, la paire USB, les plans de masse.
- **Supprimer** les pistes des nets de signal qui ont changé, puis router à neuf. Le menu contextuel d'une piste permet de sélectionner tout son net. Autre méthode : verrouiller ce qu'on garde, puis Modifier > Suppressions globales en excluant les éléments verrouillés (vérifier l'option dans la boîte de dialogue).

### 5.3 Placer (§5.2, §8)

1. **Au verso** (retourner l'empreinte) : U6 (IMU) et ses condensateurs ; U9 (ampli) avec C24, C25, R10, R11 ; à leur place de la v0.1, comme l'a supposé l'étude des GPIO.
2. **Extendeur** au verso, à côté de l'IMU ; **SW_RADAR** au verso, à côté de J5.
3. **SW_SERVO et SW_LED** au recto, au point d'étoile du +5V, avant C9 / C18 ; CT collé à sa broche.
4. **AO3401A** au recto, entre J17 et J18.
5. **Dégager le flanc droit du module** : SW3, R14, C13 ailleurs (R14 / C13 près de la broche EN).
6. Résistances série et pull-downs **près des connecteurs** ; 330 Ω des NeoPixel près de IC2.
7. **0 Ω 2512** entre D1 / D5 et IC1 ; R7 / R8 collées à la broche FB du TPS631000 (§4.10).
8. Points de test.

### 5.4 Relancer l'étude des GPIO (moi)

Je relance `Turret2_portable/tools/gpio_study/` sur ton placement (`TURRET_PCB=Turret2_v2/Turret2.kicad_pcb`). S'il trouve nettement mieux pour quelques broches, on les change dans le schéma (étape 3.2), puis F8.

**Point de contrôle 5** : commit. Je vérifie la face et la position de chaque pièce par rapport au plan, et je te donne le résultat de l'étude.

---

## Phase 6 — PCB : routage

Dans cet ordre (§4, §5.2) :

1. **Puissance** : VBUS → 0 Ω → IC1 → point d'étoile +5V → SW_SERVO → 5V_SERVO, au recto, en pistes larges, **sans via** sur le chemin des servos ; SW_LED ; SW_RADAR (deux vias vers le verso) ; +3V3 et 3V3_HALL. Pads thermiques des WSON : vias de 0,2–0,3 mm vers la masse.
2. **Nœud FB du TPS631000** : court, loin de LX1 / LX2 (§4.10).
3. **Paire USB** : intouchée ; aucune piste ne la croise au recto.
4. **I²S, AMP_SD, gain** (flanc droit → ampli au verso) ; **I²C** (module → IMU → extendeur → J11).
5. **Lignes de servos, NeoPixel, UART du radar, capteurs Hall** (analogiques : à l'écart des horloges I²S), LEDs, PWR_FLT.
6. **Commandes des rails** (extendeur → ON des switches) : lentes, n'importe quelle face.
7. **Zones** : remplir à nouveau ; vias de couture autour des nouvelles pièces.
8. **DRC** (Inspecter > Contrôle des règles de conception), avec les règles du projet (`Turret2.kicad_dru`) : 0 erreur, 0 connexion manquante.
9. **Sérigraphie** : « v0.2 » à la place de « Turret V0.1 » ; SW1 « DEBUG » ; noms des rails près des points de test ; broche 1 de chaque WSON, TQFN et LGA.

**Point de contrôle 6** : commit. Je lance le DRC avec `kicad-cli` et je relis les connexions manquantes et la sérigraphie.

---

## Phase 7 — Fabrication

1. **Vue 3D** : les deux faces ; hauteur des pièces du verso par rapport au support de la tourelle (des connecteurs y sont déjà).
2. **Fichiers** : Gerber et perçages ; nomenclature ; fichier de position si l'assemblage est fait par le fabricant ; **deux pochoirs** (recto et verso, §3.1).
3. **Revue finale avec moi** : je reprends la netlist et le PCB contre le plan, point par point (liste d'achat, découplage, brochage, tirages, faces).
4. `git tag hw-v0.2`.

---

## Pièges connus

- **Deux labels identiques sur deux broches de U7** : deux fonctions reliées, souvent sans avertissement de l'ERC (étape 3).
- **Symboles importés de LCSC** : changer la valeur ne change pas la référence commandée (R7, R8, et les écarts de l'étape 4.4).
- **F8 et « Supprimer les empreintes sans symbole »** : H1, H2 et le logo sont « carte uniquement », donc gardés ; relire quand même la liste.
- **ADDR de l'extendeur** : directement à GND ou à VCCP, jamais par une résistance [DS].
- **Module ESP32 et connecteurs** : ne pas les déplacer (antenne au bord, longueur des câbles de la tourelle).
- **Rails de la v0.1** : trois symboles +5V sur la feuille MCU, reliés par des fils ; une branche oubliée laisse un servo sur le +5V permanent. Ma vérification du point de contrôle 2 le voit.

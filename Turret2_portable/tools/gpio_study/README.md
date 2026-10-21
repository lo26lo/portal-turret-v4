# Étude du brochage ESP32 pour le routage (carte v2)

Outil écrit le 21.10.2026 pour choisir les GPIO de la carte v2 avant le routage. Résultat et explications : [docs/carte-v2-plan.md](../../../docs/carte-v2-plan.md), §5.

## Ce que fait l'outil

`gpio_study.py` lit le PCB KiCad : module ESP32, connecteurs et composants. Pour chaque signal, il trace le chemin le plus simple depuis la pastille du module jusqu'à sa destination :

- **ligne droite sur la face avant** quand c'est possible ;
- sinon **via au pied du module, puis ligne droite sur la face arrière**, avec un second via si l'arrivée est un CMS de la face avant.

Il compte ensuite les croisements sur une même face, les vias, la longueur, les passages par J16 ou par la zone d'antenne, et les bus éclatés (ampli, NeoPixel, I²C, radar). Enfin, il cherche par recuit simulé le brochage qui minimise tout cela.

Il respecte les contraintes matérielles de l'ESP32-S3 :

- **ADC** : ADC1 sur IO1–IO10 seulement ;
- **réveil et tactile** : réveil sur IO0–IO21, tactile sur IO1–IO14 ;
- **reset** : pull-up de IO26 au reset ;
- **lignes de gain de l'ampli** : uniquement sur des broches sans pull-up ni glitch au démarrage ;
- **broches fixes** : IO0, IO19, IO20, IO43–IO46.

C'est un **modèle grossier** : des lignes droites, pas de vrai routeur. Il sert à comparer des brochages entre eux, pas à prédire le routage final.

## Variantes

| | Description |
|---|---|
| A | toutes les commandes sortent de l'ESP32 |
| B | extendeur I²C en face avant, entre les switches |
| C | **extendeur I²C au dos, à côté de l'IMU** (variante retenue) |

Les positions des pièces nouvelles ou déplacées sont des hypothèses : dictionnaire `POS` et fonctions `Bk()` (IMU et ampli au dos) en tête de `gpio_study.py`. **À mettre à jour dès que le placement v2 existe**, puis relancer l'étude.

## Utilisation

```
python gpio_study.py eval C final          # coût du brochage retenu
python gpio_study.py eval A v1             # coût des broches v1 reprises telles quelles
python gpio_study.py C 8 60000 essai       # nouvelle recherche (8 départs), écrit route_essai.json
python gpio_draw.py C route_essai.json essai.png "titre"    # dessin (Pillow : pip install pillow)
```

Pour étudier un autre fichier (le PCB v2 une fois placé) : définir `TURRET_PCB=chemin\vers\fichier.kicad_pcb`. Les références des composants (U7, J3…) doivent rester les mêmes, sinon il faut adapter `BASE_NETS`.

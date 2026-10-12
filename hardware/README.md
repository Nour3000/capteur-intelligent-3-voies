# Matériel

![Vue 3D du PCB](pcb_3d.jpg)

- `capteur_PCB.pdsprj` : schéma et PCB (Proteus 8.17)
- `gerbers/capteur_PCB_gerbers.zip` : Gerber X2, perçages, netlist IPC-D-356
- `BOM.csv` : nomenclature

Carte de 100 × 80 mm, deux couches, FR4 1,6 mm. Pistes de 0,5 mm pour les signaux et de 1 mm pour l'alimentation, 0,3 mm d'écart minimum, plan de masse sur la face inférieure, quatre trous de fixation de 3 mm.

## Connexions

| Net | Broches |
|---|---|
| +5V | J1-1, C1, C2, C3+, C4+, J2-J4 VCC, J5-1, J6A-2 (VDD), R8 → J6B-7 (rétroéclairage), J7-2, J9B-8 (5V BluePill), U1-4, RV1, R7 → LED |
| GND | J1-2, condensateurs, J2-J4 GND, J5-3, J6A-1, J6A-5 (RW), J6B-8, J7-1, J8-3, J9B-9, J10B-9, J10B-10, R4-R6, RV1, U1-11, U1-12 |
| TRIG 0° / 120° / 240° | J2-2 / J3-2 / J4-2 → PB0 / PB1 / PB10 |
| ECHO 0° | J2-3 → R1 (10 k) → U1-3 et R4 (15 k) → GND ; U1-1 et U1-2 → PA0 |
| ECHO 120° | J3-3 → R2 → U1-5 et R5 → GND ; U1-6 et U1-7 → PA1 |
| ECHO 240° | J4-3 → R3 → U1-10 et R6 → GND ; U1-8 et U1-9 → PA2 |
| Quatrième ampli | U1-12 → GND, U1-13 relié à U1-14 |
| LM35 | J5-2 → PA3 |
| LCD | contraste J6A-3 ← curseur de RV1 ; RS PA4, E PA5, D4 PA6, D5 PA7, D6 PB6, D7 PB7 |
| Servo | J7-3 → PB8 |
| UART | J8-1 PA9, J8-2 PA10 |

J9 est la barrette BluePill côté B12, J10 la barrette côté VB. J6A et J6B sont les broches 1–8 et 9–16 du LCD.

Avec 3,0 V en entrée, le LM324 alimenté en 5 V reste dans sa plage d'entrée (jusqu'à environ 3,5 V) et sa sortie ne peut pas dépasser la tension admise par le STM32.

## Avant fabrication

Imprimer la couche cuivre à l'échelle 1:1 et poser la BluePill dessus : B12 doit tomber sur J9-1 et VB sur J10-1. Vérifier aussi que le potentiomètre acheté a ses trois broches en triangle, curseur décalé.

Ne pas brancher l'USB de la BluePill en même temps que l'alimentation 5 V externe. Prévoir une alimentation d'au moins 2 A pour le servo.

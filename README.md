# Capteur intelligent de distance à trois voies

Un capteur classique donne une valeur et s'arrête là. Un capteur **intelligent** sait aussi :
- corriger ce qui fausse sa mesure ;
- dire quand il se trompe ou quand il est en panne ;
- se recalibrer ;
- économiser son énergie ;
- dialoguer avec le système qui l'utilise.

Ce projet part de là. Il a été réalisé dans le cadre du module *Capteurs intelligents et chaînes de mesure* (ENSIT, 3ᵉ année ETA). Le but était de partir d'un besoin réel et de concevoir un capteur complet : du choix du capteur jusqu'au bilan d'incertitude.

Notre besoin : un petit robot d'intérieur doit savoir à tout moment **où est l'obstacle le plus proche et à quelle distance**. La carte lit trois HC-SR04 placés à 0°, 120° et 240° autour d'un STM32F103 (BluePill). Elle oriente un servo vers l'obstacle le plus proche.

![Schéma Proteus pendant la simulation](docs/images/simulation_schema.png)

Le circuit a été simulé sous Proteus et la chaîne de mesure étudiée sous MATLAB. Le PCB est dessiné mais la carte n'a pas encore été fabriquée : les chiffres ci-dessous viennent de la simulation.

## Ce qui rend ce capteur « intelligent »

| Fonction | Ce que fait la carte |
|---|---|
| **Compenser une grandeur d'influence** | La vitesse du son dépend de la température (0,18 %/°C). Un LM35 la mesure et la distance est corrigée à chaque cycle. Sans cela, l'erreur atteint 3,7 cm à 1 m entre 0 et 40 °C. |
| **Valider ses mesures** | Une mesure qui saute trop loin de la précédente est rejetée et remplacée par la moyenne des bonnes valeurs. La limite tient compte du bruit du capteur et de la vitesse du robot (50 cm/s). |
| **S'auto-diagnostiquer** | Chaque voie signale si elle est hors de portée, en panne, bloquée, si sa mesure saute ou si elle est instable. |
| **S'auto-calibrer** | On pose une cible à deux distances connues, on envoie `CAL1` puis `CAL2`, et la carte recalcule sa pente et son décalage. |
| **Gérer son énergie** | Veille du processeur entre deux mesures, servo coupé au repos, et passage en mode éco (une mesure toutes les 2 s) quand rien ne bouge. |
| **Communiquer dans les deux sens** | Liaison série : la carte envoie ses mesures et accepte des commandes de réglage. |

## La chaîne de mesure

```
distance ─► HC-SR04 ─► pont 10k/15k + suiveur LM324 ─► CAN 12 bits ─┐
                                                                    ├─► STM32 ─► LCD, UART, servo
température ─► LM35 ─────────────────────────────────► CAN 12 bits ─┘
```

Le HC-SR04 renvoie une impulsion dont la **largeur** donne le temps aller-retour de l'onde. Cette impulsion sort en 5 V : un pont diviseur la ramène à 3 V, puis un LM324 en suiveur l'envoie au CAN. Le STM32 détecte les deux fronts et calcule :

```
d (cm) = t (µs) × v / 20000        v = 331,3 + 0,606 × T   (m/s, T en °C)
```

Le seuil de détection est placé **à mi-niveau** (1,5 V, soit 1861 codes). Ainsi, une erreur sur le niveau du signal (tolérance des résistances, alimentation, offset de l'ampli) décale les deux fronts de la même façon, et la largeur ne change pas. Grâce à ce choix, l'électronique de conditionnement pèse moins de 0,1 mm dans le bilan d'erreur.

![Oscilloscope : TRIG, ECHO, sortie du suiveur et PA0](docs/images/oscilloscope.png)

Voie 0° à 100 cm, 1 ms/div. A : TRIG. B : ECHO brut. C : sortie du suiveur. D : entrée PA0. Les trois dernières impulsions ont la même largeur, environ 5,8 ms, soit 100 cm × 58 µs/cm.

## Ce que l'on voit en simulation

![LCD : distances des trois voies, température et direction](docs/images/lcd.png)

Les voies sont réglées à 100, 50 et 200 cm et la direction retenue est 120°. Le LCD lit environ 0,5 % de trop, pour une raison connue : le modèle Proteus du HC-SR04 utilise une vitesse du son fixe (22 °C), alors que la carte corrige pour les 25 °C lus sur le LM35. Une calibration à deux points efface cet écart.

![LCD avec la voie 120° hors de portée](docs/images/diagnostic.png)

La voie 120° est mise hors de portée : elle affiche `--` et la carte bascule sur la voie valide la plus proche.

**Codes de défaut** (UART, entre crochets) : `H` hors de portée, `P` capteur muet, `B` entrée bloquée à l'état haut, `S` saut impossible, `I` mesure instable. Sur le LCD, un `!` apparaît en fin de ligne dès qu'une voie est en défaut.

**Commandes série** (9600 bauds) :

| Commande | Effet |
|---|---|
| `STAT` | état détaillé de chaque voie |
| `CAL1 d [voie]`, `CAL2 d [voie]` | calibration sur deux distances de référence |
| `CALRAZ` | retour au modèle théorique |
| `MODE AUTO`, `MODE NORMAL`, `MODE ECO` | stratégie énergétique |
| `PER ms` | période de mesure (200 à 10 000 ms) |

## Les chiffres

| | |
|---|---|
| Plage de mesure | 2 à 400 cm |
| Résolution du CAN | 0,806 mV, soit 0,08 °C pour le LM35 |
| Erreur à 1 m sans compensation (0 à 40 °C) | jusqu'à 3,7 cm |
| Incertitude probable à 1 m | 4,5 mm |
| Incertitude maximale à 1 m | 8,5 mm |
| Courant moyen sans gestion / avec mode éco | 76 mA / 46 mA |

Ce qui limite la précision, dans l'ordre : le HC-SR04 lui-même (3 mm), l'erreur du LM35, puis l'humidité, qui n'est pas compensée. Le détail est dans le [rapport](docs/rapport_capteur_intelligent.pdf).

## PCB

Carte de 100 × 80 mm, deux couches, composants traversants. La BluePill s'enfiche sur deux barrettes. Projet Proteus, Gerber et nomenclature dans [hardware/](hardware/).

![Vue 3D du PCB](hardware/pcb_3d.jpg)

## Brochage

| Fonction | STM32 |
|---|---|
| TRIG 0°, 120°, 240° | PB0, PB1, PB10 |
| ECHO après conditionnement | PA0, PA1, PA2 |
| LM35 | PA3 |
| LCD RS, E, D4–D7 | PA4, PA5, PA6, PA7, PB6, PB7 |
| Servo | PB8 |
| UART TX, RX | PA9, PA10 |

## Arborescence

```
firmware/capteur_intelligent/   firmware de la carte (Arduino IDE + STM32duino)
firmware/proteus/               même code pour Proteus VSM
simulation/proteus/             projet de simulation
simulation/matlab/              influence, incertitude, calibration, diagnostic, énergie
hardware/                       PCB, Gerber, nomenclature
docs/                           rapport, étude du conditionneur, captures
```

## Lancer le projet

**Simulation** : ouvrir `simulation/proteus/capteur_simulation.pdsprj`, coller `firmware/proteus/Main.ino` dans l'onglet Source Code, compiler (F7) et lancer. Les distances et la température se règlent avec les flèches des HC-SR04 et du LM35.

**Carte** : dans Arduino IDE, installer *STM32 MCU based boards*, choisir *Generic STM32F1 series* puis *BluePill F103C8*, ouvrir `firmware/capteur_intelligent/capteur_intelligent.ino` et téléverser. `SIMULATION` doit rester à 0.

**MATLAB** (testé sous R2023a) :
- `etude_capteur_intelligent.m` : grandeurs d'influence, bilan d'incertitude, calibration, diagnostic, énergie ;
- `etude_chaine_conditionnement.m` : caractéristique statique, Bode, réponse indicielle.

## À faire

- fabriquer la carte et mesurer des distances de référence à plusieurs températures ;
- tester les commandes série dans Proteus et refaire la capture du terminal : `docs/images/terminal.png` montre encore l'ancien format de trame ;
- ajouter un MOSFET pour couper les capteurs entre deux mesures : on passerait de 46 à environ 20 mA ;
- enregistrer la calibration en mémoire flash pour qu'elle survive à une coupure.

## Licence

MIT

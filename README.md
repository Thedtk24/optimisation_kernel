# Analyse et performance avec gcc : Optimisation C

Ce répertoire présente une étude approfondie en calcul haute performance (HPC) portant sur l'optimisation incrémentale d'un noyau mathématique en langage C intensif en fonctions trigonométriques.

---

## Environnement Expérimental

Les mesures de performances ont été réalisées dans l'environnement suivant :

- **Processeur** : AMD Ryzen 7 3700U (Zen+, Famille 23, Modèle 24, x86_64)
- **Fréquence / Gouverneur** : 2.30 GHz (acpi-cpufreq, performance)
- **Système** : Ubuntu (Linux noyau 6.17.0-14-generic, exécution native)
- **Compilateur** : gcc 13.3.0
- **Analyseur de Performance** : MAQAO 2026.0.0.b

<img width="663" height="439" alt="lstopo_thed" src="https://github.com/user-attachments/assets/e422c6a4-71e0-4146-bf91-0eff98d1b608" />

## Méthodologie Expérimentale

### Choix de la taille mémoire ($n$)
Pour isoler les performances matérielles au niveau des différents niveaux de cache, la taille de la matrice $n$ a été fixée mathématiquement. Sachant qu'un flottant occupe 4 octets et que le noyau manipule les tampons `a[n][n]` et `b[n]`, l'empreinte mémoire totale est de $4 \times (n^2 + n)$ octets.

Les tailles $n$ choisies pour cibler spécifiquement les caches du Ryzen 3700U sont :
- **Cache L1d (32 KiB)** : $n = 85$
- **Cache L2 (512 KiB)** : $n = 350$
- **Cache L3 (4096 KiB)** : $n = 800$

### Calibration (Warm-up)
Les métriques soulignent que l'exécution se stabilise à partir de la 4ème itération d'échauffement cache. Par mesure de fiabilité, l'échauffement a été **fixé à 15 répétitions**.

<img width="500" height="300" alt="warmup_thed" src="https://github.com/user-attachments/assets/9015f3c5-0f13-4b2f-baea-d23df66b6abd" />

## Transformations et Mesures d'Optimisation

Le code d'origine (NOOPT) passe l'immense majorité de son temps d'exécution limité par la vitesse des fonctions trigonométriques, avec un score de vectorisation MAQAO s'élevant à un modeste 37.5 % avec le flag `-O2`. Voici l'évolution algorithmique pour contrecarrer ces goulots :

### OPT1 : Localité Spatiale (Inversion de Boucles)
Dans la version originale, la boucle interne faisait varier `i`, causant des défauts de cache (memory misses) dus au fait que l'accès à un tableau `a[i][j]` n'est pas contigu.
- **Transformation** : La boucle `j` a été déplacée en interne pour y effectuer des accès séquentiels.
- **Résultat** : L'efficacité d'accès aux cases de tableaux a bondi de **87,7 % à 100 %**.

### OPT2 : Sortie d'Invariants et Passage au 32-bits (`float`)
Les calculs à base de `b[i]` étaient redondants pour chaque itération de la boucle interne `j`.
- **Transformation** : Précalcul des fonctions trigonométriques sur `b[i]` hors de la boucle interne. Remplacement des calculs de précision double `sin/cos/tan` par leurs équivalents en 32 bits `sinf/cosf/tanf`. 
- **Résultat** : Le temps perdu dans l'appel à la bibliothèque `math.h` a été diminué de 4 %. Par ailleurs, la manipulation du 32 bits peut optimiser l'utilisation des registres vectoriels (8 floats par registre 256-bits).

<img width="433" height="249" alt="Capture d’écran 2026-04-21 à 12 58 05" src="https://github.com/user-attachments/assets/c9fd671d-78c8-4756-8911-eb80fbf495ef" />

### OPT3 : Déroulage de Boucle (Unrolling)
Le modulo (`i % 4`) utilisé pour les règles trigonométriques ne dépend pas de `j`. Il bloquait les optimisations de branchement depuis l'intérieur du noyau.
- **Transformation** : Séparation des conditions hors de la boucle, puis déroulage explicite de la boucle interne `j` par des pas de 4 pour offrir plus de granularité au pipeline.
- **Résultat** : Les tests de fin de boucle et les incréments de `j` ayant dramatiquement diminué, le temps accumulé dans les boucles externes fut réduit de moitié !

### OPT4 : L'Optimisation Ultime ($O(N^2) \rightarrow O(N)$)
Même avec de l'Unrolling, la boucle `j` nécessitait encore de recalculer `b[j]` localement.
- **Transformation** : Allocation dynamique globale (`malloc`) de tableaux retenant préventivement `sinf(b)`, `tanf(b)`, `1/cosf(b)` et `1/tanf(b)`. La boucle imbriquée ne réalise alors strictement plus **aucun appel de trigonométrie**, uniquement des multiplications et affectations mémoire rapides.
- **Résultat flag `-O3`** : L'efficience est foudroyante. Combinée aux flags puissants (`-ffast-math -O3 -march=native -funroll-loops`), la boucle interne a atteint un **ratio de vectorisation exclusif de 100 %**.  Le ratio de temps passé dans les appels lents de `math.h` devient désormais complètement négligeable dans les rapports MAQAO Profiler.

### OPT5 : Optimisations Avancées (OpenMP / Multithreading)
Une version OPT5 (théorisée) est incluse pouvant inclure des directives `<omp.h>` : `#pragma omp parallel for private(i, j)`. Sur des grandes matrices (exemple $n \ge 800$), le travail de calcul préventif sera découpé par threads, permettant des gains qui échelonnent l'accélération mathématiquement au nombre disponible de cœurs physiques du Ryzen.

## Conclusion du Profiling

Le passage de NOOPT à des structures avancées a débloqué tout le potentiel sous-jacent de `gcc` :
1. Stabilisation du flux mémoire (OPT1).
2. Allégement des registres (OPT2).
3. Restructuration algorithmique radicale de la charge de calcul (OPT4).
Couplé à MAQAO, ce projet illustre finement comment compiler intelligemment (de `-O2` à `-O3 -march=native`) pour sublimer du code et exploiter formellement le multicoeur.

---

## Compilation & Exécution

*Note : La documentation détaillée du compilateur n'est pas l'objectif. Le `Makefile` est auto-porteur.*

Pour compiler la version visée :
```bash
# Remplacer OPT4 par l'optimisation souhaitée (NOOPT, OPT1, OPT2, OPT3, OPT4)
make OPT=OPT4
```

Analyses :
```bash
./calibrate <val_n> <repetitions_max> # Calcule le warmup
./measure <val_n> <warmups> <run_count>
maqao oneview -R1 -- ./measure <val_n> <warmups> <run_count> # Trace MAQAO
```

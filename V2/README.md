# ProgrammationSystemesLinux

Évaluation M1 SRC / Programmation Systèmes sous Linux / Langage C / M. Malinge

# Simulation de conquête militaire

## Présentation

Ce projet simule une conquête militaire en langage C sous Linux, avec une hiérarchie réelle de processus Unix :

- 1 armée commandée par le général
- 3 divisions
- 3 régiments par division
- 5 compagnies par régiment

Chaque structure militaire est représentée par un **processus distinct**.

Le programme repose sur :

- la **mémoire partagée System V**
- les **sémaphores System V**
- les **signaux Unix**
- la création hiérarchique de processus avec `fork()`

Le général affiche l’état global de la conquête **toutes les 10 secondes**, conformément au sujet. Le sujet impose aussi des remontées d’informations non synchronisées depuis les compagnies jusqu’au général, et des échanges par mémoire partagée. :contentReference[oaicite:2]{index=2}

---

## Objectif du projet

Chaque compagnie génère aléatoirement des informations de combat :

- morts
- blessés
- ennemis morts
- prisonniers
- avancée en kilomètres
- recul en kilomètres

Ces informations sont ensuite remontées dans la hiérarchie :

- les compagnies mettent à jour leur régiment
- les régiments agrègent les données de leurs compagnies
- les divisions agrègent les données de leurs régiments
- l’armée affiche périodiquement un état général

Cette progression en kilomètres correspond bien à l’évolution possible mentionnée dans le sujet. :contentReference[oaicite:3]{index=3}

---

## Architecture du programme

### Hiérarchie simulée

- 1 processus Armée
- 3 processus Divisions
- 9 processus Régiments
- 45 processus Compagnies

### Total

**58 processus**

---

## Organisation mémoire

La mémoire partagée contient :

- les statistiques cumulées des compagnies
- les statistiques cumulées des régiments
- les statistiques cumulées des divisions

Structure utilisée :

- `compagnies[3][3][5]`
- `regiments[3][3]`
- `divisions[3]`

---

## Synchronisation

Dans cette version, la synchronisation est plus fine que dans une version à mutex global.

### Sémaphores utilisés

- **1 sémaphore par régiment**
- **1 sémaphore par division**

Soit :

- 9 sémaphores de régiment
- 3 sémaphores de division
- **12 sémaphores au total**

### Avantage

Deux régiments différents peuvent travailler en parallèle sans se bloquer inutilement.

Cela rend l’architecture plus propre et plus proche d’une vraie logique système, tout en restant simple à comprendre.

---

## Fonctionnement détaillé

### Compagnie

Chaque compagnie :

- génère des pertes aléatoires
- ajoute ses pertes à son cumul
- met à jour la mémoire partagée protégée par le sémaphore de son régiment
- affiche son activité avec horodatage

### Régiment

Chaque régiment :

- crée ses 5 compagnies
- lit les données cumulées de ses compagnies
- agrège les pertes
- met à jour son état dans la mémoire partagée
- transmet son état à la division

### Division

Chaque division :

- crée ses 3 régiments
- lit les états des régiments
- agrège les résultats
- met à jour son état
- transmet son état à l’armée

### Armée

Le général :

- crée les 3 divisions
- lit les états des divisions
- calcule le total global
- affiche l’état de la conquête toutes les 10 secondes
- affiche aussi le classement des divisions selon leur progression nette

---

## Concepts systèmes utilisés

### `fork()`

Création hiérarchique réelle des processus.

### Mémoire partagée System V

- `shmget`
- `shmat`
- `shmdt`
- `shmctl`

Elle permet aux processus de partager le même état global.

### Sémaphores System V

- `semget`
- `semop`
- `semctl`

Ils protègent les accès concurrents aux zones partagées.

### Signaux

- `SIGINT` : arrêt propre avec `Ctrl+C`
- `SIGTERM` : terminaison des processus enfants

### Nettoyage IPC

À la fin du programme :

- suppression du segment de mémoire partagée
- suppression des sémaphores
- arrêt propre de tous les processus enfants

---

## Compilation

```bash
make clean
make
Exécution
./simulation

Pour arrêter la simulation proprement :

Ctrl+C
Nettoyage IPC manuel

Si le programme a été interrompu brutalement :

make clean_ipc

ou :

ipcrm -a
Vérifications utiles

Dans un autre terminal :

ps -ef | grep simulation
ipcs
Exemple d’affichage
=== DEBUT DE LA SIMULATION ===
Appuyez sur Ctrl+C pour arreter.

[12:17:01] Compagnie C0 R1 D2 : +morts=5 +blesses=2 +ennemis=10 +prisonniers=5 +avance=2km +recul=1km
[12:17:01] Regiment R1 (Division D2) transmet : morts=5 blesses=2 ennemis=10 prisonniers=5 net=1km
[12:17:02] Division D2 transmet a l'armee : morts=5 blesses=2 ennemis=10 prisonniers=5 net=1km

===== ETAT DU GENERAL (toutes les 10s) =====
Allies  : morts=330 blesses=513
Ennemis : morts=434 prisonniers=266
Progression : avance=117km recul=63km net=54km
===========================================

===== CLASSEMENT DES DIVISIONS =====
1) Division D2 | net=22km | morts=100 blesses=160 | ennemis=150 | prisonniers=80
2) Division D1 | net=20km | morts=115 blesses=170 | ennemis=140 | prisonniers=90
3) Division D0 | net=12km | morts=115 blesses=183 | ennemis=144 | prisonniers=96
====================================
Choix techniques
Pourquoi une mémoire partagée unique ?

Parce qu’elle simplifie l’architecture tout en restant fidèle au sujet.

Pourquoi plusieurs sémaphores ?

Pour éviter qu’un seul verrou global bloque toute la simulation.

Pourquoi une simulation cumulative ?

Parce qu’elle représente bien une conquête progressive où les pertes et la progression s’accumulent dans le temps.

Pourquoi gérer Ctrl+C proprement ?

Pour éviter :

les processus zombies
les ressources IPC orphelines
les segments de mémoire ou sémaphores laissés dans le système
Auteur

Karim GASMI
M1 Systèmes, Réseaux & Cloud Computing - ESGI
Programmation Système sous Linux - M. Malinge

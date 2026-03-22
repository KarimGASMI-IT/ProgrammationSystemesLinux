# ProgrammationSystemesLinux

Évaluation M1 SRC – Programmation Systèmes sous Linux – Langage C  
M. Malinge

---

# Simulation de conquête militaire

## Présentation

Ce projet simule une conquête militaire en langage C sous Linux, avec une hiérarchie réelle de processus Unix :

- 1 armée commandée par le général
- 3 divisions
- 3 régiments par division
- 5 compagnies par régiment

Chaque structure militaire est représentée par un **processus distinct**.

Le programme repose sur :

- la mémoire partagée System V
- les sémaphores System V
- les signaux Unix
- la création hiérarchique de processus avec `fork()`

Le général affiche l’état global de la conquête **toutes les 10 secondes**.

---

## Objectif du projet

Chaque compagnie génère aléatoirement des informations de combat :

- morts
- blessés
- ennemis morts
- prisonniers
- avancée en kilomètres
- recul en kilomètres

Ces informations sont remontées dans la hiérarchie :

- les compagnies mettent à jour leur régiment
- les régiments agrègent les données
- les divisions agrègent les résultats
- l’armée affiche un état global périodique

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

- les statistiques des compagnies
- les statistiques des régiments
- les statistiques des divisions

Structure utilisée :

```bash
compagnies[3][3][5]
regiments[3][3]
divisions[3]
```

---

## Synchronisation

Cette version utilise une synchronisation **fine**.

### Sémaphores utilisés

- 1 sémaphore par régiment
- 1 sémaphore par division

Soit :

- 9 sémaphores de régiment
- 3 sémaphores de division
- **12 sémaphores au total**

### Avantage

Deux régiments différents peuvent travailler en parallèle sans se bloquer.

Cela améliore :

- le parallélisme
- la fluidité de la simulation
- la cohérence avec une architecture système réelle

---

## Fonctionnement détaillé

### Compagnie

Chaque compagnie :

- génère des pertes aléatoires
- met à jour son état
- écrit en mémoire partagée (protégée par son sémaphore)
- affiche son activité

---

### Régiment

Chaque régiment :

- crée ses 5 compagnies
- lit leurs données
- agrège les pertes
- met à jour son état
- transmet à la division

---

### Division

Chaque division :

- crée ses 3 régiments
- lit leurs états
- agrège les résultats
- met à jour son état
- transmet à l’armée

---

### Armée

Le général :

- crée les divisions
- lit les états des divisions
- calcule le total global
- affiche l’état toutes les 10 secondes
- affiche un classement des divisions

---

## Concepts systèmes utilisés

### Processus
- `fork()`

### Mémoire partagée (System V)
- `shmget`
- `shmat`
- `shmdt`
- `shmctl`

### Sémaphores (System V)
- `semget`
- `semop`
- `semctl`

### Signaux
- `SIGINT` : arrêt avec Ctrl+C
- `SIGTERM` : terminaison des processus

---

## Arrêt et nettoyage

À la fin du programme :

- arrêt de tous les processus
- affichage du bilan final
- suppression de la mémoire partagée
- suppression des sémaphores

---

## Compilation

```bash
make clean
make
```
---

## Exécution
```bash
./simulation
```
---

## Arrêt
Ctrl + C

Nettoyage IPC (en cas de problème)

---

## Si le programme s’arrête mal :

```bash
make clean_ipc
```

ou :

```bash
ipcrm -a
```

---

## Vérifications
```bash
ps -ef | grep simulation
ipcs
```

Après arrêt, il ne doit rester aucun processus ni ressource IPC.

```bash
Exemple d’affichage
===== ETAT DU GENERAL (toutes les 10s) =====
Allies  : morts=330 blesses=513
Ennemis : morts=434 prisonniers=266
Progression : avance=117km recul=63km net=54km
===========================================

===== CLASSEMENT DES DIVISIONS =====
1) Division D2 | net=22km
2) Division D1 | net=20km
3) Division D0 | net=12km
====================================
```

## Choix techniques
Pourquoi une mémoire partagée unique ?

Simplifie l’architecture tout en respectant le sujet.

Pourquoi plusieurs sémaphores ?

Évite les blocages liés à un verrou global unique.

Pourquoi une simulation cumulative ?

Représente une progression continue dans le temps.

Pourquoi gérer Ctrl+C proprement ?

Pour éviter :

les processus zombies
les ressources IPC orphelines

---

## Auteur

Karim GASMI
M1 Systèmes, Réseaux & Cloud Computing – ESGI
Programmation Systèmes sous Linux – M. Malinge

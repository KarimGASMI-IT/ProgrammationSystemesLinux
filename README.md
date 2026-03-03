# ProgrammationSystèmesLinux
Évaluation M1 SRC / Programmation Systèmes sous Linux / Langage C / Mr Malinge

# Simulation de Conquête Militaire  
## Programmation Système sous Linux – M1 Systèmes, Réseaux & Cloud Computing

---

## Objectif du projet

Ce programme simule une conquête militaire en respectant une hiérarchie réelle :

- 1 Armée (Général)
- 3 Divisions
- 3 Régiments par division
- 5 Compagnies par régiment
- 150 hommes par compagnie (simulation logique)

Chaque entité est représentée par **un processus Unix distinct**.

La communication entre les niveaux hiérarchiques se fait via :

- Segments de mémoire partagée (System V)
- Sémaphores (mutex) pour assurer l'exclusion mutuelle

Le général affiche l'état global de la conquête toutes les 10 secondes.

---

## Architecture technique

### Nombre total de processus

- 1 processus Armée
- 3 processus Divisions
- 9 processus Régiments
- 45 processus Compagnies

Total : **58 processus**

---

## Fonctionnement de la simulation

### Compagnie

Chaque compagnie :
- Génère aléatoirement :
  - Morts
  - Blessés
  - Ennemis morts
  - Prisonniers
  - Avancée (km)
  - Recul (km)
- Met à jour ses pertes en **mode cumulatif**
- Protège l'accès à la mémoire partagée via un sémaphore

---

### Régiment

Chaque régiment :
- Agrège les pertes cumulées de ses compagnies
- Met à jour ses propres statistiques
- Transmet à la division

---

### Division

Chaque division :
- Agrège les pertes de ses régiments
- Met à jour son état global
- Transmet à l'armée

---

### Armée (Général)

Toutes les 10 secondes :
- Recalcule l'état global
- Affiche :
  - Pertes alliées
  - Pertes ennemies
  - Progression nette
  - Classement des divisions

---

## Concepts systèmes utilisés

### fork()
Création hiérarchique réelle des processus.

### Mémoire partagée (System V)
- `shmget`
- `shmat`
- `shmdt`
- `shmctl`

Permet le partage des données entre tous les processus.

### Sémaphores (System V)
- `semget`
- `semop`
- `semctl`

Utilisation d'un mutex global pour éviter les conditions de course.

### Signaux
- SIGINT (Ctrl+C) : arrêt propre
- SIGTERM : terminaison des processus enfants

### Nettoyage IPC
Les ressources sont supprimées proprement à la fin :

- Suppression segment mémoire
- Suppression sémaphore

---

## Synchronisation

L'accès à la mémoire partagée est protégé par un sémaphore mutex :

- `P()` avant écriture
- `V()` après écriture

Cela garantit :

- Aucune corruption mémoire
- Pas de race condition
- Cohérence des agrégations

---

## Compilation

```bash
make clean
make
▶Exécution
./simulation

Appuyer sur Ctrl+C pour arrêter la simulation proprement.

🧹 Nettoyage manuel IPC (si nécessaire)

Si la simulation est interrompue brutalement :

make clean_ipc

ou

ipcrm -a

----- 

## Vérification (à faire sur un autre terminal)

ps -ef | grep simulation
ipcs

----- 

Exemple d'affichage après Ctrl+C sur Terminal 1
===== ETAT DU GENERAL =====
Allies  : morts=330 blesses=513
Ennemis : morts=434 prisonniers=266
Progression : avance=117km recul=63km net=54km
===========================================

===== CLASSEMENT DES DIVISIONS =====
1) Division D2 | net=22km | ...
2) Division D1 | net=20km | ...
3) Division D0 | net=12km | ...

Exemple d'affichage après Ctrl+C sur Terminal 2
===== ETAT DU GENERAL =====
Allies  : morts=330 blesses=513
Ennemis : morts=434 prisonniers=266
Progression : avance=117km recul=63km net=54km
===========================================

===== CLASSEMENT DES DIVISIONS =====
1) Division D2 | net=22km | ...
2) Division D1 | net=20km | ...
3) Division D0 | net=12km | ...
-----

Choix techniques et justification

Utilisation d'une mémoire partagée unique pour simplifier l'architecture
Agrégation hiérarchique fidèle au modèle militaire
Simulation cumulative pour refléter une conquête progressive
Gestion propre des signaux pour éviter les processus zombies
Nettoyage rigoureux des ressources IPC

Réalisé par Karim GASMI
M1 Systèmes, Réseaux & Cloud Computing - ESGI
Programmation Système sous Linux - M. Malinge

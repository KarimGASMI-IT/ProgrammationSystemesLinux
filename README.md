# ProgrammationSystèmesLinux
Évaluation M1 SRC / Programmation Systèmes sous Linux / Langage C / Mr Malinge

make clean
make
./simulation


ps -ef | grep simulation
ipcs

J'aimerai qu'on rajoute dans le programme qu'on affiche affiche aussi les valeurs transmises par les régiments et divisions (actuellement ils transmettent sans montrer les chiffres) et que les différentes structures remontent des informations sur la 
progression sur le terrain (avancée de x km, recul de y km...)

🪖 Simulation de Conquête Militaire
Évaluation ESGI-M1 SRC / Programmation Systèmes sous Linux / Langage C / Mr Malinge

📌 Description du projet

Ce projet implémente une simulation hiérarchique d’une armée en conquête, en utilisant :
📦 Processus multiples (fork)
🧠 Mémoire partagée System V
🔐 Sémaphores System V
📡 Gestion avancée des signaux (SIGINT, SIGTERM)
🧹 Nettoyage propre des ressources IPC

L’objectif est de modéliser la remontée d’informations militaires à travers plusieurs niveaux hiérarchiques.
🏗 Architecture de l’armée simulée

L’organisation est strictement hiérarchique :
Armée (1 processus - Général)
 ├── 3 Divisions
 │     ├── 3 Régiments chacune
 │     │     ├── 5 Compagnies chacune
🔢 Nombre total de processus

1 Armée

3 Divisions

9 Régiments

45 Compagnies

➡ Total : 58 processus concurrents

Chaque structure est représentée par un processus distinct.

⚙ Fonctionnement de la simulation
🪖 Compagnies

Chaque compagnie génère aléatoirement :

Morts alliés

Blessés alliés

Ennemis morts

Prisonniers

Les données sont écrites dans la mémoire partagée.

🏹 Régiments

Chaque régiment :

Lit les données de ses 5 compagnies

Calcule la somme des pertes

Transmet les données à la division

Affiche les valeurs transmises

🏰 Divisions

Chaque division :

Lit les données de ses 3 régiments

Calcule la somme globale

Transmet à l’armée

Affiche les valeurs transmises

🎖 Armée (Général)

Toutes les 10 secondes, le général :

Lit les données des divisions

Calcule le total global

Affiche l’état général de la conquête

🔐 Synchronisation

Les accès à la mémoire partagée sont protégés par :

🔒 Un sémaphore binaire (mutex System V)

Cela garantit l'absence de race conditions lors des lectures/écritures concurrentes.

🧠 Gestion des signaux
CTRL + C (SIGINT)

Le général intercepte le signal

Envoie un SIGTERM à tout le groupe de processus

Attend la terminaison de tous les processus enfants

Affiche le rapport final

Libère proprement :

la mémoire partagée

le sémaphore

Sécurité

Utilisation de _exit() dans les handlers

Utilisation de write() (async-signal-safe)

Pas de processus zombies

Pas de fuite IPC

📦 IPC utilisées
Mémoire partagée

Contient :

Données des compagnies

Données agrégées des régiments

Données agrégées des divisions

Sémaphore

1 sémaphore binaire

Protège toutes les sections critiques

▶ Compilation
make
▶ Exécution
./simulation

Ou :

make run
🛑 Arrêt de la simulation

Appuyer sur :

CTRL + C

Affichage :

FIN DE LA CONQUETE
Divisions  : 3
Regiments  : 9
Compagnies : 45
Pertes alliees  : ...
Pertes ennemies : ...
Ressources IPC liberees.

Vérification :

ipcs

➡ Aucun segment ni sémaphore restant.

🧪 Vérification système
Voir les processus :
ps -ef | grep simulation
Voir les IPC :
ipcs
🛠 Outils et technologies

Langage C (C11)

Linux

fork()

shmget / shmat / shmctl

semget / semop / semctl

sigaction()

waitpid()

nanosleep()

🎯 Points techniques importants

Hiérarchie complète de processus

Synchronisation correcte

Nettoyage robuste des ressources IPC

Gestion sécurisée des signaux

Architecture conforme au scénario académique

📚 Objectifs pédagogiques couverts

✔ Programmation concurrente
✔ Communication inter-processus
✔ Synchronisation
✔ Gestion des signaux
✔ Gestion des ressources système
✔ Architecture hiérarchique multi-processus

👨‍🎓 Auteur

Étudiant M1 Systèmes, Réseaux & Cloud Computing
Programmation Systèmes sous Linux

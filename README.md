# ProgrammationSystemesLinux

Évaluation M1 SRC – Programmation Systèmes sous Linux – Langage C  
M. Malinge

---

## Présentation

Ce projet implémente une simulation de conquête militaire en langage C sous Linux.

Il repose sur :

- les processus (`fork`)
- la mémoire partagée (System V)
- les sémaphores (System V)
- les signaux Unix

Le projet a été réalisé en **deux versions distinctes** afin d'améliorer progressivement la gestion de la concurrence.

---

## V1 – Version initiale

Cette version correspond à la première implémentation du projet.

### Caractéristiques

- mémoire partagée unique
- synchronisation avec **un sémaphore global**
- architecture fonctionnelle
- implémentation simple

### Limites

- forte contention (blocage entre processus)
- faible parallélisme
- tous les processus doivent attendre le même verrou

---

## V2 – Version améliorée

Cette version introduit une amélioration de la gestion de la concurrence.

### Améliorations

- suppression du sémaphore global
- utilisation de **sémaphores multiples**
  - 1 par régiment
  - 1 par division
- accès concurrent plus fin à la mémoire partagée

### Avantages

- réduction des blocages
- amélioration du parallélisme
- meilleure performance globale
- architecture plus réaliste

---

## Comparaison V1 vs V2

| Critère              | V1                          | V2                          |
|---------------------|-----------------------------|-----------------------------|
| Synchronisation     | Sémaphore global            | Sémaphores multiples        |
| Parallélisme        | Faible                      | Élevé                       |
| Complexité          | Simple                      | Plus structurée             |
| Performance         | Limitée                     | Améliorée                   |
| Réalisme système    | Basique                     | Plus proche du réel         |

---

## Objectif pédagogique

Ce projet permet de comprendre :

- la gestion des processus sous Linux
- les mécanismes IPC (mémoire partagée, sémaphores)
- les problèmes de concurrence
- les stratégies de synchronisation

L’évolution entre V1 et V2 illustre le passage :

- d’une synchronisation globale  
- à une synchronisation fine

---

## Compilation et exécution

Se placer dans un dossier :

```bash
cd V2   # ou V1
make clean
make
./simulation
```
## Conclusion

La version V2 améliore significativement la gestion de la concurrence en remplaçant un verrou global par des verrous locaux.

Cela permet une meilleure exploitation du parallélisme et une simulation plus fluide.

## Auteur

Karim GASMI

M1 Systèmes, Réseaux & Cloud Computing – ESGI

Programmation Systèmes sous Linux – M. Malinge

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <string.h>

/* ================= CONFIG ================= */

#define NB_DIV  3
#define NB_REG  3
#define NB_COMP 5

/* ================= STRUCTURES ================= */

typedef struct {
    int morts;
    int blesses;
    int ennemis_morts;
    int prisonniers;
} pertes_t;

typedef struct {
    pertes_t compagnies[NB_DIV][NB_REG][NB_COMP];
} armee_t;

/* ================= GLOBAL ================= */

static int shmid;
static int semid;
static armee_t *shm;

/* ================= SEMAPHORE ================= */

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void sem_P(int id) {
    struct sembuf op = {0, -1, 0};
    semop(id, &op, 1);
}

void sem_V(int id) {
    struct sembuf op = {0, +1, 0};
    semop(id, &op, 1);
}

/* ================= UTILS ================= */

int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

void compute_total(pertes_t *total) {
    memset(total, 0, sizeof(*total));

    for (int d = 0; d < NB_DIV; d++)
        for (int r = 0; r < NB_REG; r++)
            for (int c = 0; c < NB_COMP; c++) {
                total->morts += shm->compagnies[d][r][c].morts;
                total->blesses += shm->compagnies[d][r][c].blesses;
                total->ennemis_morts += shm->compagnies[d][r][c].ennemis_morts;
                total->prisonniers += shm->compagnies[d][r][c].prisonniers;
            }
}

/* ================= CLEANUP ================= */

void cleanup() {
    if (shm && shm != (void*)-1)
        shmdt(shm);

    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}

void handle_sigterm(int sig) {
    (void)sig;
    exit(0);
}

void handle_sigint(int sig) {
    (void)sig;

    printf("\nArret propre de la simulation...\n");
    fflush(stdout);

    // Le chef ignore SIGTERM pour ne pas se tuer avant le cleanup
    signal(SIGTERM, SIG_IGN);

    // Tue tout le groupe
    kill(0, SIGTERM);

    // Nettoyage IPC
    cleanup();

    exit(0);
}

/* ================= COMPAGNIE ================= */

void run_compagnie(int d, int r, int c) {

    srand(time(NULL) ^ getpid());

    while (1) {

        pertes_t p;
        p.morts = rand_range(0, 15);
        p.blesses = rand_range(0, 20);
        p.ennemis_morts = rand_range(0, 20);
        p.prisonniers = rand_range(0, 10);

        sem_P(semid);
        shm->compagnies[d][r][c] = p;
        sem_V(semid);

        printf("Compagnie [%d][%d][%d] : morts=%d blesses=%d ennemis=%d prisonniers=%d\n",
               d, r, c, p.morts, p.blesses, p.ennemis_morts, p.prisonniers);
        fflush(stdout);

        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = rand_range(300, 1500) * 1000000L;
        nanosleep(&ts, NULL);
    }
}

/* ================= REGIMENT ================= */

void run_regiment(int d, int r) {

    for (int c = 0; c < NB_COMP; c++) {
        if (fork() == 0) {
            run_compagnie(d, r, c);
            exit(0);
        }
    }

    while (1) pause();
}

/* ================= DIVISION ================= */

void run_division(int d) {

    for (int r = 0; r < NB_REG; r++) {
        if (fork() == 0) {
            run_regiment(d, r);
            exit(0);
        }
    }

    while (1) pause();
}

/* ================= ARMEE ================= */

void run_armee() {

    for (int d = 0; d < NB_DIV; d++) {
        if (fork() == 0) {
            run_division(d);
            exit(0);
        }
    }

    while (1) {
        sleep(10);

        pertes_t total;

        sem_P(semid);
        compute_total(&total);
        sem_V(semid);

        printf("\n===== ETAT GENERAL =====\n");
        printf("Allies : morts=%d blesses=%d\n", total.morts, total.blesses);
        printf("Ennemis : morts=%d prisonniers=%d\n", total.ennemis_morts, total.prisonniers);
        printf("========================\n\n");

        fflush(stdout);
    }
}

/* ================= MAIN ================= */

int main() {

    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigint);

    key_t key = ftok(".", 'A');

    shmid = shmget(key, sizeof(armee_t), IPC_CREAT | 0666);
    shm = (armee_t*) shmat(shmid, NULL, 0);
    memset(shm, 0, sizeof(armee_t));

    semid = semget(key, 1, IPC_CREAT | 0666);

    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    run_armee();

    return 0;
}

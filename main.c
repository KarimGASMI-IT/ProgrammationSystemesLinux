#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>

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
    int progression;   // km (+ avance / - recul)
} pertes_t;

typedef struct {
    pertes_t compagnies[NB_DIV][NB_REG][NB_COMP];
    pertes_t regiments[NB_DIV][NB_REG];
    pertes_t divisions[NB_DIV];
} armee_t;

/* ================= GLOBAL ================= */

static int shmid = -1;
static int semid = -1;
static armee_t *shm = NULL;
static pid_t chef_pid;

/* ================= SEMAPHORE ================= */

union semun {
    int val;
};

static void sem_P(void) {
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

static void sem_V(void) {
    struct sembuf op = {0, +1, 0};
    semop(semid, &op, 1);
}

/* ================= UTILS ================= */

static void timestamp() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

static int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

static pertes_t zero_pertes(void) {
    pertes_t p;
    memset(&p, 0, sizeof(p));
    return p;
}

static pertes_t add_pertes(pertes_t a, pertes_t b) {
    a.morts += b.morts;
    a.blesses += b.blesses;
    a.ennemis_morts += b.ennemis_morts;
    a.prisonniers += b.prisonniers;
    a.progression += b.progression;
    return a;
}

static void seed_rng(void) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
}

static void tiny_sleep(long ms) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = ms * 1000000L;
    nanosleep(&ts, NULL);
}

/* ================= FINAL REPORT ================= */

static pertes_t compute_total(void) {
    pertes_t total = zero_pertes();
    pertes_t local[NB_DIV];

    sem_P();
    for (int d = 0; d < NB_DIV; d++)
        local[d] = shm->divisions[d];
    sem_V();

    for (int d = 0; d < NB_DIV; d++)
        total = add_pertes(total, local[d]);

    return total;
}

static void print_final_report(void) {
    pertes_t total = compute_total();

    printf("\n==============================\n");
    printf("        FIN DE LA CONQUETE\n");
    printf("==============================\n");

    printf("Pertes alliees : morts=%d blesses=%d\n",
           total.morts, total.blesses);

    printf("Pertes ennemies : morts=%d prisonniers=%d\n",
           total.ennemis_morts, total.prisonniers);

    printf("Progression totale sur le terrain : %d km\n",
           total.progression);

    printf("==============================\n\n");
}

/* ================= SIGNALS ================= */

static void handle_sigterm(int sig) {
    (void)sig;
    _exit(0);
}

static void handle_sigint(int sig) {
    (void)sig;

    if (getpid() != chef_pid)
        _exit(0);

    printf("\nArret de la simulation...\n");

    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);

    tiny_sleep(200);

    print_final_report();

    if (shm) shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    _exit(0);
}

/* ================= COMPAGNIE ================= */

static void run_compagnie(int d, int r, int c) {
    seed_rng();

    while (1) {
        pertes_t p;
        p.morts = rand_range(0, 15);
        p.blesses = rand_range(0, 20);
        p.ennemis_morts = rand_range(0, 20);
        p.prisonniers = rand_range(0, 10);
        p.progression = rand_range(-2, 5); // recul ou avance

        sem_P();
        shm->compagnies[d][r][c] = p;
        sem_V();

        timestamp();
        printf("Compagnie C%d R%d D%d envoie : "
               "morts=%d blesses=%d ennemis=%d prisonniers=%d progression=%+dkm\n",
               c, r, d,
               p.morts, p.blesses,
               p.ennemis_morts, p.prisonniers,
               p.progression);

        fflush(stdout);
        tiny_sleep(rand_range(300, 1200));
    }
}

/* ================= REGIMENT ================= */

static void run_regiment(int d, int r) {
    seed_rng();

    for (int c = 0; c < NB_COMP; c++)
        if (fork() == 0) {
            run_compagnie(d, r, c);
            _exit(0);
        }

    while (1) {
        pertes_t sum = zero_pertes();

        sem_P();
        for (int c = 0; c < NB_COMP; c++)
            sum = add_pertes(sum, shm->compagnies[d][r][c]);
        shm->regiments[d][r] = sum;
        sem_V();

        timestamp();
        printf("Regiment R%d (Division D%d) transmet : "
               "morts=%d blesses=%d ennemis=%d prisonniers=%d progression=%+dkm\n",
               r, d,
               sum.morts, sum.blesses,
               sum.ennemis_morts, sum.prisonniers,
               sum.progression);

        tiny_sleep(rand_range(500, 1000));
    }
}

/* ================= DIVISION ================= */

static void run_division(int d) {
    seed_rng();

    for (int r = 0; r < NB_REG; r++)
        if (fork() == 0) {
            run_regiment(d, r);
            _exit(0);
        }

    while (1) {
        pertes_t sum = zero_pertes();

        sem_P();
        for (int r = 0; r < NB_REG; r++)
            sum = add_pertes(sum, shm->regiments[d][r]);
        shm->divisions[d] = sum;
        sem_V();

        timestamp();
        printf("Division D%d transmet a l'armee : "
               "morts=%d blesses=%d ennemis=%d prisonniers=%d progression=%+dkm\n",
               d,
               sum.morts, sum.blesses,
               sum.ennemis_morts, sum.prisonniers,
               sum.progression);

        tiny_sleep(rand_range(800, 1500));
    }
}

/* ================= ARMEE ================= */

static void run_armee(void) {
    for (int d = 0; d < NB_DIV; d++)
        if (fork() == 0) {
            run_division(d);
            _exit(0);
        }

    while (1) {
        sleep(10);
        pertes_t total = compute_total();

        printf("\n===== ETAT GENERAL =====\n");
        printf("Allies : morts=%d blesses=%d\n",
               total.morts, total.blesses);
        printf("Ennemis : morts=%d prisonniers=%d\n",
               total.ennemis_morts, total.prisonniers);
        printf("Progression globale : %d km\n",
               total.progression);
        printf("========================\n\n");
        fflush(stdout);
    }
}

/* ================= MAIN ================= */

int main(void) {
    chef_pid = getpid();
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, SIG_IGN);

    key_t key = ftok(".", 'A');
    shmid = shmget(key, sizeof(armee_t), IPC_CREAT | 0666);
    shm = shmat(shmid, NULL, 0);
    memset(shm, 0, sizeof(*shm));

    semid = semget(key, 1, IPC_CREAT | 0666);
    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    run_armee();
    return 0;
}

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
    struct semid_ds *buf;
    unsigned short *array;
};

static void sem_P(void) {
    struct sembuf op = {0, -1, 0};
    if (semop(semid, &op, 1) == -1) {
        perror("semop P");
        _exit(1);
    }
}

static void sem_V(void) {
    struct sembuf op = {0, +1, 0};
    if (semop(semid, &op, 1) == -1) {
        perror("semop V");
        _exit(1);
    }
}

/* ================= UTILS ================= */

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

/* ================= CLEANUP ================= */

static void cleanup_ipc(void) {
    if (getpid() != chef_pid) return;

    if (shm) shmdt(shm);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
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
    time_t now = time(NULL);

    printf("\n\n==============================\n");
    printf("        FIN DE LA CONQUETE\n");
    printf("==============================\n");
    printf("Date : %s\n", ctime(&now));

    printf("Structures engagees :\n");
    printf("  Divisions  : %d\n", NB_DIV);
    printf("  Regiments  : %d\n", NB_DIV * NB_REG);
    printf("  Compagnies : %d\n\n", NB_DIV * NB_REG * NB_COMP);

    printf("Pertes alliees :\n");
    printf("  Morts   : %d\n", total.morts);
    printf("  Blesses : %d\n\n", total.blesses);

    printf("Pertes ennemies :\n");
    printf("  Morts       : %d\n", total.ennemis_morts);
    printf("  Prisonniers : %d\n\n", total.prisonniers);

    printf("Simulation terminee proprement.\n");
    printf("Ressources IPC liberees.\n");
    printf("==============================\n\n");
}

/* ================= SIGNALS ================= */

static void handle_sigterm(int sig) {
    (void)sig;
    _exit(0);  // enfants meurent immédiatement
}

static void handle_sigint(int sig) {
    (void)sig;

    if (getpid() != chef_pid)
        _exit(0);

    printf("\nArret de la simulation...\n");
    fflush(stdout);

    /* Ignorer SIGTERM pour le chef */
    struct sigaction sa_ignore;
    memset(&sa_ignore, 0, sizeof(sa_ignore));
    sa_ignore.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &sa_ignore, NULL);

    /* Tuer tous les enfants */
    kill(0, SIGTERM);

    tiny_sleep(200);

    print_final_report();
    cleanup_ipc();

    _exit(0);
}

static void install_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);
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

        sem_P();
        shm->compagnies[d][r][c] = p;
        sem_V();

        printf("Compagnie D%d R%d C%d : morts=%d blesses=%d ennemis=%d prisonniers=%d\n",
               d, r, c, p.morts, p.blesses, p.ennemis_morts, p.prisonniers);
        fflush(stdout);

        tiny_sleep(rand_range(300, 1500));
    }
}

/* ================= REGIMENT ================= */

static void run_regiment(int d, int r) {
    seed_rng();

    for (int c = 0; c < NB_COMP; c++) {
        if (fork() == 0) {
            run_compagnie(d, r, c);
            _exit(0);
        }
    }

    while (1) {
        pertes_t local[NB_COMP];

        sem_P();
        for (int c = 0; c < NB_COMP; c++)
            local[c] = shm->compagnies[d][r][c];
        sem_V();

        pertes_t sum = zero_pertes();
        for (int c = 0; c < NB_COMP; c++)
            sum = add_pertes(sum, local[c]);

        sem_P();
        shm->regiments[d][r] = sum;
        sem_V();

        tiny_sleep(rand_range(200, 800));
    }
}

/* ================= DIVISION ================= */

static void run_division(int d) {
    seed_rng();

    for (int r = 0; r < NB_REG; r++) {
        if (fork() == 0) {
            run_regiment(d, r);
            _exit(0);
        }
    }

    while (1) {
        pertes_t local[NB_REG];

        sem_P();
        for (int r = 0; r < NB_REG; r++)
            local[r] = shm->regiments[d][r];
        sem_V();

        pertes_t sum = zero_pertes();
        for (int r = 0; r < NB_REG; r++)
            sum = add_pertes(sum, local[r]);

        sem_P();
        shm->divisions[d] = sum;
        sem_V();

        tiny_sleep(rand_range(300, 1000));
    }
}

/* ================= ARMEE ================= */

static void run_armee(void) {
    for (int d = 0; d < NB_DIV; d++) {
        if (fork() == 0) {
            run_division(d);
            _exit(0);
        }
    }

    while (1) {
        sleep(10);
        pertes_t total = compute_total();

        printf("\n===== ETAT GENERAL =====\n");
        printf("Allies : morts=%d blesses=%d\n", total.morts, total.blesses);
        printf("Ennemis : morts=%d prisonniers=%d\n", total.ennemis_morts, total.prisonniers);
        printf("========================\n\n");
        fflush(stdout);
    }
}

/* ================= MAIN ================= */

int main(void) {
    chef_pid = getpid();

    setpgid(0, 0);

    install_signals();

    key_t key = ftok(".", 'A');
    if (key == -1) key = IPC_PRIVATE;

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

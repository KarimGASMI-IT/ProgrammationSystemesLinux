#define _XOPEN_SOURCE 700

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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
    // écrit par les compagnies
    pertes_t compagnies[NB_DIV][NB_REG][NB_COMP];
    // écrit par les régiments (somme des 5 compagnies)
    pertes_t regiments[NB_DIV][NB_REG];
    // écrit par les divisions (somme des 3 régiments)
    pertes_t divisions[NB_DIV];
} armee_t;

/* ================= GLOBAL ================= */

static int shmid = -1;
static int semid = -1;
static armee_t *shm = (void *)-1;
static pid_t chef_pid = -1;

/* ================= SEMAPHORE ================= */

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void sem_P(void) {
    struct sembuf op = {0, -1, 0};
    if (semop(semid, &op, 1) == -1) {
        perror("semop(P)");
        _exit(1);
    }
}

static void sem_V(void) {
    struct sembuf op = {0, +1, 0};
    if (semop(semid, &op, 1) == -1) {
        perror("semop(V)");
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

static void die_perror(const char *msg) {
    perror(msg);
    exit(1);
}

static void seed_rng(void) {
    // seed différent par processus
    srand((unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)(uintptr_t)&errno);
}

/* ================= CLEANUP (chef uniquement) ================= */

static void cleanup_ipc_chef(void) {
    if (getpid() != chef_pid) return;

    if (shm != (void *)-1) {
        if (shmdt(shm) == -1) perror("shmdt");
        shm = (void *)-1;
    }

    if (shmid != -1) {
        if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("shmctl(IPC_RMID)");
        shmid = -1;
    }

    if (semid != -1) {
        if (semctl(semid, 0, IPC_RMID) == -1) perror("semctl(IPC_RMID)");
        semid = -1;
    }
}

/* ================= FINAL REPORT (chef) ================= */

static pertes_t compute_total_from_divisions(void) {
    pertes_t local_div[NB_DIV];

    sem_P();
    for (int d = 0; d < NB_DIV; d++) {
        local_div[d] = shm->divisions[d];
    }
    sem_V();

    pertes_t total = zero_pertes();
    for (int d = 0; d < NB_DIV; d++) {
        total = add_pertes(total, local_div[d]);
    }
    return total;
}

static void print_final_report(void) {
    pertes_t total = compute_total_from_divisions();

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_now);

    printf("\n\n==============================\n");
    printf("        FIN DE LA CONQUETE\n");
    printf("==============================\n");
    printf("Date/heure : %s\n\n", buf);

    printf("Structures engagees :\n");
    printf("  Divisions   : %d\n", NB_DIV);
    printf("  Regiments   : %d\n", NB_DIV * NB_REG);
    printf("  Compagnies  : %d\n\n", NB_DIV * NB_REG * NB_COMP);

    printf("Pertes alliees :\n");
    printf("  Morts        : %d\n", total.morts);
    printf("  Blesses      : %d\n\n", total.blesses);

    printf("Pertes ennemies :\n");
    printf("  Morts         : %d\n", total.ennemis_morts);
    printf("  Prisonniers   : %d\n\n", total.prisonniers);

    printf("Arret propre : OK\n");
    printf("Nettoyage IPC : OK\n");
    printf("==============================\n\n");
    fflush(stdout);
}

/* ================= SIGNALS ================= */

static void handle_sigterm(int sig) {
    (void)sig;
    // Les enfants sortent immédiatement (sans cleanup IPC)
    _exit(0);
}

static void handle_sigint(int sig) {
    (void)sig;

    // Seul le chef affiche le bilan et nettoie
    if (getpid() != chef_pid) {
        _exit(0);
    }

    // Ignorer SIGTERM le temps de tuer tout le groupe
    struct sigaction ign;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &ign, NULL);

    // Stopper tout le groupe de processus
    if (kill(0, SIGTERM) == -1) {
        // si erreur, on continue quand même vers cleanup (ex: déjà mort)
        perror("kill(0, SIGTERM)");
    }

    // petite latence pour laisser les processus mourir
    usleep(200000);

    // Affichage final clair
    print_final_report();

    // Nettoyage IPC chef
    cleanup_ipc_chef();

    _exit(0);
}

static void install_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = handle_sigterm;
    if (sigaction(SIGTERM, &sa, NULL) == -1) die_perror("sigaction(SIGTERM)");

    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) die_perror("sigaction(SIGINT)");

    // Pas de zombies
    struct sigaction schld;
    memset(&schld, 0, sizeof(schld));
    sigemptyset(&schld.sa_mask);
    schld.sa_handler = SIG_IGN;
    schld.sa_flags = SA_RESTART | SA_NOCLDWAIT;
    (void)sigaction(SIGCHLD, &schld, NULL);
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

        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = (long)rand_range(300, 1500) * 1000000L;
        nanosleep(&ts, NULL);
    }
}

/* ================= REGIMENT ================= */

static void run_regiment(int d, int r) {
    seed_rng();

    // Crée les 5 compagnies
    for (int c = 0; c < NB_COMP; c++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork(compagnie)");
            _exit(1);
        }
        if (pid == 0) {
            run_compagnie(d, r, c);
            _exit(0);
        }
    }

    // Remontée : somme des compagnies -> regiments[d][r]
    while (1) {
        pertes_t local[NB_COMP];

        sem_P();
        for (int c = 0; c < NB_COMP; c++) {
            local[c] = shm->compagnies[d][r][c];
        }
        sem_V();

        pertes_t sum = zero_pertes();
        for (int c = 0; c < NB_COMP; c++) {
            sum = add_pertes(sum, local[c]);
        }

        sem_P();
        shm->regiments[d][r] = sum;
        sem_V();

        usleep((useconds_t)rand_range(200, 800) * 1000);
    }
}

/* ================= DIVISION ================= */

static void run_division(int d) {
    seed_rng();

    // Crée les 3 régiments
    for (int r = 0; r < NB_REG; r++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork(regiment)");
            _exit(1);
        }
        if (pid == 0) {
            run_regiment(d, r);
            _exit(0);
        }
    }

    // Remontée : somme des régiments -> divisions[d]
    while (1) {
        pertes_t local[NB_REG];

        sem_P();
        for (int r = 0; r < NB_REG; r++) {
            local[r] = shm->regiments[d][r];
        }
        sem_V();

        pertes_t sum = zero_pertes();
        for (int r = 0; r < NB_REG; r++) {
            sum = add_pertes(sum, local[r]);
        }

        sem_P();
        shm->divisions[d] = sum;
        sem_V();

        usleep((useconds_t)rand_range(300, 1000) * 1000);
    }
}

/* ================= ARMEE ================= */

static void run_armee(void) {
    // Crée les 3 divisions
    for (int d = 0; d < NB_DIV; d++) {
        pid_t pid = fork();
        if (pid == -1) die_perror("fork(division)");
        if (pid == 0) {
            run_division(d);
            _exit(0);
        }
    }

    // Affichage toutes les 10s
    while (1) {
        sleep(10);

        pertes_t total = compute_total_from_divisions();

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

    // Mettre le programme dans son propre groupe pour kill(0, SIGTERM)
    // (si déjà leader, ça peut échouer avec EACCES/EINVAL selon cas, pas bloquant)
    (void)setpgid(0, 0);

    install_signals();

    key_t key = ftok(".", 'A');
    if (key == (key_t)-1) {
        // fallback si ftok échoue
        perror("ftok");
        key = IPC_PRIVATE;
    }

    shmid = shmget(key, sizeof(armee_t), IPC_CREAT | 0666);
    if (shmid == -1) die_perror("shmget");

    shm = (armee_t *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) die_perror("shmat");

    memset(shm, 0, sizeof(*shm));

    semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) die_perror("semget");

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) die_perror("semctl(SETVAL)");

    // au cas où main() sort “normalement”
    atexit(cleanup_ipc_chef);

    run_armee();
    return 0;
}

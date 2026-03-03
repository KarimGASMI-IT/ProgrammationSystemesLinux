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
#include <sys/wait.h>
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
    int avance_km;
    int recul_km;
} pertes_t;

typedef struct {
    pertes_t compagnies[NB_DIV][NB_REG][NB_COMP]; /* cumul par compagnie */
    pertes_t regiments[NB_DIV][NB_REG];           /* cumul agrégé */
    pertes_t divisions[NB_DIV];                   /* cumul agrégé */
} armee_t;

/* ================= GLOBAL ================= */

static int shmid = -1;
static int semid = -1;
static armee_t *shm = NULL;
static pid_t chef_pid = -1;

/* ================= SEMAPHORE ================= */

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void die(const char *msg) {
    perror(msg);
    _exit(EXIT_FAILURE);
}

static void sem_P(void) {
    struct sembuf op = {0, -1, 0};
    while (semop(semid, &op, 1) == -1) {
        if (errno == EINTR) continue;
        die("semop(P)");
    }
}

static void sem_V(void) {
    struct sembuf op = {0, +1, 0};
    while (semop(semid, &op, 1) == -1) {
        if (errno == EINTR) continue;
        die("semop(V)");
    }
}

/* ================= UTILS ================= */

static void timestamp(void) {
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
    a.morts         += b.morts;
    a.blesses       += b.blesses;
    a.ennemis_morts += b.ennemis_morts;
    a.prisonniers   += b.prisonniers;
    a.avance_km     += b.avance_km;
    a.recul_km      += b.recul_km;
    return a;
}

static void seed_rng(void) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
}

static void tiny_sleep(long ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1) {
        if (errno == EINTR) continue;
        break;
    }
}

/* ================= CLEANUP IPC ================= */

static void cleanup_ipc(void) {
    if (getpid() != chef_pid) return;

    if (shm) {
        shmdt(shm);
        shm = NULL;
    }
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
        shmid = -1;
    }
    if (semid != -1) {
        semctl(semid, 0, IPC_RMID);
        semid = -1;
    }
}

/* ================= RECALCUL (agrégation) ================= */

static void recalcul_regiment(int d, int r) {
    pertes_t sum = zero_pertes();
    for (int c = 0; c < NB_COMP; c++)
        sum = add_pertes(sum, shm->compagnies[d][r][c]);
    shm->regiments[d][r] = sum;
}

static void recalcul_division(int d) {
    pertes_t sum = zero_pertes();
    for (int r = 0; r < NB_REG; r++)
        sum = add_pertes(sum, shm->regiments[d][r]);
    shm->divisions[d] = sum;
}

static void recalcul_global(void) {
    sem_P();
    for (int d = 0; d < NB_DIV; d++) {
        for (int r = 0; r < NB_REG; r++)
            recalcul_regiment(d, r);
        recalcul_division(d);
    }
    sem_V();
}

static pertes_t compute_total(void) {
    pertes_t total = zero_pertes();
    sem_P();
    for (int d = 0; d < NB_DIV; d++)
        total = add_pertes(total, shm->divisions[d]);
    sem_V();
    return total;
}

/* ================= CLASSEMENT ================= */

static void afficher_classement(void) {
    pertes_t divs[NB_DIV];

    sem_P();
    for (int i = 0; i < NB_DIV; i++)
        divs[i] = shm->divisions[i];
    sem_V();

    int ordre[NB_DIV];
    for (int i = 0; i < NB_DIV; i++) ordre[i] = i;

    for (int i = 0; i < NB_DIV - 1; i++) {
        for (int j = i + 1; j < NB_DIV; j++) {
            int net_i = divs[ordre[i]].avance_km - divs[ordre[i]].recul_km;
            int net_j = divs[ordre[j]].avance_km - divs[ordre[j]].recul_km;
            if (net_j > net_i) {
                int tmp = ordre[i];
                ordre[i] = ordre[j];
                ordre[j] = tmp;
            }
        }
    }

    printf("\n===== CLASSEMENT DES DIVISIONS =====\n");
    for (int i = 0; i < NB_DIV; i++) {
        int d = ordre[i];
        int net = divs[d].avance_km - divs[d].recul_km;
        printf("%d) Division D%d | net=%dkm | morts=%d blesses=%d | ennemis=%d | prisonniers=%d\n",
               i + 1, d, net,
               divs[d].morts, divs[d].blesses,
               divs[d].ennemis_morts, divs[d].prisonniers);
    }
    printf("====================================\n");
}

/* ================= FINAL REPORT ================= */

static void print_final_report(void) {
    recalcul_global();
    pertes_t total = compute_total();

    printf("\n==============================\n");
    printf("        FIN DE LA CONQUETE\n");
    printf("==============================\n");
    printf("Divisions  : %d\n", NB_DIV);
    printf("Regiments  : %d\n", NB_DIV * NB_REG);
    printf("Compagnies : %d\n\n", NB_DIV * NB_REG * NB_COMP);

    printf("Pertes alliees  : morts=%d blesses=%d\n", total.morts, total.blesses);
    printf("Pertes ennemies : morts=%d prisonniers=%d\n", total.ennemis_morts, total.prisonniers);

    printf("Progression finale : avance=%dkm recul=%dkm net=%dkm\n",
           total.avance_km, total.recul_km, total.avance_km - total.recul_km);

    afficher_classement();

    printf("\nSimulation terminee proprement.\n");
    printf("Ressources IPC liberees.\n");
    printf("==============================\n\n");
}

/* ================= SIGNALS ================= */

static void handle_sigterm(int sig) {
    (void)sig;
    _exit(0);
}

static void handle_sigint(int sig) {
    (void)sig;

    if (getpid() != chef_pid) _exit(0);

    write(STDOUT_FILENO, "\nArret de la simulation...\n", 27);

    /* éviter que le SIGTERM envoyé ensuite ne retombe ici */
    struct sigaction sa_ignore;
    memset(&sa_ignore, 0, sizeof(sa_ignore));
    sa_ignore.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &sa_ignore, NULL);

    /* stoppe tous les processus du groupe */
    kill(0, SIGTERM);

    while (waitpid(-1, NULL, 0) > 0) {
        /* attendre tous les fils */
    }

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
}

/* ================= HIERARCHIE ================= */

static void run_compagnie(int d, int r, int c) {
    seed_rng();

    while (1) {
        pertes_t p = {
            rand_range(0, 15),
            rand_range(0, 20),
            rand_range(0, 20),
            rand_range(0, 10),
            rand_range(0, 5),
            rand_range(0, 3)
        };

        /* cumul: on ajoute au lieu d’écraser */
        sem_P();
        shm->compagnies[d][r][c] = add_pertes(shm->compagnies[d][r][c], p);
        sem_V();

        timestamp();
        printf("Compagnie C%d R%d D%d : +morts=%d +blesses=%d +ennemis=%d +prisonniers=%d +avance=%dkm +recul=%dkm\n",
               c, r, d, p.morts, p.blesses, p.ennemis_morts, p.prisonniers, p.avance_km, p.recul_km);
        fflush(stdout);

        tiny_sleep(rand_range(300, 1200));
    }
}

static void run_regiment(int d, int r) {
    for (int c = 0; c < NB_COMP; c++) {
        pid_t pid = fork();
        if (pid == -1) die("fork(compagnie)");
        if (pid == 0) { run_compagnie(d, r, c); _exit(0); }
    }

    while (1) {
        pertes_t sum = zero_pertes();

        sem_P();
        for (int c = 0; c < NB_COMP; c++)
            sum = add_pertes(sum, shm->compagnies[d][r][c]);
        shm->regiments[d][r] = sum;
        sem_V();

        timestamp();
        printf("Regiment R%d (Division D%d) transmet : morts=%d blesses=%d ennemis=%d prisonniers=%d net=%dkm\n",
               r, d, sum.morts, sum.blesses, sum.ennemis_morts, sum.prisonniers,
               sum.avance_km - sum.recul_km);
        fflush(stdout);

        tiny_sleep(500);
    }
}

static void run_division(int d) {
    for (int r = 0; r < NB_REG; r++) {
        pid_t pid = fork();
        if (pid == -1) die("fork(regiment)");
        if (pid == 0) { run_regiment(d, r); _exit(0); }
    }

    while (1) {
        pertes_t sum = zero_pertes();

        sem_P();
        for (int r = 0; r < NB_REG; r++)
            sum = add_pertes(sum, shm->regiments[d][r]);
        shm->divisions[d] = sum;
        sem_V();

        timestamp();
        printf("Division D%d transmet a l'armee : morts=%d blesses=%d ennemis=%d prisonniers=%d net=%dkm\n",
               d, sum.morts, sum.blesses, sum.ennemis_morts, sum.prisonniers,
               sum.avance_km - sum.recul_km);
        fflush(stdout);

        tiny_sleep(700);
    }
}

/* ================= ARMEE ================= */

static void afficher_etat_general(void) {
    recalcul_global();
    pertes_t total = compute_total();

    printf("\n===== ETAT DU GENERAL (toutes les 10s) =====\n");
    printf("Allies  : morts=%d blesses=%d\n", total.morts, total.blesses);
    printf("Ennemis : morts=%d prisonniers=%d\n", total.ennemis_morts, total.prisonniers);
    printf("Progression : avance=%dkm recul=%dkm net=%dkm\n",
           total.avance_km, total.recul_km, total.avance_km - total.recul_km);
    printf("===========================================\n");

    afficher_classement();
    fflush(stdout);
}

static void run_armee(void) {
    for (int d = 0; d < NB_DIV; d++) {
        pid_t pid = fork();
        if (pid == -1) die("fork(division)");
        if (pid == 0) { run_division(d); _exit(0); }
    }

    sleep(1);
    afficher_etat_general();

    while (1) {
        sleep(10);
        afficher_etat_general();
    }
}

/* ================= MAIN ================= */

int main(void) {
    chef_pid = getpid();

    /* isoler le programme dans son groupe pour kill(0, SIGTERM) */
    if (setpgid(0, 0) == -1) {
        /* pas critique */
    }

    install_signals();

    key_t key = ftok(".", 'A');
    if (key == -1) {
        /* fallback: ok pour un TP */
        key = IPC_PRIVATE;
    }

    shmid = shmget(key, sizeof(armee_t), IPC_CREAT | 0666);
    if (shmid == -1) die("shmget");

    shm = shmat(shmid, NULL, 0);
    if (shm == (void *)-1) die("shmat");

    memset(shm, 0, sizeof(*shm));

    semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) die("semget");

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) die("semctl(SETVAL)");

    printf("=== DEBUT DE LA SIMULATION ===\n");
    printf("Appuyez sur Ctrl+C pour arreter.\n\n");

    run_armee();
    return 0;
}

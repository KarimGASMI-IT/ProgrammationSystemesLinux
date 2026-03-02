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
#include <sys/wait.h>

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
    pertes_t compagnies[NB_DIV][NB_REG][NB_COMP]; // écrit par les compagnies
    pertes_t regiments[NB_DIV][NB_REG];           // écrit par les régiments (somme compagnies)
    pertes_t divisions[NB_DIV];                   // écrit par les divisions (somme régiments)
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

static void sem_P(int id) {
    struct sembuf op = {0, -1, 0};
    if (semop(id, &op, 1) == -1) {
        perror("semop(P)");
        _exit(1);
    }
}

static void sem_V(int id) {
    struct sembuf op = {0, +1, 0};
    if (semop(id, &op, 1) == -1) {
        perror("semop(V)");
        _exit(1);
    }
}

/* ================= UTILS ================= */

static int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

static pertes_t add_pertes(pertes_t a, pertes_t b) {
    a.morts += b.morts;
    a.blesses += b.blesses;
    a.ennemis_morts += b.ennemis_morts;
    a.prisonniers += b.prisonniers;
    return a;
}

static pertes_t zero_pertes(void) {
    pertes_t p;
    memset(&p, 0, sizeof(p));
    return p;
}

/* ================= CLEANUP (chef uniquement) ================= */

static void cleanup_ipc_chef(void) {
    if (getpid() != chef_pid) return;

    if (shm != (void *)-1) {
        shmdt(shm);
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

/* ================= SIGNALS ================= */

static void handle_sigterm(int sig) {
    (void)sig;
    _exit(0); // enfants sortent sans cleanup IPC
}

static void handle_sigint(int sig) {
    (void)sig;

    if (getpid() != chef_pid) {
        _exit(0);
    }

    write(STDOUT_FILENO, "\nArret propre de la simulation...\n", 34);

    // ignorer SIGTERM le temps de tuer le groupe
    struct sigaction sa_ign;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &sa_ign, NULL);

    // tue tout le groupe de processus
    kill(0, SIGTERM);

    // petite récolte (optionnelle, mais propre)
    // Comme SIGCHLD est ignoré, pas besoin de wait.
    cleanup_ipc_chef();
    _exit(0);
}

static void install_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM)");
        exit(1);
    }

    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        exit(1);
    }

    // évite les zombies : le système récolte automatiquement
    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = SIG_IGN;
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDWAIT;
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);
}

/* ================= COMPAGNIE ================= */

static void run_compagnie(int d, int r, int c) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    while (1) {
        pertes_t p;
        p.morts = rand_range(0, 15);
        p.blesses = rand_range(0, 20);
        p.ennemis_morts = rand_range(0, 20);
        p.prisonniers = rand_range(0, 10);

        sem_P(semid);
        shm->compagnies[d][r][c] = p;
        sem_V(semid);

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
/* calcule et écrit shm->regiments[d][r] */

static void run_regiment(int d, int r) {
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

    while (1) {
        // copie sous lock puis calcule hors lock (lock court)
        pertes_t local_comp[NB_COMP];

        sem_P(semid);
        for (int c = 0; c < NB_COMP; c++) {
            local_comp[c] = shm->compagnies[d][r][c];
        }
        sem_V(semid);

        pertes_t sum = zero_pertes();
        for (int c = 0; c < NB_COMP; c++) {
            sum = add_pertes(sum, local_comp[c]);
        }

        sem_P(semid);
        shm->regiments[d][r] = sum;
        sem_V(semid);

        // régiment non synchronisé : intervalle aléatoire léger
        usleep((useconds_t)rand_range(200, 800) * 1000);
    }
}

/* ================= DIVISION ================= */
/* calcule et écrit shm->divisions[d] */

static void run_division(int d) {
    for (int r = 0; r < NB_REG; r++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork(regiment)");
            _exit(1);
        }
        if (pid == 0) {
            srand((unsigned)time(NULL) ^ (unsigned)getpid());
            run_regiment(d, r);
            _exit(0);
        }
    }

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    while (1) {
        pertes_t local_reg[NB_REG];

        sem_P(semid);
        for (int r = 0; r < NB_REG; r++) {
            local_reg[r] = shm->regiments[d][r];
        }
        sem_V(semid);

        pertes_t sum = zero_pertes();
        for (int r = 0; r < NB_REG; r++) {
            sum = add_pertes(sum, local_reg[r]);
        }

        sem_P(semid);
        shm->divisions[d] = sum;
        sem_V(semid);

        usleep((useconds_t)rand_range(300, 1000) * 1000);
    }
}

/* ================= ARMEE ================= */

static void run_armee(void) {
    for (int d = 0; d < NB_DIV; d++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork(division)");
            exit(1);
        }
        if (pid == 0) {
            srand((unsigned)time(NULL) ^ (unsigned)getpid());
            run_division(d);
            _exit(0);
        }
    }

    while (1) {
        sleep(10);

        pertes_t local_div[NB_DIV];

        sem_P(semid);
        for (int d = 0; d < NB_DIV; d++) {
            local_div[d] = shm->divisions[d];
        }
        sem_V(semid);

        pertes_t total = zero_pertes();
        for (int d = 0; d < NB_DIV; d++) {
            total = add_pertes(total, local_div[d]);
        }

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

    // met le programme dans son propre groupe (utile pour kill(0,...))
    if (setpgid(0, 0) == -1) {
        // pas bloquant si déjà leader
    }

    install_signals();

    key_t key = ftok(".", 'A');
    if (key == (key_t)-1) {
        perror("ftok");
        // fallback possible, mais on peut aussi quitter :
        // exit(1);
        key = IPC_PRIVATE;
    }

    shmid = shmget(key, sizeof(armee_t), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    shm = (armee_t *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // init uniquement par le chef
    memset(shm, 0, sizeof(*shm));

    semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl(SETVAL)");
        exit(1);
    }

    atexit(cleanup_ipc_chef);

    run_armee();
    return 0;
}

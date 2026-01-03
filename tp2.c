#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <string.h>

// ==================== CONSTANTES ET STRUCTURES ====================

#define N 5    // Taille du tampon
#define M1 5          // Nombre de malades critiques
#define M2 6          // Nombre de malades non critiques
#define N1 3        // Nombre d'organes critiques différents
#define N2 4       // Nombre d'organes non critiques différents

// Structure pour les messages simples (num_malade, num_organe)
typedef struct {
    long mtype;       // Type du message 
    int num_malade;
    int num_organe;
} MessageSimple;

// Structure pour les messages triples (type_malade, num_malade, num_organe)
typedef struct {
    long mtype;       // Type du message
    int type_malade;  // 1 = critique, 2 = non critique
    int num_malade;
    int num_organe;
} MessageTriplet;

// Structure pour le tampon (mémoire partagée)
typedef struct {
    int type_malade;
    int num_malade;
    int num_organe;
} Organe;

// Structure pour les tableaux du donneur
typedef struct {
    int valide;       // 1 si la commande est en attente, 0 si honorée
    int num_organe;
} Commande;

// ==================== VARIABLES GLOBALES ====================

int mutex, nv;
int Qcr, Qncr, Qimp;              // Files de messages
int shmid;                        // ID de la mémoire partagée
Organe* Torgane;                  // Tampon partagé
int tube[2];                      // Tube (pipe)
int *cpt;                         // Pointeur au compteur d'organes (mémoire partagée)
int shmid_cpt;                    // ID du segment mémoire du compteur
int *head_pos;                    // Position de lecture (partagée)
int *tail_pos;                    // Position d'écriture (partagée)
int shmid_head, shmid_tail;       // IDs des segments mémoire partagée

// ==================== FONCTIONS DE BASE ====================

// Fonction pour générer un booléen aléatoire
int Agenerer() {
    return rand() % 2;
}

// Initialisation des sémaphores   
void Creer_et_initialiser_semaphores() {
    key_t key = ftok(".", 'S');
    
    // Création du sémaphore mutex
    mutex = semget(key, 1, IPC_CREAT | 0666);
    semctl(mutex, 0, SETVAL, 1);  // Initialisé à 1
    
    // Création du sémaphore nv
    nv = semget(key + 1, 1, IPC_CREAT | 0666);
    semctl(nv, 0, SETVAL, N);     // Initialisé à N (taille du tampon)
}

// Destruction des sémaphores
void Detruire_semaphores(int nv, int mutex) {
    semctl(mutex, 0, IPC_RMID, 0);
    semctl(nv, 0, IPC_RMID, 0);
}


// Opérations sur les sémaphores
void P(int semid) {
    struct sembuf op = {0, -1, 0};
    semop(semid, &op, 1);
}

void V(int semid) {
    struct sembuf op = {0, 1, 0};
    semop(semid, &op, 1);
}

// Création des files de messages
void Creer_files_messages() {
    key_t key1 = ftok(".", 'Q');
    key_t key2 = ftok(".", 'R');
    key_t key3 = ftok(".", 'I');
    
    Qcr = msgget(key1, IPC_CREAT | 0666);
    Qncr = msgget(key2, IPC_CREAT | 0666);
    Qimp = msgget(key3, IPC_CREAT | 0666);
}

// Destruction des files de messages
void Detruire_files_messages(int Qcr, int Qncr, int Qimp) {
    msgctl(Qcr, IPC_RMID, NULL);
    msgctl(Qncr, IPC_RMID, NULL);
    msgctl(Qimp, IPC_RMID, NULL);
}

// Création et attachement du tampon (mémoire partagée)
void Creer_et_attacher_tampon() {
    key_t key = ftok(".", 'T');
    shmid = shmget(key, N * sizeof(Organe), IPC_CREAT | 0666);
    Torgane = (Organe*)shmat(shmid, NULL, 0);
    
    // Initialisation du tampon
    // for(int i = 0; i < N; i++) {
    //     Torgane[i].type_malade = 0;
    //     Torgane[i].num_malade = 0;
    //     Torgane[i].num_organe = 0;
    // }
}

void Detruire_tampon(Organe* Torgane, int shmid) {
    shmdt(Torgane);
    shmctl(shmid, IPC_RMID, NULL);
}

// Destruction des mémoires partagées cpt, head_pos, tail_pos
void Detruire_shm_int(int *ptr, int shmid) {
    shmdt(ptr);
    shmctl(shmid, IPC_RMID, NULL);
}




// Création du tube
void Creer_tube() {
    if(pipe(tube) == -1) {
        perror("Erreur création tube");
        exit(1);
    }
}

// Destruction du tube
void Detruire_tube(int tube[2]) {
    close(tube[0]);
    close(tube[1]);
}

// ==================== FONCTIONS AUXILIAIRES ====================

// Fonction pour déposer un organe dans le tampon
void Deposer(Organe* org) {
    Torgane[*tail_pos].type_malade = org->type_malade;
    Torgane[*tail_pos].num_malade = org->num_malade;
    Torgane[*tail_pos].num_organe = org->num_organe;
    *tail_pos = (*tail_pos + 1) % N;
}

// Fonction pour prélever un organe du tampon
void Prelever(Organe* org) {
    org->type_malade = Torgane[*head_pos].type_malade;
    org->num_malade = Torgane[*head_pos].num_malade;
    org->num_organe = Torgane[*head_pos].num_organe;
    *head_pos = (*head_pos + 1) % N;
}

// ==================== PROCESSUS MALADE CRITIQUE ====================

void MaladeCr() {
    printf("[MaladeCr] Processus %d démarré\n", getpid());
    MessageSimple mess1;
    MessageTriplet mess2;

    srand(getpid() + time(NULL));
    int i = 0;                // Numéro de malade
    int nb_rep = 0;           // Nombre d'organes reçus
    
    while(nb_rep < M1) {
        // Générer une nouvelle commande
        if((i < M1) && Agenerer()) {
            i++;
            mess1.mtype = 1;
            mess1.num_malade = i;
            mess1.num_organe = rand() % N1 + 1;
            
            // Envoyer la commande
            msgsnd(Qcr, &mess1, sizeof(mess1) - sizeof(long), 0);
            printf("[MaladeCr] Commande envoyée: malade=%d, organe=%d\n", 
                   mess1.num_malade, mess1.num_organe);
        }
        
        // Vérifier si un organe a été implanté
        if(msgrcv(Qimp, &mess2, sizeof(mess2) - sizeof(long), 1, IPC_NOWAIT) > 0) {
           // Vérifier que c'est pour un critique
                printf("[MaladeCr] Organe reçu: malade=%d, organe=%d\n", 
                       mess2.num_malade, mess2.num_organe);
                nb_rep++;
        
        }
        
        usleep(100000);  // Pause courte
    }
    
    printf("[MaladeCr] Processus terminé (reçu %d organes)\n", nb_rep);
    exit(0);
}

// ==================== PROCESSUS MALADE NON CRITIQUE ====================

void MaladeNCr() {
    printf("[MaladeNCr] Processus %d démarré\n", getpid());
    MessageSimple mess1;
    MessageTriplet mess2;
    srand(getpid() + time(NULL));
    int i = 0;                // Numéro de malade
    int nb_rep = 0;           // Nombre d'organes reçus
    
    while(nb_rep < M2) {
        // Générer une nouvelle commande
        if((i < M2) && Agenerer()) {
            i++;
            mess1.mtype = 1;
            mess1.num_malade = i;
            mess1.num_organe = rand() % N2 + 1;
            
            // Envoyer la commande
            msgsnd(Qncr, &mess1, sizeof(mess1) - sizeof(long), 0);
            printf("[MaladeNCr] Commande envoyée: malade=%d, organe=%d\n", 
                   mess1.num_malade, mess1.num_organe);
        }
        
        // Vérifier si un organe a été implanté
        if(msgrcv(Qimp, &mess2, sizeof(mess2) - sizeof(long), 2, IPC_NOWAIT) > 0) {
              // Vérifier que c'est pour un non critique
                printf("[MaladeNCr] Organe reçu: malade=%d, organe=%d\n", 
                       mess2.num_malade, mess2.num_organe);
                nb_rep++;
            
        }
        
        usleep(150000);  // Pause un peu plus longue
    }
    
    printf("[MaladeNCr] Processus terminé (reçu %d organes)\n", nb_rep);
    exit(0);
}

// ==================== PROCESSUS CHIRURGIEN ====================

void Chirurgien() {
    printf("[Chirurgien] Processus %d démarré\n", getpid());
    
    int i1 = 0;      // Requêtes reçues des critiques
    int i2 = 0;      // Requêtes reçues des non critiques
    int nb_rep = 0;  // Commandes honorées
    
    while(nb_rep < (M1 + M2)) {
        MessageSimple mess1;
        MessageTriplet mess2;
        
        // Recevoir des commandes de malades critiques
        if((i1 < M1) && (msgrcv(Qcr, &mess1, sizeof(mess1) - sizeof(long), 0, IPC_NOWAIT) > 0)) {
            i1++;
            mess2.mtype = 1;
            mess2.type_malade = 1;
            mess2.num_malade = mess1.num_malade;
            mess2.num_organe = mess1.num_organe;
            
            // Envoyer au donneur via le tube
            write(tube[1], &mess2, sizeof(mess2));
            printf("[Chirurgien] Commande critique transmise: malade=%d, organe=%d\n", 
                   mess2.num_malade, mess2.num_organe);
        }
        
        // Recevoir des commandes de malades non critiques
        if((i2 < M2) && (msgrcv(Qncr, &mess1, sizeof(mess1) - sizeof(long), 0, IPC_NOWAIT) > 0)) {
            i2++;
            mess2.mtype = 2;
            mess2.type_malade = 2;
            mess2.num_malade = mess1.num_malade;
            mess2.num_organe = mess1.num_organe;
            
            // Envoyer au donneur via le tube
            write(tube[1], &mess2, sizeof(mess2));
            printf("[Chirurgien] Commande non critique transmise: malade=%d, organe=%d\n", 
                   mess2.num_malade, mess2.num_organe);
        }
        
        // Vérifier s'il y a des organes dans le tampon
        P(mutex);
        if(*cpt > 0) {
            V(mutex);
            
            usleep(10000);  // Petite pause pour laisser Deposer finir
            
            // Prélever un organe du tampon
            Organe org;
            Prelever(&org);
            
            P(mutex);
            *cpt = *cpt - 1;
            V(mutex);
            V(nv);  // Une place de libre dans le tampon
            
            // Envoyer l'organe pour implantation
            mess2.mtype = org.type_malade;
            mess2.type_malade = org.type_malade;
            mess2.num_malade = org.num_malade;
            mess2.num_organe = org.num_organe;
            
            msgsnd(Qimp, &mess2, sizeof(mess2) - sizeof(long), 0);
            printf("[Chirurgien] Organe implanté: type=%d, malade=%d, organe=%d\n", 
                   mess2.type_malade, mess2.num_malade, mess2.num_organe);
            
            nb_rep++;
        } else {
            V(mutex);
        }
        
        usleep(200000);
    }
    
    printf("[Chirurgien] Processus terminé (%d organes implantés)\n", nb_rep);
    exit(0);
}

// ==================== PROCESSUS DONNEUR ====================

void Donneur() {
    printf("[Donneur] Processus %d démarré\n", getpid());
    
    srand(getpid() + time(NULL));
    
    // Tableaux pour enregistrer les commandes
    Commande Tcr[M1];
    Commande Tncr[M2];
    MessageTriplet mess2;
    
    // Initialisation des tableaux
    // for(int i = 0; i < M1; i++) {
    //     Tcr[i].valide = 0;
    //     Tcr[i].num_organe = 0;
    // }
    // for(int i = 0; i < M2; i++) {
    //     Tncr[i].valide = 0;
    //     Tncr[i].num_organe = 0;
    // }
    
    int i = 0;        // Nombre de commandes reçues
    int nb_rep = 0;   // Nombre de commandes honorées
    
    while(nb_rep < (M1 + M2)) {
        
        // Recevoir une commande du chirurgien+
        if((i < (M1 + M2)) && (read(tube[0], &mess2, sizeof(mess2)) > 0)) {
            i++;
            
            if(mess2.type_malade == 1) {  // Malade critique
                if(mess2.num_malade >= 1 && mess2.num_malade <= M1) {
                    Tcr[mess2.num_malade - 1].valide = 1;
                    Tcr[mess2.num_malade - 1].num_organe = mess2.num_organe;
                    printf("[Donneur] Commande critique enregistrée: malade=%d, organe=%d\n", 
                           mess2.num_malade, mess2.num_organe);
                }
            } else if(mess2.type_malade == 2) {  // Malade non critique
                if(mess2.num_malade >= 1 && mess2.num_malade <= M2) {
                    Tncr[mess2.num_malade - 1].valide = 1;
                    Tncr[mess2.num_malade - 1].num_organe = mess2.num_organe;
                    printf("[Donneur] Commande non critique enregistrée: malade=%d, organe=%d\n", 
                           mess2.num_malade, mess2.num_organe);
                }
            }
        }
        
        // Générer aléatoirement un type de malade
        int Tmalade = rand() % 2 + 1;
        
        if(Tmalade == 1) {  // Malade critique
            int Nmalade = rand() % M1 + 1;
            int Norgane = rand() % N1 + 1;
            
            // Vérifier si cette commande existe
            if(Tcr[Nmalade - 1].valide == 1 && Tcr[Nmalade - 1].num_organe == Norgane) {
                // Organe trouvé, le déposer dans le tampon
                P(nv);  // Attendre une place vide dans le tampon
                
                Organe org;
                org.type_malade = 1;
                org.num_malade = Nmalade;
                org.num_organe = Norgane;
                
                P(mutex);
                Deposer(&org);
                *cpt = *cpt + 1;
                V(mutex);
                
                nb_rep++;
                Tcr[Nmalade - 1].valide = 0;  // Commande honorée
                printf("[Donneur] Organe critique livré: malade=%d, organe=%d\n", 
                       Nmalade, Norgane);
            }
        } else {  // Malade non critique
            int Nmalade = rand() % M2 + 1;
            int Norgane = rand() % N2 + 1;
            
            // Vérifier si cette commande existe
            if(Tncr[Nmalade - 1].valide == 1 && Tncr[Nmalade - 1].num_organe == Norgane) {
                // Organe trouvé, le déposer dans le tampon
                P(nv);  // Attendre une place vide dans le tampon
                
                Organe org;
                org.type_malade = 2;
                org.num_malade = Nmalade;
                org.num_organe = Norgane;
                
                P(mutex);
                Deposer(&org);
                *cpt = *cpt + 1;
                V(mutex);
                
                nb_rep++;
                Tncr[Nmalade - 1].valide = 0;  // Commande honorée
                printf("[Donneur] Organe non critique livré: malade=%d, organe=%d\n", 
                       Nmalade, Norgane);
            }
        }
        
        usleep(250000);
    }
    
    printf("[Donneur] Processus terminé (%d organes livrés)\n", nb_rep);
    exit(0);
}



int main(void) {
    printf("=== Systeme de Transplantation d'Organes ===\n");
    printf("Malades critiques: %d, Non-critiques: %d\n", M1, M2);
    printf("Organes critiques: %d, Non-critiques: %d\n\n", N1, N2);
    
    // Création des ressources IPC
    Creer_et_initialiser_semaphores();
    Creer_files_messages();
    Creer_et_attacher_tampon();
    Creer_tube();
    
    // Créer la mémoire partagée pour le compteur
    key_t key_cpt = ftok(".", 'C');
    shmid_cpt = shmget(key_cpt, sizeof(int), IPC_CREAT | 0666); // shmget  → crée un bloc mémoire partagé 
    cpt = (int *)shmat(shmid_cpt, NULL, 0); // shmat   → attache ce bloc à un pointeur

    *cpt = 0;
    
    // Créer la mémoire partagée pour head_pos
    // head_pos → position de lecture dans le tampon (où le chirurgien prend un organe)

    key_t key_head = ftok(".", 'H');
    shmid_head = shmget(key_head, sizeof(int), IPC_CREAT | 0666);
    head_pos = (int *)shmat(shmid_head, NULL, 0);
    *head_pos = 0;
    
    // Créer la mémoire partagée pour tail_pos
    // tail_pos → position de dépôt dans le tampon (où le donneur met un organe)
    key_t key_tail = ftok(".", 'L');
    shmid_tail = shmget(key_tail, sizeof(int), IPC_CREAT | 0666);
    tail_pos = (int *)shmat(shmid_tail, NULL, 0);
    *tail_pos = 0;
    
    printf("Ressources IPC creees\n\n");
    
    // Création des 4 processus enfants
    pid_t id;
    
    id = fork();
    if (id == 0) MaladeCr();
    
    id = fork();
    if (id == 0) MaladeNCr();
    
    id = fork();
    if (id == 0) Chirurgien();
    
    id = fork();
    if (id == 0) Donneur();
    
    // Attendre la fin des 4 processus
    for (int i = 0; i < 4; i++) {
        wait(NULL);
    }
    
    printf("\n=== Fin du programme ===\n");
    
    // Destruction des mémoires partagées cpt, head_pos, tail_pos
    Detruire_shm_int(cpt, shmid_cpt);
    Detruire_shm_int(head_pos, shmid_head);
    Detruire_shm_int(tail_pos, shmid_tail);


    // Nettoyage des ressources
    Detruire_semaphores(nv,mutex);
    Detruire_files_messages(Qcr, Qncr, Qimp);
    Detruire_tampon(Torgane, shmid);
    Detruire_tube(tube);
    
    
    printf("Ressources IPC liberees\n");
    
    return 0;
}

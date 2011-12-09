/*
 * Progetto di laboratorio di Sistemi Operativi 2010/2011
 *
 * Author: Andrea Sosso
 * Matricola: 716023
 *
 * Author: Andrea Marchetti
 * Matricola: 714467
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <stdarg.h>

/* Configurazione parametri */
#define MAX_STR 25 // Numero massimo di caratteri per pacchetto
#define MAX_PKG 100 // Numero massimo di pacchetti

/* Codici di sincronizzazione */
#define MSGKEY 4446454 // Codice coda messaggi
#define MSGTIP 5552645 // Codice TIPO per indicare i messaggi rivolti verso il server
#define SEMKEY 2225342 // Codice semafori

/* Semafori */
#define QUEUE_SEM 8 // Semaforo per coda messaggi

/* Log Path */
#define LOG_CLIENT "./log/client/" // Path log per i client

/* Definizione tipi di messaggio */
#define M_DOWN 1 // Messaggio di download - Pacchetto singolo
#define M_DALL 2  // Messaggio di download - Tutto il repo
#define M_UPD 3  // Messaggio di upload - aggiornamento pacchetto
#define M_ADD 4  // Messaggio di upload - aggiunta pacchetto
#define M_AUTH 5 // Messaggio di autorizzazione
#define M_PUSH 6 // Messaggio di push

/* Definizione valori booleani */
#define TRUE 1
#define FALSE 0

/* Struttura di un pacchetto: nome e versione */
typedef struct {
	char nome[MAX_STR];
	int ver;
} pkg;

/*
 * Strtuttura del repository: formata da un array di pacchetti
 * n: numero di pacchetti nella lista del repository
 * repo->n: corrisponde alla prima riga libera
 * lista: array di pacchetti
 */
typedef struct {
	int n;
	pkg lista[MAX_PKG];
} repo;

/* Messaggio utilizzato per scambio d'informazioni tra client e server */
typedef struct {
	long tipo; //Tipo di messaggio
	int todo; //Azione del messaggio
	int pwd; //codice di autenticazione
	int pid; //Pid del processo
	pkg pkg; //Il pacchetto
} messaggio;

/* String: Puntatore a array di caratteri */
typedef char *string;

/* Firme delle funzioni in library.c */
pkg mkpkg(char nome[], int v);
messaggio nuovo_messaggio();
int writelog(string file, string m, string tipo);
void get_date(char *buffer);
int log_date(string log, char *buff, ...);
int errorlog(string log, char *buff, ...);
int read_repo(string f);
int P(int semid, int semnum);
int V(int semid, int semnum);
int msg_send(messaggio *req, int l, int q);
int msg_receive(messaggio *req, int l, int q, int cs);

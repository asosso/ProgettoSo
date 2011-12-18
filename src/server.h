/*
 * Progetto di laboratorio di Sistemi Operativi 2010/2011
 *
 * Author: Andrea Sosso
 * Matricola: 716023
 *
 * Author: Andrea Marchetti
 * Matricola: 714467
 */
#include "library.h"

/* Beta Test */
#define QUEUE_MAX 250 // Numero massimo di messaggi da spedire contemporaneamente

/* Configurazione parametri */
#define AUTH_MAX 500 // Numero massimo di clienti upload
#define PUSH_MAX 50 // Numero massimo di clienti push
#define MAX_SERVER 5 // Massimo numero di connessioni per il server
#define BUFFER_SIZE 500 // Dimensione massima per il buffer
#define PASSWORD 234 // Codice per autenticarsi come client up

/* Aree di memoria */
#define SHMKEY 3338723 // Codice area memoria condivisa
#define BUFFER_SHMKEY 333456 // Codice area memoria condivisa per il buffer
#define LS_SHMKEY 369852 // Codice area memoria condivisa per lettori/scrittori
#define AUTH_SHMKEY 365845 // Codice area memoria condivisa autorizzazioni
#define PUSH_SHMKEY 456789 // Codice area memoria condivisa push

/* Semafori */
#define SEM_TOT 9 // Numero totale di semafori
#define SEM_SERVER 0 // Semaforo per il numero di figli del server
#define BUFFER_MUTEX 1 // Mutua esclusione per scrivere sul buffer
#define LS_MUTEX 2 // Mutua esclusione per leggere nel repository
#define LS_SCRIVI 3 // Semaforo degli scrittori per modificare il repository
#define AUTH_MUTEX 4 // Mutua esclusione per leggere i client autorizzati
#define AUTH_SCRIVI 5 // Semaforo degli scrittori per modificare i client autorizzati
#define PUSH_MUTEX 6 // Mutua esclusione per leggere i client push
#define PUSH_SCRIVI 7 // emaforo degli scrittori per modificare i client push


/* Log Path */
#define LOG_PATH "./log/server/server.log"
#define SERVER_INITIAL_REPO "input/server.repo"

/* Struttura di autorizzazione per clienti di upload */
typedef struct {
	int n; //numero client autorizzati
	int lista[AUTH_MAX]; //Lista  PID dei client autorizzati
} auth;

/* Struttura di autorizzazione per clienti di push */
typedef struct {
	int n; //numero client autorizzati
	int lista[PUSH_MAX]; //Lista PID dei client autorizzati
} push;

/* Buffer per il log */
typedef struct {
	char text[BUFFER_SIZE];
	int n;
} buffer;

/* Lettori e scrittori */
typedef struct {
	int n[SEM_TOT];
} numlettori;

/* Allocazione puntatori */
repo * rep;
numlettori * ls;
auth * aut;
push * pus;

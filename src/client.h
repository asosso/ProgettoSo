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

/* Configurazione parametri */
#define N_CLIENT 1 // Un client di default
#define MAX_TODO 100 // Numero massimo di cose da fare
#define ISTR_CLIENT "./input/"

/* Configurazione client di download */
#define FILE_D 3 // Numero di file istruzioni per il download
#define D_SEC 2 // Numero di secondi tra gli aggiornamenti del client di download

/* Configurazione client di upload */
#define FILE_U 3 // Numero di file istruzioni per l'upload

/* Firme delle funzioni in client.c */
int read_istr(int mode, string logp);
void comunica_client(messaggio *req, string logp, int l, int q);
void client_stop(string logp);

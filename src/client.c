/*
 * Progetto di laboratorio di Sistemi Operativi 2010/2011
 *
 * Author: Andrea Sosso
 * Matricola: 716023
 *
 * Author: Andrea Marchetti
 * Matricola: 714467
 */

#include "client.h"

/* Funzione utilizzata per leggere le istruzioni da un file
 * Restituisce 0 in caso di errore, 1 altrimenti.
 */
int read_istr(int mode, string logp) {
	char nome[MAX_STR], attr[MAX_STR], buf[150], istr[2];
	FILE * fp;
	string type;
	int file_n, t = 0;

	if (mode == 1) { // Download
		type = "down";
		file_n = getpid() % FILE_U;
	} else { // Upload
		type = "up";
		file_n = getpid() % FILE_U;
	}
	//Apro il file delle istruzioni
	sprintf(buf, "%s%s_%d.istr", ISTR_CLIENT, type, (file_n) + 1);
	if ((fp = fopen(buf, "r")) == NULL) { errorlog(logp, "(esegui) Impossibile aprire file istruzioni");
		return 0;
	}

	//Importo il file dei pacchetti
	while (!feof(fp) && t < MAX_TODO) {
		if (mode == 1) {
			fscanf(fp, "%s %s\n", istr, nome);
			esegui(istr, nome);
		} else {
			fscanf(fp, "%s %s %s\n", istr, nome, attr);
			esegui(istr, nome, attr);
		}
	}
	// Svuota la memoria e chiudo le connessioni con il file
	fflush(fp); fclose(fp);

	if (t >= MAX_TODO)
		errorlog(logp, "(esegui) File di istruzioni non letto completamente - troppo lungo");
	else
		log_date(logp, "(esegui) File istruzioni letto correttamente");
	return 1;
}

/* Comunica un messaggio di un client al server, ed attende una risposta */
void comunica_client(messaggio *req, string logp, int l, int q) {
	waitS(QUEUE_SEM);
	// Invio la richiesta al server
	if (msg_send(req, l, q) < 0) {
		errorlog(logp, "(shutdown) Impossibile inviare un messaggio al server");
		errorlog(logp, "(shutdown) Terminazione forzata del client");
		exit(0);
	}
	// Attendo la risposta dal server
	if (msg_receive(req, l, q, 0) < 0) {
		errorlog(logp, "(shutdown) Impossibile ricevere un messaggio dal server");
		errorlog(logp, "(shutdown) Terminazione forzata del client");
		exit(0);
	}
	signalS(QUEUE_SEM);
}

void client_stop(string logp) {
	errorlog(logp, "(shutdown) Terminazione forzata del client");
	exit(0);
}

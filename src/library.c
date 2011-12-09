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

/*
 * Funzione utilizzata per creare un nuovo pacchetto di cui nome e versione
 * sono passati come parametri.
 * Restituisce il pacchetto creato
 */
pkg mkpkg(string nome, int v) {
	pkg tmp;
	tmp.ver = v;
	strcpy(tmp.nome, nome); //Trasforma in una stringa
	return tmp;
}

/*
 * Funzione utilizzata per creare un muovo messaggio.
 * Restituisce un messaggio con tutti gli attributi settati a 0.
 */
messaggio nuovo_messaggio() {
	messaggio req;
	req.tipo = 0;
	req.todo = 0;
	req.pwd = 0;
	req.pid = 0;
	req.pkg = mkpkg("", 0);
	return req;
}

/*
 * Funzione utilizzata per scrivere su un file di log passato come parametro.
 * Inotre come parametri sono passati:
 * m: messaggio da scrivere sul file
 * tipo: modalita' di scrittura sul file
 * Restituisce 0 nel caso di erronea creazione, 1 in caso di scrittura corretta.
 */
int writelog(string file, string m, string tipo) {
	FILE *fp;
	fp = fopen(file, tipo);
	if (!fp)
		return 0;
	fprintf(fp, "%s", m);
	fclose(fp);
	return 1;
}

void get_date(char *buffer) {
	time_t timer;
	struct tm* tm_info;
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buffer, 25, "[%Y-%m-%d %H:%M:%S]", tm_info);
}

/*
 * Funzione utilizzata per creare stampare la data sul file di log passato come parametro.
 * Inoltre come parametro e' passato:
 * msg: il messaggio da scrivere sul log
 * Restituisce 0 nel caso di erronea creazione, 1 in caso di scrittura corretta.
 */
int log_date(string log, char *buff, ...) {

	char msg[500];
	va_list arglist;
	va_start(arglist,buff);
	vsprintf(msg,buff,arglist);
	va_end(arglist);

	char mydate[525];
	char buffer[25];
	get_date(buffer);

	sprintf(mydate, "%s\t%s\n", buffer, msg);
	fprintf(stdout, "%s\t%s\n", buffer, msg);
	return writelog(log, mydate, "a");
}

/*
 * Stampa gli errori,contenuti nel parametro msg, sul file di log, nel caso in cui si verifichino
 * Restituisce 0 nel caso di erronea creazione, 1 in caso di scrittura corretta.
 */
int errorlog(string log, char *buff, ...) {

	char msg[500];
	va_list arglist;
	va_start(arglist,buff);
	vsprintf(msg,buff,arglist);
	va_end(arglist);

	char tmp[510];
	fprintf(stderr, "[======ERRORE!======]\t%s\n", msg);
	sprintf(tmp, "[======ERRORE!======]\t%s\n", msg);
	return writelog(log, tmp, "a");
}

/*
 * Legge un file f contenente pacchetti, preso in input come parametro,
 * e inserisce tutti i pacchetti all'interno del repository
 * Restituisce 0 se non riesce a trovare il file, 1 altrimenti.
 */
int read_repo(string f) {
	char s[MAX_STR];
	char buf[MAX_STR + 10];int
	v;
	FILE * fp;
	sprintf(buf, "%s", f); // Apro il file dei pacchetti
	if ((fp = fopen(buf, "r")) == NULL) {
		return 0;
	}
	while (!feof(fp)) { // Importo il file dei pacchetti
		fscanf(fp, "%s %d\n", s, &v);
		addv(s, v);
	}
	fclose(fp);
	return 1;
}

/*
 * WAIT su un semaforo, decrementa di 1 il valore del semaforo
 * Parametri:
 * semid: codice dell'array di semafori
 * semnum: numero del semaforo
 * Restisuisce -1 in caso di errore, 0 altrimenti
 */
int P(int semid, int semnum) {
	struct sembuf cmd;
	cmd.sem_num = semnum;
	cmd.sem_op = -1;
	cmd.sem_flg = 0;

	//Impedisco a segnali di interrompere una semop
	int errore;
	int val;
	do {
		errno = 0;
		val = semop(semid, &cmd, 1);
		errore = errno;
	} while (errore == EINTR); // se la semop e' stata interrotta da un segnale la rieseguo

	return val;
}

/*
 * SIGNAL su un semaforo, incrementa di 1 il valore del semaforo
 * Parametri:
 * semid: codice dell'array di semafori
 * semnum: numero del semaforo
 * Restisuisce -1 in caso di errore, 0 altrimenti
 */
int V(int semid, int semnum) {
	struct sembuf cmd;
	cmd.sem_num = semnum;
	cmd.sem_op = 1;
	cmd.sem_flg = 0;

	//Impedisco a segnali di interrompere una semop
	int errore;
	int val;

	do {
		errno = 0;
		val = semop(semid, &cmd, 1);
		errore = errno;
	} while (errore == EINTR); // se la semop e' stata interrotta da un segnale la rieseguo

	return val;
}

/**
 * Funzone utilizzata per mandare un messaggio al server.
 * req: il messaggio
 * l: la lunghezza del messaggio in byte
 * q: identificatore della coda
 */
int msg_send(messaggio *req, int l, int q) {
	int ris;
	do {
		errno = 0;
		ris = msgsnd(q, req, l, 0);
	} while (errno == EINTR);
	return ris;
}

/**
 * Funzone utilizzata per ricevere un messaggio dal server.
 * req: il messaggio
 * l: la lunghezza del messaggio in byte
 * q: identificatore della coda
 */
int msg_receive(messaggio *req, int l, int q, int cs) {
	int ris = -2;
	if(cs == 0) cs = getpid();
	else cs = MSGTIP;
	do {
		errno = 0;
		ris = msgrcv(q, req, l, cs, 0);
	} while (errno == EINTR);
	return ris;
}

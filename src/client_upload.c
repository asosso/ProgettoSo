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

int padre; // pid del padre
int l = sizeof(messaggio) - sizeof(long); // lunghezza della coda di messaggi
int q; // id della coda su cui viene inviato il messaggio
int semid; // id dei semafori
int isauth = -1; // Autorizzazione a fare upload
int nfigli = N_CLIENT; // Numero di client di default

// Log system
char logp[50]; // Logpath del client

/* Crea un pacchetto da un nome e una versione
 * Nel client di upload non viene mai usata
 * */
pkg addv(string nome, int v) {
	return mkpkg(nome, v);
}

/* Imprelenta la WAIT su un semaforo */
int waitS(int semnum){
	return P(semid, semnum);
}

/* Implementa la SIGNAL su un semaforo */
int signalS(int semnum){
	return V(semid, semnum);
}

/* Comunica invia un messaggio al server passato come parametro
 * Successivamente rimane in attesa di una rispsota
 * Il semaforo serve per impostare un limite alle richieste 
 * per non intasare la coda del server
 */
void comunica(messaggio *req) {	
	comunica_client(req, logp, l, q);
}

/* Funzione che implementa la richiesta di autenticazione
 * Prende come paramentro pwd cioe' la password.
 * Restiuisce */
int reqauth(int pwd) {
	if(pwd != -1) log_date(logp, "(auth) Invio richiesta di autenticazione");

	messaggio req = nuovo_messaggio(); req.tipo = MSGTIP; req.pid = getpid();

	req.todo = M_AUTH; // Codice REQ_AUTH
	req.pwd = pwd;

	// Invia il messaggio al server e attende una risposta
	comunica(&req);

	if(pwd == -1 && req.todo > 0) log_date(logp, "(auth) Logout riuscito");
	else if(req.todo > 0) log_date(logp, "(auth) Autenticazione riuscita");
	else if (req.todo == -1) errorlog(logp, "(auth) Password errata");
	else if (req.todo == -2) errorlog(logp, "(auth) Client gia' autenticato");
	else if (req.todo == -3) errorlog(logp, "(auth) Impossibile registrarsi EXIT");
	else if (req.todo == -4) errorlog(logp, "(auth) Logout non riuscito");
	else if (req.todo == -5) errorlog(logp, "(auth) Logout fallito: autenticazione non effettuata");

	return req.todo;
}

/* Logout: comunica al server che non ho piu' la necessita di fare upload
 * Restituisce 1 in caso di successo
 * -4 In caso di fallimento
 * -5 Se il client non era autenticato
 */
int logout() {
	log_date(logp, "(auth) Invio richiesta di logout");
	return reqauth(-1);
}

/* Funzione utilizzata per eseguire l'upload del pacchetto p che ha come versione i
 * Restituisce 0 in caso di fallimento */
int send_pkg(pkg p, int i) {
	messaggio req = nuovo_messaggio(); req.tipo = MSGTIP; req.pid = getpid();

	req.todo = i;
	req.pwd = isauth;
	req.pkg = p;

	// Invia il messaggio al server e attende una risposta
	comunica(&req);

	/* Update dinamico dei pacchetti */
	if (i == M_DOWN) { // Update dinamico
		if(req.todo) {
			req.pkg.ver = req.pkg.ver + (getpid() % 10);
			return send_pkg(req.pkg, M_UPD);
		}
		errorlog(logp, "(update) Richiesta pacchetto per UPDATE DINAMICO fallita: il pacchetto non e' presente sul server");
	}

	if(req.todo > 0) {
		log_date(logp, "(update) Upload riuscito");
		return 1;
	}
	if(req.todo == -2) {
		errorlog(logp, "(auth) Upload fallito, client non autenticato");
		return 0;
	}
	if (i == M_UPD) { // Update
		if(req.todo == -1) errorlog(logp, "(update) impossibile trovare il pacchetto da aggiornare");
		if(req.todo == 0) errorlog(logp, "(update) numero di versione presente nel server maggiore o uguale di quello inviato");
	}
	if (i == M_ADD) { // Add
		if(req.todo == -1) errorlog(logp, "(add) raggiunta la quantita' massima di pacchetti nel server");
		if(req.todo == 0) errorlog(logp, "(add) esiste gia' un pacchetto con lo stesso nome");
	}

	return req.todo;
}

int aggiorna_dinamico(pkg p) {
	log_date(logp, "(update) Aggiornamento dinamico");
	return send_pkg(p, M_DOWN);
}

int aggiorna_pkg(pkg p) {
	log_date(logp, "(update) Invio un aggiornamento al server");
	return send_pkg(p, M_UPD);
}

int aggiungi_pkg(pkg p) {
	log_date(logp, "(add) Invio un nuovo pacchetto al server");
	return send_pkg(p, M_ADD);
}


void esegui(string istr, string nome, string attr) {
	if(strcmp(istr, "r") == 0) { // Registrazione
		int k = atoi(nome);
		isauth = reqauth(k);
	}
	else if(strcmp(istr, "a") == 0) {  // Add
		int k = atoi(attr);
		aggiungi_pkg(mkpkg(nome, k));
	}
	else if(strcmp(istr, "u") == 0) { // Update
		int k = atoi(attr);
		if (k == 0) {
			aggiorna_dinamico(mkpkg(nome, -1));
		} else {
			aggiorna_pkg(mkpkg(nome, k));
		}
	}
	else {
		errorlog(logp, "(esegui) %s - %s - %s non riconosciuto", istr, nome, attr);
	}
}

/* Legge le istruzioni per il file di upload */
int leggi_istruzioni() {
	return read_istr(2, logp);
}

void client_terminate() {
	client_stop(logp);
}

int main(int argc, char *argv[]) {

	signal(SIGINT, client_terminate);  // Forza lo spegnimento premendo ctrl + c

	// Connessione al server
	do {
		q = msgget(MSGKEY, 0);
		if ((!q || q < 0)) {
			errorlog(logp, "(start) In attesa di trovare un server a cui connettersi");
			sleep(2);
		}
	} while (!q || q < 0);

	//Mi connetto all'array di semafori
	if ((semid = semget(SEMKEY, 0, 0)) == -1) {
		errorlog(logp, "(start) Semafori non allocati");
		client_stop(logp);
	}

	padre = getpid();

	l = sizeof(messaggio) - sizeof(long);

	/* Client figli da generare */
	if(argc > 1) nfigli = atoi(argv[1]);
	if (nfigli < 1) {
		errorlog(logp, "(strat) Impossibile generare meno di un client");
		client_stop(logp);
	}
	int i = 0;

	for (; i < nfigli && padre && padre != -1; i++)
		padre = fork();

	// Se non posso eseguire una fork termino
	if(padre == -1) {
		errorlog(logp, "(shutdown) Impossibile eseguire la fork");
		client_stop(logp);
	}

	sprintf(logp, "%supload_%d.log", LOG_CLIENT, getpid());

	if(!padre) {
		log_date(logp,"(start) Avvio client upload %d", getpid());
		leggi_istruzioni();
		logout();
	} else { // Sono il padre
		log_date(logp,"(start) Avvio PADRE client upload %d", getpid());

		int j = 0;
		for (;j < nfigli; j++) // Aspetto che i figli muoiano
			log_date(logp, "(wait) Figlio terminato: %d", wait(0));
		log_date(logp, "(stats) %d client generati", nfigli);
		log_date(logp, "(shutdown) Shutdown PADRE client upload");
	}
	return 0;
}

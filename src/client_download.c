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
int nfigli = N_CLIENT; // Numero di client di default

// Repository
repo rep; 

// Log system
char logp[50]; // Logpath del client
char logr[50]; // Logpath del repository

// Aggiornamenti push
int alert_push = FALSE;
int fine = TRUE; // il figlio deve morire alla fine del programma?

/* waitS su un semaforo */
int waitS(int semnum) {
	return P(semid, semnum);
}

/* signalS su un semaforo */
int signalS(int semnum) {
	return V(semid, semnum);
}

/*
 * Funzione utilizzata per controllare se un pacchetto esiste nel repository locale
 * In caso negativo restituisce -1,
 * altrimenti restituisce la posizione, all'interno del repository, in cui e' memorizzato.
 */
int find(string nome) {
	int i = 0;
	for (; i < rep.n; i++)
		if (strcmp(nome, rep.lista[i].nome) == 0)
			return i;
	return -1;
}

/* Funzione utilizzata per aggiungere il pacchetto myack passato come parametro
 * all'interno del repository locale
 * Restituisce:
 * -2 se il repository ha raggiunto il numero massimo di pacchetti
 * -1 se il pacchetto che si tenta di inserire e' gia esistente
 * altrimenti ritorna il numero di pacchetti presenti nel repository */
int add(pkg mypack) {
	if (rep.n >= MAX_PKG) {
		errorlog(logp, "(add) Impossibile aggiungere ulteriori pacchetti", mypack.nome);
		return -2;
	}
	if (find(mypack.nome) == -1) { // Se il pacchetto non esiste lo aggiungo
		rep.lista[rep.n++] = mypack;
		log_date(logp, "(add) %s ver. %d", mypack.nome, mypack.ver);
		return rep.n;
	}
	errorlog(logp, "(add) Impossibile aggiungere %s, pacchetto esistente", mypack.nome);
	return -1;
}

/* Funzione che, presi come parametro nome pacchetto nome e versione v,
 * lo inserisce all'interno del repository.
 */
int addv(char nome[MAX_STR], int v) {
	return add(mkpkg(nome, v));
}

/* Aggiorna la versione del pacchetto il cui nome e' passato come parametro.
 * Restituisce:
 * 1 se il pacchetto e' stato aggiornato correttamente
 * -1 se si tenta di aggiornare il pacchetto con una versione piu' vecchia di quella presente
 * 0 se il pacchetto il cui nome e' passato come parametro non e' presente nel repository
 */
int update(pkg mypack, int k) {
	int i = find(mypack.nome);

	if (i != -1) { // Se il pacchetto e' già presente nel repository
		if (rep.lista[i].ver < mypack.ver) {
			rep.lista[i].ver = mypack.ver;
			log_date(logp, "(update) %s ver. %d aggiornato correttamente", mypack.nome, mypack.ver);
			return 1;
		}
		if (k) // Non stampare questo errore (per aggiorna_repo())
			errorlog(logp, "(update) Impossibile aggiornare il pacchetto %s. Ver. attuale (%d) - Ver. aggiornamenti (%d)", mypack.nome, rep.lista[i].ver, mypack.ver);
		return -1;
	}
	errorlog(logp, "(update) Impossibile aggiornare il pacchetto %s. Non e' presente nel repository locale",
			mypack.nome);

	return -2;
}

/* Comunica invia un messaggio al server passato come parametro
 * Successivamente rimane in attesa di una rispsota
 * Il semaforo serve per impostare un limite alle richieste 
 * per non intasare la coda del server
 */
void comunica(messaggio *req) {	
	comunica_client(req, logp, l, q);
}

/* Funzione che richiede al server il repository e aggiunge ciascun pacchetto all'interno del repository locale.
 * Restituisce 1 se l'inserimento dei pacchetti e' andato a buon fine, 0 altrimenti.
 */
int download_all() {
	log_date(logp, "(download) Richiedo al server tutti i pacchetti");

	messaggio req = nuovo_messaggio();
	req.tipo = MSGTIP;
	req.pid = getpid();
	req.todo = M_DALL; //Scarica i pacchetti

	// Invia il messaggio al server e attende una risposta
	comunica(&req);

	// Legge il repository dal file inviato dal server
	if (req.todo) {
		read_repo(logr);
		char tmp[200];
		sprintf(tmp, "rm %s", logr);
		if(system(tmp) != 0) errorlog(logp, "(download) Non sono riuscito a cancellare il file temporaneo del repository: %s", logr);
		log_date(logp, "(download) Inserimento pacchetti completato");
		return 1;
	}
	errorlog(logp, "(download) Il server non ha potuto creare il file");
	return 0;
}

/* Funzione che richiede al server un pacchetto
 * richiesta = 1 : si richiede il download
 * richiesta = 2 : si richiedono informazioni
 * richiesta = 3 : si richiede aggiornamento
 * richiesta = 4 : si richiede aggiornamento senza comunicazione di errori
 * viene usato per quando si fa l'aggiornamento completo con push attivi
 * Restituisce:
 * 1 se e' andato tutto bene
 * 0 se il pacchetto richiesto non e' presente sul server.
 * -1 se il pacchetto sul client e' piu' aggiornato del server
 * -2 se il pacchetto non e' presente sul client - solo aggiorna_repo()
 * -3 se la richiesta non e' corretta
 *  */
int richiedi(string nome, int richiesta) {
	messaggio req = nuovo_messaggio();
	req.tipo = MSGTIP;
	req.pid = getpid();

	req.todo = M_DOWN;

	req.pkg = mkpkg(nome, -1);

	// Invia il messaggio al server e attende una risposta
	comunica(&req);

	if (!req.todo) {
		errorlog(logp, "(info server) Il pacchetto %s non e' presente", nome);
		return 0;
	}

	if (richiesta == 1)
		return add(req.pkg); // Richiesta di download
	if (richiesta == 2) { // Richiedi informazioni
		log_date(logp, "(info server) %s ver. %d", req.pkg.nome, req.pkg.ver);
		return 1;
	}
	if (richiesta == 3)
		return update(req.pkg, 1); // Richiedi aggiornamento
	if (richiesta == 4)
		return update(req.pkg, 0); // Richiedi aggiornamento senza stampa errori
	return -3;
}

/* Richiede al server di scaricare il pacchetto il cui nome e' passato come paramentro */
int download(string nome) {
	log_date(logp, "(download) Richiesta download pacchetto");
	return richiedi(nome, 1);
}

/* Richiede al server informazioni sul pacchetto il cui nome e' passato come paramentro */
int get_info(string nome) {
	log_date(logp, "(info server) Richiesta di informazioni su un pacchetto");
	return richiedi(nome, 2);
}

/* Richiede al server un aggiornamento del pacchetto il cui nome e' passato come paramentro */
int aggiorna(string nome) {
	log_date(logp, "(downlaod) Richiesta di aggiornamento di un pacchetto");
	return richiedi(nome, 3);
}
/* Aggiorna tutto il repository locale per s volte attendendo D_SEC secondi tra un aggiornamento e l'altro */
void aggiorna_repo(int s) {
	int k = 0;
	do {
		log_date(logp, "(download all) Aggiornamento intero repository %d/%d", ++k, s);
		int i = 0;
		for (; i < rep.n; i++)
			richiedi(rep.lista[i].nome, 4);
		sleep(D_SEC);
	} while (k < s);
}

void handler_update(int s) {
	alert_push++;
}

void handler_term(int s) {
	fine = TRUE;
}

/* Funzione utilizzata da un client per effettuare la registrazione al servizo push */
int push() {
	log_date(logp, "(push) Invio richiesta di registrazione");

	messaggio req = nuovo_messaggio();
	req.tipo = MSGTIP;
	req.pid = getpid();

	req.todo = M_PUSH;

	// Invia il messaggio al server e attende una risposta
	comunica(&req);

	if (req.todo == -2) {
		errorlog(logp, "(push) Impossibile registrare il client %s al servizio", getpid());
		return req.todo;
	}

	if (req.todo > 0) {
		log_date(logp, "(push) client %d autenticato", getpid());
	} else if (req.todo == -1)
		errorlog(logp, "(push) client %s gia' autenticato", getpid());

	signal(SIGINT, handler_update); //Aggiornamento push
	signal(SIGTERM, handler_term); //Fine

	return req.todo;
}

/* Trasforma il client di downlaod in un demone
 * che si addormenta in attesa di un segnale da parte del server
 * Richiede al server di aggiornare tutti i pacchetti in modo da risultare
 * sempre aggiornato.
 * Fa terminare il client soltanto quando riceve un segnale di terminazione
 *  */
void daemonize() {
	while (!fine) {
		pause();
		while (alert_push > 0) {
			download_all();
			alert_push--;
		}
	}
}

void esegui(string istr, string nome) {
	if (strcmp(istr, "d") == 0)
		download(nome); // Download
	else if (strcmp(istr, "u") == 0)
		aggiorna(nome); // Update
	else if (strcmp(istr, "i") == 0)
		get_info(nome); // Info
	else if (strcmp(istr, "r") == 0) { // Aggiorna repo locale
		if (!fine)
			errorlog(logp, "(esegui) Il cliente e' registrato al servizio push, non puo' eseguire un aggiornamento");
		else {
			int k = atoi(nome);
			aggiorna_repo(k);
		}
	} else if (strcmp(istr, "p") == 0) {
		if (push() > -1)
			fine = FALSE;
	} else 
		errorlog(logp, "(esegui) Comando '%s' con argomento '%s' non riconosciuto", istr, nome);

}
int leggi_istruzioni() {
	return read_istr(1, logp);
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
		errorlog(logp, "(start) Impossibile trovare il semaforo del server");
		client_stop(logp);
	}

	// Inizializzo il repository a zero
	rep.n = 0;

	// Memorizzo il pid del padre
	padre = getpid();
	
	/* Client figli da generare */
	if (argc > 1) nfigli = atoi(argv[1]);

	if (nfigli < 1) {
		errorlog(logp, "(start) Impossibile generare meno di un client");
		client_stop(logp);
	}

	int i = 0;

	for (; i < nfigli && padre && padre != -1; i++)
		padre = fork();

	// Se non posso eseguire una fork termino
	if(padre == -1) {
		errorlog(logp, "Impossibile eseguire la fork");
		client_stop(logp);
	}

	sprintf(logp, "%sdownload_%d.log", LOG_CLIENT, getpid());
	sprintf(logr, "%s%d.repo", LOG_CLIENT, getpid());

	if (!padre) {
		log_date(logp, "(start) Avvio cliente download %d", getpid());
		// Scarico l'intero repository
		download_all();
		// Leggo le istruzioni da file
		leggi_istruzioni();
		// Se sono registrato push non termino
		if (!fine)
			daemonize();

		// Salvo il repository attuale del server
		char mypack[MAX_STR + 5];
		int s = 0;
		char pidlog[25];
		sprintf(pidlog, "%sdownload_%d.repo", LOG_CLIENT, getpid());
		log_date(pidlog, "(shutdown) Salvataggio repository su file");
		for (; s < rep.n; s++) {
			sprintf(mypack, "%s %d\n", rep.lista[s].nome, rep.lista[s].ver);
			writelog(pidlog, mypack, "a");
		}
	} else { // Sono il padre
		log_date(logp, "(start) Avvio PADRE client download %d", getpid());

		int j = 0;
		for (; j < nfigli; j++)  // Aspetto che i figli muoiano
			log_date(logp, "(wait) Figlio terminato: %d", wait(0));
		log_date(logp, "(stats) %d client generati", nfigli);
		log_date(logp, "(shutdown) Shutdown PADRE client download");
	}
	return 0;
}

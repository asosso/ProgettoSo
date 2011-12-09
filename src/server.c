/*
 * Progetto di laboratorio di Sistemi Operativi 2010/2011
 *
 * Author: Andrea Sosso
 * Matricola: 716023
 *
 * Author: Andrea Marchetti
 * Matricola: 714467
 */
#include "server.h"

int semid, shmid, shmlog, shmls, shmauth, shmpush, msgid, padre;
int msgl = sizeof(messaggio) - sizeof(long);
messaggio req;

//Statistiche
int counter_r = 0, counter_u = 0, counter_d = 0, counter_p = 0, counter_a = 0;
char start_date[25];

/* Funzione utilizzata per richiede lo spegnimento del server */
void shutdown() {
	int msgid = msgget(MSGKEY, 0);
	if (msgid == -1) {
		printf("Impossibile trovare un server da spegnere\n");
		exit(0);
	}  //Se il server non e' attivo, esco
	messaggio m1;
	m1.tipo = MSGTIP;  //Per indicare che il messaggio deve riceverlo il server
	m1.pid = -2;  //Segnale di terminazione per il server
	msgsnd(msgid, &m1, msgl, 0);

	printf("\n..Richiesta di terminazione inviata..\n");
	exit(0);
}

/* WAIT su un semaforo, decrementa di 1 il valore del semaforo.
 * return:  -1 in caso di errore, 0 altrimenti */
int waitS(int semnum) {
	return P(semid, semnum);
}

/* SIGNAL su un semaforo, incrementa di 1 il valore del semaforo.
 * return: -1 in caso di errore, 0 altrimenti */
int signalS(int semnum) {
	return V(semid, semnum);
}

/* Funzione che implementa il metodo wait del modello di sincornizzazione Lettori e Scrittori.
 * Verra'utilizzata per accedere in lettura al repository */
void waitL(int scrivi, int mutex) {
	waitS(mutex);
	ls->n[mutex]++;
	if (ls->n[mutex] == 1) waitS(scrivi);
	signalS(mutex);
}

/* Funzione che implementa il metodo signal del modello di sincornizzazione Lettori e Scrittori.
 * Verra' utilizzata per uscire dalla sezione critica del repository */
void signalL(int scrivi, int mutex) {
	waitS(mutex);
	ls->n[mutex]--;
	if (ls->n[mutex] == 0) signalS(scrivi);
	signalS(mutex);
}

/* Funzione che prendendo come parametro la posizione di un
 * pacchetto all'interno del repository ritorna il pacchetto stesso. */
pkg get_pkgid(int i) {
	pkg mypack;
	waitL(LS_SCRIVI, LS_MUTEX);
	// Sezione critica
	mypack = rep->lista[i];
	// Fine sezione critica
	signalL(LS_SCRIVI, LS_MUTEX);
	return mypack;
}

/* Controlla se un pacchetto esiste all'interno del repository.
 * In caso negativo restituisce -1, altrimenti ritorna l'id del pacchetto.
 */
int find(string nome) {
	//Non possono cercare mentre qualcuno sta scrivendo (semaforo)
	int i = 0;
	int found = -1;
	waitL(LS_SCRIVI, LS_MUTEX);
	// Sezione critica
	for (; i < rep->n && found == -1; i++)
		if (strcmp(nome, rep->lista[i].nome) == 0) {
			found = i;
		}
	// Fine sezione critica
	signalL(LS_SCRIVI, LS_MUTEX);
	return found;
}

/* Aggiunge all'interno del repository un nuovo pacchetto il cui nome e versione sono passati come parametro.
 * Se riesce a inserirlo restituisce 1, -1 altrimenti. */
int addv(string nome, int v) {
	if (find(nome) != -1) return 0;  // Il pacchetto è già esistente
	if (get_nrepo() < MAX_PKG) {  //Inserisco il pacchetto
		waitS(LS_SCRIVI);
		rep->lista[rep->n++] = mkpkg(nome, v);
		signalS(LS_SCRIVI);
		return 1;
	} else return -1;  // Superati i pacchetti massimi
}

/* Esegue l'update sul pacchetto (il cui nome e'passato come parametro) con un numero di versione specifico.
 * 1 in caso di successo
 * -1 se il pacchetto non è stato trovato
 * 0 se il numero di versione e' inferiore o uguale alla versione attuale nel repository
 * */
int update(char nome[], int v) {
	int l = find(nome);
	if (l == -1) return -1;  // Non trovato

	// Fine sezione critica
	waitS(LS_SCRIVI);
	if (rep->lista[l].ver < v) {
		rep->lista[l].ver = v;
		signalS(LS_SCRIVI);
		return 1;
	}
	signalS(LS_SCRIVI);
	return 0;
}

/* Scrive su buffer la stringa tmp passata come parametro
 * e ricopia il buffer nel file una volta che e' pieno.*/
int writebuf(char *buff, ...) {
	char tmp[500];
	va_list arglist;
	va_start(arglist,buff);
	vsprintf(tmp,buff,arglist);
	va_end(arglist);

	buffer *log;
	fprintf(stdout, "%s", tmp);
	int n = strlen(tmp);  //Lunghezza del testo inviato
	waitS(BUFFER_MUTEX);
	log = (buffer *) shmat(shmlog, NULL, 0);

	if (n < (BUFFER_SIZE - 5) - log->n) {  // Posso scrivere nel buffer
		strcpy(log->text + log->n, tmp);
		log->n += strlen(tmp);  //log->n += n(try);
	} else {  // Svuoto il buffer su file e inserisco la stirnga su buffer
		writelog(LOG_PATH, log->text, "a");
		strcpy(log->text, tmp);
		log->n = strlen(tmp);  //log->n += n(try);
	}
	shmdt(log);
	signalS(BUFFER_MUTEX);
	return 0;
}

/* Invia un segnale generico ai client che hanno fatto registrazione a servizio PUSH */
segnale(int s) {
	int i = 0;
	waitS(PUSH_MUTEX);
	for (; i < pus->n; i++)
		kill(pus->lista[i], s);
	signalS(PUSH_MUTEX);
}

/* Termina i client che hanno fatto registrazione a servizio PUSH */
void termina_push() {
	segnale(SIGTERM);
}

/* Avvisa i client che hanno fatto registrazione a servizio PUSH
 * che sono presenti nuovi aggiornamenti */
void avvisa_push() {
	segnale(SIGINT);
}

/* Funzione utilizzata per disallocare le risorse  */
void server_stop() {
	int notread = 1;
	//Stampa i messagggi in coda
	while (number_msges_in_queue(msgid) != 0) {
		msgrcv(msgid, &req, msgl, 0, 0);
		writebuf("(messaggio) %d - pid %d - todo: %d tip: %ld\n", notread++,
				req.pid, req.todo, req.tipo);
	}
	termina_push();  //Termino i push

	// Scrittura su file del buffer
	buffer *log;
	log = (buffer *) shmat(shmlog, NULL, 0);
	if (log->n > 0) writelog(LOG_PATH, log->text, "a");
	shmdt(log);

	// Salvo il repository attuale del server
	char mypack[MAX_STR + 5];int
	i = 0;
	char pidlog[25];
	sprintf(pidlog, "%s_%d.repo", LOG_PATH, padre);
	log_date(pidlog, "(shutdown) Salvataggio repository su file");
	for (; i < rep->n; i++) {
		sprintf(mypack, "%s %d\n", rep->lista[i].nome, rep->lista[i].ver);
		writelog(pidlog, mypack, "a");
	}
	
	shmdt(rep); shmdt(aut); shmdt(pus); shmdt(ls);
	
	// Rimozione risorse allocate
	if (semctl(semid, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere semafori");
	if (shmctl(shmid, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere Area di memoria Repository");
	if (shmctl(shmlog, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere Area di memoria LOG");
	if (shmctl(shmls, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere Area di memoria LETTORI/SCRITTORI");
	if (shmctl(shmauth, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere Area di memoria AUTORIZZAZIONI");
	if (shmctl(shmpush, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere Area di memoria PUSH");
	if (msgctl(msgid, 0, IPC_RMID) == -1) errorlog(LOG_PATH, "(shutdown) Impossibile rimuovere coda messaggi");

	//Statistiche
	log_date(LOG_PATH, "(stats) ");
	log_date(LOG_PATH, "(stats) %d richieste elaborate", counter_r);
	log_date(LOG_PATH, "(stats) %d richieste di download", counter_d);
	log_date(LOG_PATH, "(stats) %d richieste di upload", counter_u);
	log_date(LOG_PATH, "(stats) %d autorizzazioni client push", counter_p);
	log_date(LOG_PATH, "(stats) %d autorizzazioni client upload", counter_a);
	log_date(LOG_PATH, "(stats) Data di accensione\t --> %s", start_date);
	get_date(start_date);
	log_date(LOG_PATH, "(stats) Data di spegnimento\t --> %s", start_date);
	log_date(LOG_PATH, "(stats) ");

	log_date(LOG_PATH, "(shutdown) Spegnimento server completato");
	exit(0);
}

/* Funzione utilizzata per allocare aree di memoria e settare il valore dei semafori  */
int setup() {
	log_date(LOG_PATH, "Settaggio parametri");

	// Allocazione aree di memoria
	rep = (repo *) shmat(shmid, NULL, 0);
	ls = (numlettori *) shmat(shmls, NULL, 0);
	aut = (auth *) shmat(shmauth, NULL, 0);
	pus = (push *) shmat(shmpush, NULL, 0);

	// Impostazione a 0 delle variabili contatori
	rep->n = 0;
	aut->n = 0;
	pus->n = 0;

	int po;
	for(; po < SEM_TOT; po++)
		ls->n[po] = 0;

	// Settaggio seamfori
	semctl(semid, SEM_SERVER, SETVAL, MAX_SERVER - 1);
	semctl(semid, BUFFER_MUTEX, SETVAL, 1);
	semctl(semid, LS_SCRIVI, SETVAL, 1);
	semctl(semid, LS_MUTEX, SETVAL, 1);
	semctl(semid, AUTH_MUTEX, SETVAL, 1);
	semctl(semid, AUTH_SCRIVI, SETVAL, 1);
	semctl(semid, PUSH_MUTEX, SETVAL, 1);
	semctl(semid, PUSH_SCRIVI, SETVAL, 1);
	semctl(semid, QUEUE_SEM, SETVAL, QUEUE_MAX - 1);

	padre = getpid();

	writebuf("\n(start) Avvio del server completato: ID %d\n", padre);
	writebuf("\n(Aree di memora allocate)\nSEM: %d\nMSG: %d\nSHM: %d\nSHMLOG: %d\nSHMLS: %d\nSHMAUTH: %d\nSHMPUSH: %d\n\n",
			semid, msgid, shmid, shmlog, shmls, shmauth, shmpush);
	/* SIG_IGN al segnlae di terminazione dei figli
	 * i figli terminati non diventeranno zombie */
	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, server_stop);  // Forza lo spegnimento premendo ctrl + c
	// Lettura repository iniziale
	return read_repo(SERVER_INITIAL_REPO);
}
/* funzione utilizzate per la creazione di semafori, memorie e code di messaggi.
 * ritorna 1 se tutte le risorse sono allocate correttamente, 0 altrimenti.  */
int server_start() {
	log_date(LOG_PATH, "(start) Avvio server");
	get_date(start_date);
	// Creazione di semafori, memorie e code di messaggi
	if ((semid = semget(SEMKEY, SEM_TOT, 0600 | IPC_CREAT)) == -1) errorlog(LOG_PATH, "Semafori non allocati");

	if ((shmid = shmget(SHMKEY, sizeof(repo), 0600 | IPC_CREAT)) == -1) errorlog(
			LOG_PATH, "Area di memoria Repository non allocata");

	if ((shmlog = shmget(BUFFER_SHMKEY, sizeof(buffer), 0600 | IPC_CREAT)) == -1) errorlog(
			LOG_PATH, "Area di memoria LOG non allocata");

	if ((shmls = shmget(LS_SHMKEY, sizeof(numlettori), 0600 | IPC_CREAT)) == -1) errorlog(
			LOG_PATH, "Area di memoria LETTORI/SCRITTORI non allocata");

	if ((shmauth = shmget(AUTH_SHMKEY, sizeof(auth), 0600 | IPC_CREAT)) == -1) errorlog(
			LOG_PATH, "Area di memoria AUTORIZZAZIONI non allocata");

	if ((shmpush = shmget(PUSH_SHMKEY, sizeof(push), 0600 | IPC_CREAT)) == -1) errorlog(
			LOG_PATH, "Area di memoria PUSH non allocata");

	if ((msgid = msgget(MSGKEY, IPC_CREAT | 0600)) == -1) errorlog(LOG_PATH,
			"Coda messaggi non allocata");

	// Controllo errori all'avvio */
	if (semid == -1 || shmid == -1 || shmlog == -1 || shmls == -1 || shmauth == -1
			|| shmpush == -1 || msgid == -1) server_stop();

	if (!setup()) {
		errorlog(LOG_PATH, "(start) File del repository iniziale non trovato");
		return 0;
	}
	return 1;
}

/* Funzione utilizzata per conoscere il numero di messaggi presenti nella coda di messaggi
 * msq_id passata come parametro.
 * Restituisce il numero di messaggi.
 */
int number_msges_in_queue(int msq_id) {
	// l'operazione MSG_STAT mi permette di salvare nella struttura msg_stats lo stato
	// della coda di messaggi.
	struct msqid_ds msq_stats;
	msgctl(msq_id, MSG_STAT, &msq_stats);

	return msq_stats.msg_qnum;
}

/* Funzione utilizzata da un client per scaricare tutti i pacchetti presenti all'interno del repository. */
int download_all() {
	writebuf("(download all)\t %d Preparo il repository su file\n", req.pid);

	req.tipo = req.pid;
	char mypack[MAX_STR + 10];int
	i = 0;
	char pidlog[25];

	int check = 1;  //Se check e' falsa: problemi a scrivere sul file
	sprintf(pidlog, "%s%d.repo", LOG_CLIENT, req.pid);

	waitL(LS_SCRIVI, LS_MUTEX);
	// Inizio sezione critica
	for (; i < rep->n && check; i++) {
		sprintf(mypack, "%s %d\n", rep->lista[i].nome, rep->lista[i].ver);
		check = writelog(pidlog, mypack, "a");
	}
	// Fine sezione critica
	signalL(LS_SCRIVI, LS_MUTEX);

	// Il file e' pronto?
	if (check) {
		req.todo = 1;
		writebuf("(download all)\t %d Repository su file pronto\n", req.pid);
	} else {
		req.todo = 0;
		writebuf("(download all)\t %d Impossibile scrivere il repository su file [ERRORE]\n", req.pid);
	}

	// Risposta al cliente
	//Invio il messaggio di risposta
	msg_send(&req, msgl, msgid);

	writebuf("(download all)\t %d Repository spedito\n", req.pid);
	return req.todo;
}

/* Cerca e invia il pacchetto richiesto dal client tramite messaggio. */
int download_one() {
	writebuf("(download)\t %d Cerco il pacchetto %s\n", req.pid, req.pkg.nome);
	req.tipo = req.pid;

	// Cerco il nome richiesto nel repository
	int i = find(req.pkg.nome);

	if (i != -1) {  // Se il pacchetto esiste lo inserisco nel messaggio
		req.todo = 1;
		req.pkg = get_pkgid(i);

		writebuf("(download)\t %d Pacchetto %s trovato\n", req.pid, req.pkg.nome);
	} else {  // Altrimenti creo un pacchetto non trovato
		req.todo = 0;
		writebuf("(download)\t %d Pacchetto %s non trovato [ERRORE] \n", req.pid, req.pkg.nome);
		req.pkg = mkpkg("notfound", -1);  // Altrimenti creo un pacchetto notfound
	}

	//Invio il messaggio di risposta
	msg_send(&req, msgl, msgid);

	writebuf("(download)\t %d Pacchetto %s spedito\n", req.pid, req.pkg.nome);
	return req.todo;
}

/* Controlla se un pid e' autorizzato a effuare upload all'interno del repository. */
int is_auth(int pid) {
	int i = 0;
	int trovato = -1;
	waitL(AUTH_SCRIVI, AUTH_MUTEX);
	// Sezione critica
	for (; i < aut->n && trovato == -1; i++) {
		if (aut->lista[i] == pid) trovato = i;
	}
	// Fine sezione critica
	signalL(AUTH_SCRIVI, AUTH_MUTEX);
	return trovato;
}

/*
 * Metodo che controlla l'autenticazione, senza scorrere tutto il repository,
 * utilizzando il parametro i, cioe' la posizione della sua autorizzazione
 * all'interno dell'array di autorizzati.
 */
int check_auth(int pid, int i) {
	int trovato = 0;
	waitL(AUTH_SCRIVI, AUTH_MUTEX);
	if (i > -1 && i < aut->n)  // Per evitare valori non presenti
		if (aut->lista[i] == pid)  // Controlla se e' presente il pid
			trovato = 1;
	signalL(AUTH_SCRIVI, AUTH_MUTEX);
	return trovato;
}

/*
 * Richiesta di logout
 * Restituisce:
 * 1 in caso di successo
 * -4 nel caso in caso di fallimento
 * -5 nel caso in cui il pid non sia presente
 */
int logout(int pid) {

	waitS(AUTH_SCRIVI);

	int i = 0;

	for (; i < aut->n; i++) {
		if (aut->lista[i] == pid) {
			aut->lista[i] = aut->lista[aut->n - 1];
			aut->n--;
			signalS(AUTH_SCRIVI);
			return 1;
		}
	}
	signalS(AUTH_SCRIVI);
	// Controllo finale
	if (is_auth(pid) != -1) return -4;
	else return -5;
}

/*
 * Controlla l'autenticazione.
 * Restituisce la posizione in caso di successo
 * -1 nel caso in cui la password sia sbagliata
 * -2 se il client e' gia' autenticato
 * -3 nel caso viene raggiunto il massimo numero di client autorizzabili
 */
int login(int pid, int pwd) {
	if (pwd == PASSWORD) {
		if (is_auth(pid) == -1) {
			int result;
			waitS(AUTH_SCRIVI);
			if (aut->n < AUTH_MAX) {
				aut->lista[aut->n++] = pid;
				result = aut->n;
				result--;
			} else result = -3;
			signalS(AUTH_SCRIVI);
			return result;
		}
		return -2;
	}
	return -1;
}

/*
 * Controlla se un pid e' gia' iscritto al servizio di push.
 * ritorna l'indice dell'array nel quale e' memorizzato
 * il pid passato come parametro, -1 altrimenti.
 */
int is_push(int pid) {
	int i = 0;
	int trovato = -1;
	waitL(PUSH_SCRIVI, PUSH_MUTEX);
	// Sezione critica
	for (; i < pus->n && trovato == -1; i++) {
		if (pus->lista[i] == pid) trovato = i;
	}
	// Fine sezione critica
	signalL(PUSH_SCRIVI, PUSH_MUTEX);
	return trovato;
}

/*
 * Controlla se il processo con pid passato come parametro e' iscritto al servizio di push
 * Restituisce la posizione all'interno del repository in caso di successo
 * -1 se il client e' gia' registrato per il servizio push
 * -2 nel caso viene raggiunto il massimo numero di client push
 */
int iscrivi_push(int pid) {
	if (is_push(pid) == -1) {
		int result;
		waitS(PUSH_SCRIVI);
		if (pus->n < PUSH_MAX) {
			pus->lista[pus->n++] = pid;
			result = pus->n;
			result--;
		} else result = -2;
		signalS(PUSH_SCRIVI);
		return result;
	}
	return -1;
}

/*
 * Funzione utilizzata per registrare un nuovo client di push
 * Restituisce la posizione in caso di successo.
 * -1 se il client e' gia' registrato per il servizio push
 * -2 nel caso viene raggiunto il massimo numero di client push
 */
int registra_push() {
	writebuf("(push)\t %d Richiesta di registrazione\n", req.pid);

	req.tipo = req.pid;
	req.todo = iscrivi_push(req.pid);

	if (req.todo == -1)
		writebuf("(push)\t %d Client push gia' autenticato [ERRORE]\n", req.pid);
	else if (req.todo == -2) 
		writebuf("(push)\t %d Impossibile aggiungere ulteriori client [ERRORE]\n",
				req.pid);
	else 
		writebuf("(push)\t %d Client registrato: %d\n", req.pid,
				req.todo);

	//Invio il messaggio di risposta
	msg_send(&req, msgl, msgid);

	writebuf("(push)\t %d Report autorizzazione inviato\n", req.pid);

	return req.todo;
}

/*
 * Funzione utilizzata per autenticare un client, in modo che possa eseguire l'upload.
 * Restituisce la posizione in caso di successo
 * -1 nel caso in cui la password sia sbagliata
 * -2 se il client è già autenticato
 * -3 nel caso viene raggiunto il massimo numero di client autorizzabili
 * -4 solo nel caso di richiesta di logout se fallisce
 * -5 se non ha trovato il pid con cui fare il logout
 */
int autentica() {
	req.tipo = req.pid;

	// Controllo se la richiesta e' di logout
	if (req.pwd == -1) {
		writebuf("(auth)\t\t %d Richiesta di logout\n", req.pid);

		req.todo = logout(req.pid);

		//Invio il messaggio di risposta
		msg_send(&req, msgl, msgid);

		writebuf("(auth)\t\t %d Logout inviato\n", req.pid);

		return req.todo;
	}

	writebuf("(auth)\t\t %d Richiesta di autenticazione\n", req.pid);

	req.todo = login(req.pid, req.pwd);

	if (req.todo == -1) writebuf("(auth)\t\t %d Password errata [ERRORE]\n", req.pid);
	else if (req.todo == -2) writebuf("(auth)\t\t %d Client già autenticato [ERRORE]\n", req.pid);
	else if (req.todo == -3) writebuf("(auth)\t\t %d Impossibile aggiungere ulteriori client [ERRORE]\n", req.pid);
	else writebuf("(auth)\t\t %d Autorizzazione n. %d\n", req.pid, req.todo);

	//Invio il messaggio di risposta
	msg_send(&req, msgl, msgid);

	writebuf("(auth)\t\t %d Report autenticazione inviato \n", req.pid);
	return req.todo;
}

/*
 * Resitituisce il numero di pacchetti
 * presente all'interno del repository.
 */
int get_nrepo() {
	int n;
	waitL(LS_SCRIVI, LS_MUTEX);
	//Sezione critica
	n = rep->n;
	//Fine sezione critica
	signalL(LS_SCRIVI, LS_MUTEX);
	return n;
}

/*
 * Funzione utilizzata per riceve un pacchetto dal client.
 * Restituisce:
 *  0 se esiste gia' un pacchetto con quel nome o il numero di versione e inferiore
 * -1 se raggiunta la quantita' massima di pacchetti
 * -2 se non e' stato effettuato il login
 */
int receive_pkg(int mode) {
	writebuf("(receive)\t %d Ricevo il pacchetto %s\n", req.pid, req.pkg.nome);
	req.tipo = req.pid;

	if (check_auth(req.pid, req.pwd) || is_auth(req.pid) != -1) {  //Controllo l'autorizzazione con la funzione breve o con la ricerca
		if (mode == M_ADD) {
			req.todo = addv(req.pkg.nome, req.pkg.ver);
			if (req.todo == -1) writebuf("(add)\t\t %d Raggiunta la quantita' massima di pacchetti. %s non inserito [ERRORE]\n", req.pid, req.pkg.nome);
			else if (req.todo == 0) writebuf("(add)\t\t %d %s Esiste gia' nel server. [ERRORE] \n", req.pid, req.pkg.nome);
			else {	
				writebuf("(add)\t\t %d %s aggiunto correttamente\n", req.pid, req.pkg.nome);
				avvisa_push();  //Invio un segnale ai client push
			}
		} else {
			req.todo = update(req.pkg.nome, req.pkg.ver);
			if (req.todo == -1)
				writebuf("(update)\t %d %s non e' presente sul server [ERRORE]\n",req.pid, req.pkg.nome);
			else if (req.todo == 0) 
				writebuf("(update)\t %d %s ver. %d appena inviato e' meno aggiornato di quello nel server [ERRORE]\n",
						req.pid, req.pkg.nome, req.pkg.ver);
			else {
				writebuf("(update)\t %d %s aggiornato correttamente\n", req.pid, req.pkg.nome);
				avvisa_push();  //Invio un segnale ai client push
			}
		}
	} else {
		req.todo = -2;
		writebuf("(receive)\t %d Non autenticato [ERRORE]\n", req.pid);
	}
	//Invio il messaggio di risposta
	msg_send(&req, msgl, msgid);
	writebuf("(receive)\t %d Report inviato\n", req.pid);

	return req.todo;
}

int aggiungi_pkg() {
	return receive_pkg(M_ADD);
}

int aggiorna_pkg() {
	return receive_pkg(M_UPD);
}

int main(int argc, char *argv[]) {

	/* Richiesta di shutdown server */
	if (argc > 1) if (!strcmp(argv[1], "shutdown") || !strcmp(argv[1], "-f")) shutdown();
	else {
		printf("Errore parametro in input\n\n");
		exit(0);
	}

	/* Avvio del server */
	server_start();
	int termina = FALSE;
	do {
		// Sono il padre
		if (termina && number_msges_in_queue(msgid) == 0)
			server_stop();
		if (padre == -1) {
			errorlog(LOG_PATH, "Impossibile eseguire la fork");
			server_stop();
		}
		// In attesa di messaggi dal client
		if(msg_receive(&req, msgl, msgid, 1) < 0) {
			errorlog(LOG_PATH, "Impossibile ricevere messaggi");
			server_stop();
		}
		if (req.pid == -2)
			termina = TRUE;
		else {
			// Statistiche
			counter_r++;
			if(req.todo == M_DOWN || req.todo == M_DALL)
				counter_d++;
			else if(req.todo == M_ADD || req.todo == M_UPD)
				counter_u++;
			else if(req.todo == M_PUSH)
				counter_p++;
			else if(req.todo == M_AUTH && req.pwd != -1) //Non conto i logout
				 counter_a++;
			// Fine Statistiche

			waitS(SEM_SERVER);  //WAIT su semaforo per avere al massimo 5 figli del server
			padre = fork();
		}
	} while (padre);

	/* Sono il figlio */
	if (!padre) {
		// Invio di un pacchetto dal server al client
		if (req.todo == M_DOWN) download_one();

		// Riceve un pacchetto dal client da aggiungere
		else if (req.todo == M_ADD) aggiungi_pkg();

		// Riceve un pacchetto dal client da aggiornare
		else if (req.todo == M_UPD) aggiorna_pkg();

		// Richiesta di tutto il repository da parte del cliente
		else if (req.todo == M_DALL) download_all();

		// Autenticazione clienti di upload
		else if (req.todo == M_AUTH) autentica();

		// Registrazione client push
		else if (req.todo == M_PUSH) registra_push();

		else writebuf("(unknow)\t %d Richiesta non riconosciuta [ERRORE] \n", req.pid);

		//WAIT su semaforo per lasciare la possibilità ad un altro figlio del server di generarsi
		signalS(SEM_SERVER);
	}
	return 0;
}

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

// ---------- COSTANTI ----------
#define MAX_NAME_LENGTH 20		//dimensione massima dei nomi o parametri di una richiesta
#define MAX_DESCRIPTION_LENGTH 1024	//dimensione massima di una descrizione
#define MAX_INVENTORY_SIZE 5		//dimensione dell'inventario
#define MAX_LOCATIONS 10		//massimo numero di location in una stanza
#define MAX_OBJECTS 20			//massimo numero di oggetti in una stanza
#define MAX_ENIGMI 20			//massimo numero di enigmi in una stanza
#define MAX_CONNESSIONI 50		//massimo numero di sessioni che il server gestirà
#define REQ_SIZE 48			//lunghezza di una richiesta calcolata come CMD_LENGTH + 2 * MAX_NAME_LENGTH + 2

// ---------- STRUTTURE DATI ----------
struct account{
	char utente[MAX_NAME_LENGTH];
	char password[MAX_NAME_LENGTH];
};
struct object{
	char nome[MAX_NAME_LENGTH];
	char descrizione[MAX_DESCRIPTION_LENGTH];
	int enigma_bloccante;				//indice dell'enigma nell'array enigmi
	int pickup;					//1 se si può raccogliere, 0 altrimenti
};
/* 0 per indovinello, 1 per utilizzo di oggetto*/
struct enigma{
	int tipo;					//0 indovinello, 1 legato al comando use
	char descrizione[MAX_DESCRIPTION_LENGTH];	//se tipo 0 contiene l'indovinello, altrimenti la nuova descrizione di target
	char* risposta;					//risposta dell'indovinello
	char* errore;					//cosa mostrare in caso di errore
	char* risolto;					//cosa mostrare dopo aver risolto l'enigma
	int obj;					//se tipo 1 è l'indice dell'oggetto da usare nel comando use
	int target;					//indice dell'oggetto il quale cambierà la descrizione
	int token;					//numero di token che l'enigma vale
};
struct location{
	char nome[MAX_NAME_LENGTH];
	char descrizione[MAX_DESCRIPTION_LENGTH];
};
struct partita{
	int id;
	char nome[MAX_NAME_LENGTH];
	int inventario[MAX_INVENTORY_SIZE];		//contiene gli indici degli oggetti nell'inventario
	struct location* locations[MAX_LOCATIONS];	//contiene le locations della partita
	struct object* objects[MAX_OBJECTS];		//contiene gli oggetti della partita
	struct enigma* enigmi[MAX_ENIGMI];		//contiene gli enigmi della partita
	struct timeval start;				//istante di inizio della partita
	uint16_t tempo_massimo;				//secondi massimi di gioco
	int tokens_taken;				//tokens raccolti
	int max_tokens;					//tokens totali
	int port1;
	int port2;
};
struct sessione{
	int socket;
	struct account* acc;
	struct partita* pa;
	uint16_t msg_port;
	int nplayer;
};

// ---------- DICHIARAZIONI FUNZIONI ----------
void check(int r);
void msg_comandi();
void aggiungi_fd(int fd);
int salva_account(struct account* acc);
int esiste_account(struct account* acc);
int cerca_sessione_libera();
int cerca_sessione(int socket);
void chiudi_sessione(int socket);
void chiudi_tutto();
int esiste_partita_in_corso();
int tempo_in_partita(struct timeval* start, uint16_t max);
int cerca_object(char* name, struct partita* current);
int cerca_location(char* name, struct partita* current);
int cerca_in_inventario(int obj, struct partita* current);

void gestisci_comando();
void start_server(int port);
void stop_server();
void nuova_connessione();
void registrazione(int narg, char* utente, char* password, int socket);
void login(int narg, char* utente, char* password, struct sessione* current);
void manda_mappe(int socket);

void genera_partita(char* nome, struct partita* out);
void start(int narg, char* mappa, struct sessione* current);
void look(int narg, char* what, struct sessione* current);
void take(int narg, char* what, struct sessione* current);
void use(int narg, char* what1, char* what2, struct sessione* current);
void objs(struct sessione* current);
void chiudi_partita(struct sessione* current);
void enigma(int narg, char* what, struct sessione* current);
void risolvi(int narg, char* what, char* solution, struct sessione* current);
void send_time(struct sessione* current);
void send_tokens(struct sessione* current);
void deposit(int narg, char* arg2, struct sessione* current);

void setport(int narg, char* arg2, struct sessione* current);
void send_port(struct sessione* current);

// ---------- VARIABILI GLOBALI ----------
int fine = 0;
fd_set master;
int fd_max;
int listener = -1;
FILE* account_fd;
struct sessione* sessioni[MAX_CONNESSIONI];

// ---------- MAIN ----------
int main(){
	fd_set read_fds;
	int i, narg;
	char richiesta[REQ_SIZE];		//contiene la richiesta del client
	char arg1[6];				//contiene il comando
	char arg2[MAX_NAME_LENGTH];		//contiene il primo parametro
	char arg3[MAX_NAME_LENGTH];		//contiene il secondo parametro
	char risp[2];				//contiene il codice di risposta al client
	struct sessione* current;		//puntatore alla sessione del client che ha fatto la richiesta

	//inizializzazione set di descrittori
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(STDIN_FILENO, &master);
	fd_max = STDIN_FILENO;

	//apertura file contenente gli accounts
	account_fd = fopen("accounts.txt", "a+");

	//per far funzionare lo script exec2024.sh
	start_server(4242);

	//guida dei comandi a video
	msg_comandi();

	do{
		read_fds = master;
		select(fd_max + 1, &read_fds, NULL, NULL, NULL);
		for(i = 0; i <= fd_max; i++){
			if(FD_ISSET(i, &read_fds)){
				//comando da terminale pronto
				if(i == STDIN_FILENO){
					gestisci_comando();
					continue;
				}
				
				//creazione di una nuova sessione
				if(i == listener){
					nuova_connessione();
					continue;
				}
				
				//parte riguardante client già aventi una sessione

				//recupero sessione corrispondente al socket
				current = sessioni[cerca_sessione(i)];

				//gestione di una normale richiesta nella forma: arg1 [arg2] [arg3]
				memset(richiesta, 0, sizeof(richiesta));
				check(recv(i, richiesta, sizeof(richiesta), 0));
				narg = sscanf(richiesta, "%s %s %s", arg1, arg2, arg3);

				printf("[INFO] il socket %d ha effettuato la richiesta %s\n", i, richiesta);

				//signup
				if(strncmp(arg1, "signup", 6) == 0){
					registrazione(narg - 1, arg2, arg3, i);
					continue;
				}

				//login
				if(strncmp(arg1, "login", 5) == 0){
					login(narg - 1, arg2, arg3, current);
					continue;
				}

				//logout
				if(strncmp(arg1, "logout", 6) == 0){
					chiudi_sessione(i);
					continue;
				}

				//rooms
				if(strncmp(arg1, "rooms", 5) == 0){
					manda_mappe(i);
					continue;
				}

				//start
				if(strncmp(arg1, "start", 5) == 0){
					start(narg - 1, arg2, current);
					continue;
				}

				//look
				if(strncmp(arg1, "look", 4) == 0){
					look(narg - 1, arg2, current);
					continue;
				}

				//take
				if(strncmp(arg1, "take", 4) == 0){
					take(narg - 1, arg2, current);
					continue;
				}

				//use
				if(strncmp(arg1, "use", 3) == 0){
					use(narg - 1, arg2, arg3, current);
					continue;
				}

				//objs
				if(strncmp(arg1, "objs", 4) == 0){
					objs(current);
					continue;
				}

				//end
				if(strncmp(arg1, "end", 3) == 0){
					chiudi_partita(current);
					continue;
				}

				//enigma
				if(strncmp(arg1, "enigma", 6) == 0){
					enigma(narg - 1, arg2, current);
					continue;
				}

				//unlock
				if(strncmp(arg1, "unlock", 6) == 0){
					risolvi(narg - 1, arg2, arg3, current);
					continue;
				}

				//time
				if(strncmp(arg1, "time", 4) == 0){
					send_time(current);
					continue;
				}

				//tokens
				if(strncmp(arg1, "tokens", 6) == 0){
					send_tokens(current);
					continue;
				}

				//deposit
				if(strncmp(arg1, "dep", 3) == 0){
					deposit(narg - 1, arg2, current);
					continue;
				}

				//port
				if(strncmp(arg1, "port", 4) == 0){
					send_port(current);
					continue;
				}

				//setport
				if(strncmp(arg1, "setp", 4) == 0){
					setport(narg - 1, arg2, current);
					continue;
				}

				printf("[WARN] il socket %d ha inviato una richiesta invalida\n", i);
				strcpy(risp, "ER");
				check(send(i, risp, 2,0));
			}
		}
	}while(fine == 0);
	return 0;
}
// -----------------------------------------
// |          FUNZIONI DI UTILITA          |
// -----------------------------------------

//controlla la presenza di errori
void check(int r){
	if(r < 0){
		perror("[ERROR] errore fatale");
		printf("[ERROR] chiusura server\n");
		chiudi_tutto();
		if(listener >= 0)
			close(listener);
		fclose(account_fd);
		exit(-1);
	}
}

//stampa a video i comandi disponibili del server
void msg_comandi(){
	printf("\n\n********** SERVER STARTED **********\n\n");
	printf("Digita un comando:\n\n");
	printf("1) start <port> --> avvia il server di gioco\n2) stop --> termina il server\n");
	printf("************************************\n");
}

//gestisce l'aggiunta di un descrittore la set dei descrittori
void aggiungi_fd(int fd){
	FD_SET(fd, &master);
	if(fd > fd_max)
		fd_max = fd;
}

//salva il nuovo account creato se non già esistente
int salva_account(struct account* acc){
	if(esiste_account(acc) >= 1)
		return 0;
		
	fprintf(account_fd, "%s %s\n", acc->utente, acc->password);
	return 1;
}

//controlla l'esistenza di un account, ritorna:
//0 account non esistente, 1 account esistente e password sbagliata, 2 account esistente e password giusta
int esiste_account(struct account* acc){
	char utente[MAX_NAME_LENGTH];
	char password[MAX_NAME_LENGTH];
	fpos_t pos;

	fgetpos(account_fd, &pos);
	rewind(account_fd);
	while(fscanf(account_fd, "%s %s", utente, password) != EOF){
		if(strcmp(utente, acc->utente) == 0){
			fsetpos(account_fd, &pos);
			if(strcmp(password, acc->password) == 0)
				return 2;
			return 1;
		}
	}
	return 0;
}

//ritorna l'indice di un posto libero in sessioni[]
int cerca_sessione_libera(){
	int i;
	for(i = 0; i < MAX_CONNESSIONI; i++){
		if(sessioni[i] == NULL)
			return i;
	}
	return -1;
}

//ritorna l'indice della sessione corrispondente al socket in sessioni[]
int cerca_sessione(int socket){
	int i;
	for(i = 0; i < MAX_CONNESSIONI; i++){
		if(sessioni[i] != NULL && sessioni[i]->socket == socket)
			return i;
	}
	return -1;
}

//chiude una sessione e libera la memoria che occupa
void chiudi_sessione(int socket){
	int i;

	for(i = 0; i < MAX_CONNESSIONI; i++){
		if(sessioni[i] != NULL && sessioni[i]->socket == socket){
			close(socket);
			free(sessioni[i]->acc);
			free(sessioni[i]->pa);
			free(sessioni[i]);
			sessioni[i] = NULL;
			FD_CLR(socket, &master);
			printf("[INFO] La sessione %d del socket %d è stata chiusa\n", i, socket);
			return;
		}
	}
}

//chiude tutte le sessioni
void chiudi_tutto(){
	int i;
	for(i = 0; i < MAX_CONNESSIONI; i++){
		if(sessioni[i] != NULL){
			close(sessioni[i]->socket);
			free(sessioni[i]->acc);
			free(sessioni[i]->pa);
			free(sessioni[i]);
			sessioni[i] = NULL;
		}
	}
}

//controlla l'esistenza di partite in corso
int esiste_partita_in_corso(){
	int i;
	for(i = 0; i < MAX_CONNESSIONI; i++){
		if(sessioni[i] != NULL && (*sessioni[i]).pa != NULL)
			return 1;
	}
	return 0;
}

//restituisce il tempo rimasto alla partita in secondi
int tempo_in_partita(struct timeval* start, uint16_t max){
	int passato;
	struct timeval ora;

	gettimeofday(&ora, NULL);
	passato = ora.tv_sec - start->tv_sec;
	if(passato < max)
		return max - passato;
	return 0;
}

//ricerca un oggetto all'interno della partita data
int cerca_object(char* name, struct partita* current){
	struct object* curr_obj;
	int i;
	
	for(i = 0; i < MAX_OBJECTS; i++){
		curr_obj = current->objects[i];
		if(curr_obj != NULL && strcmp(curr_obj->nome, name) == 0)
			return i;
	}
	return -1;
}

//ricerca una location all'interno della partita data
int cerca_location(char* name, struct partita* current){
	struct location* curr_loc;
	int i;

	for(i = 0; i < MAX_LOCATIONS; i++){
		curr_loc = current->locations[i];
		if(curr_loc != NULL && strcmp(curr_loc->nome, name) == 0)
			return i;
	}
	return -1;
}

//ricerca un oggetto nella partita data
int cerca_in_inventario(int obj, struct partita* current){
	int i;

	for(i = 0; i < MAX_INVENTORY_SIZE; i++){
		if(current->inventario[i] == obj)
			return i;
	}
	return -1;
}

// ----------------------------------------
// |          FUNZIONI PRINCIPALI         |
// ----------------------------------------

//gestisce i comandi inseriti dal terminale del server
void gestisci_comando(){
	char cmd[20];
	int cmdval = 0;

	memset(cmd, 0, sizeof(cmd));
	scanf("%s", cmd);

	//comando stop
	if(strcmp(cmd, "stop") == 0){
		stop_server();
	}

	//comando start
	if(strncmp(cmd, "start", 5) == 0){
		scanf("%s", &cmd[6]);
		start_server(atoi(&cmd[6]));
		cmdval = 1;
	}

	if(cmdval == 0){
		printf("comando sconosciuto\n");
	}
}

//avvia il server sulla porta specificata
void start_server(int port){
	int res, i;
	struct sockaddr_in addr;

	if(listener != -1){
		printf("[WARN] server già avviato\n");
		return;
	}

	printf("[INFO] avvio server di gioco\n");
	
	//creazione listener
	listener = socket(AF_INET, SOCK_STREAM, 0);
	check(listener);
	aggiungi_fd(listener);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	res = bind(listener, (struct sockaddr*) &addr, sizeof(addr));
	check(res);
	res = listen(listener, 10);
	check(res);

	//strutture per le sessioni
	for(i = 0; i < MAX_CONNESSIONI; i++)
		sessioni[i] = NULL;
	
	printf("[INFO] server avviato\n");

}

//chiude il server se non ci sono partite in corso
void stop_server(){
	if(esiste_partita_in_corso() == 1){
		printf("impossibile chiudere il server: ci sono partite in corso\n");
		return;
	}
	chiudi_tutto();
	if(listener >= 0)
		close(listener);
	fclose(account_fd);
	
	exit(0);
}

//creazione di una nuova sessione
void nuova_connessione(){
	int cdes, i, v = 1;
	socklen_t csize;
	struct sockaddr_in client_addr;

	i = cerca_sessione_libera();
	if(i < 0){
		printf("[WARN] raggiunto il limite massimo di connessioni\n");
		close(accept(listener, (struct sockaddr*) &client_addr, &csize));
		return;
	}

	csize = sizeof(client_addr);
	cdes = accept(listener, (struct sockaddr*) &client_addr, &csize);
	aggiungi_fd(cdes);

	//inizializzazione della sessione
	sessioni[i] = (struct sessione*) malloc(sizeof(struct sessione));
	sessioni[i]->socket = cdes;
	sessioni[i]->acc = NULL;
	sessioni[i]->pa = NULL;

	//forzare la chiusura del socket alla conclusione della connessione
	setsockopt(cdes, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int));
	printf("[INFO] Nuova connessione al socket %d, assegnata la sessione %d\n", cdes, i);
}

//registrazione di un nuovo account
void registrazione(int narg, char* utente, char* password, int socket){
	char risp[2];
	struct account* acc;

	acc = (struct account*) malloc(sizeof(struct account));
	strcpy(acc->utente, utente);
	strcpy(acc->password, password);

	if(narg != 2 || salva_account(acc) == 0){
		//account esistente o errore parametri
		strcpy(risp, "ER");
	}
	else{
		strcpy(risp, "OK");
		printf("[INFO] il socket %d ha creato un nuovo account\n", socket);
	}
	free(acc);
	send(socket, risp, 2, 0);
}

//autenticazione di un client
void login(int narg, char* utente, char* password, struct sessione* current){
	struct account* acc;
	char risp[2];
	int esito;

	acc = (struct account*) malloc(sizeof(struct account));
	strcpy(acc->utente, utente);
	strcpy(acc->password, password);

	esito = esiste_account(acc);
	if(esito == 2){
		current->acc = acc;
		strcpy(risp, "OK");
		printf("[INFO] il socket %d ha effettuato il login all'account %s\n", current->socket, utente);
	}
	else if(esito == 1){
		strcpy(risp, "PE");
		free(acc);
	}
	else{
		strcpy(risp, "ER");
		free(acc);
	}

	send(current->socket, risp, sizeof(risp), 0);
}

//manda al client la lista di stanze disponibili
void manda_mappe(int socket){
	FILE* lista;
	int i;
	uint8_t n;
	char buf[MAX_NAME_LENGTH];

	lista = fopen("lista.txt", "r");
	memset(buf, 0, sizeof(buf));

	//prendo numero di rooms
	fgets(buf, MAX_NAME_LENGTH, lista);
	n = atoi(buf);

	//mando il numero
	send(socket, (void*) &n, sizeof(n), 0);

	//mando rooms
	for(i = 0; i < n; i++){
		fgets(&buf[0], MAX_NAME_LENGTH, lista);
		send(socket, &buf[0], MAX_NAME_LENGTH, 0);
	}
	fclose(lista);
	printf("[INFO] il socket %d ha richiesto la lista delle mappe\n", socket);
}

// ---------------------------------------------
// |          PARTE DEDICATA AL GIOCO          |
// ---------------------------------------------

// ---------- GENERAZIONE PARTITA ----------
// estrae le informazioni di una stanza dal file corrispondente ed inizializza la partita
void genera_partita(char* nome, struct partita* out){
	char nome_file[50];
	char tmp[MAX_DESCRIPTION_LENGTH];
	FILE* mappa;
	int i, n;

	printf("[INFO] generazione della room %s in corso\n", nome);

	memset(nome_file, 0, sizeof(nome_file));
	strcpy(nome_file, nome);
	strcat(nome_file, ".txt");
	mappa = fopen(nome_file, "r");

	//tempo di partenza
	gettimeofday(&(out->start), NULL);

	//inizializzazione contatore
	out->tokens_taken = 0;

	//nome
	fgets(out->nome, MAX_NAME_LENGTH, mappa);

	//inizializzazione inventario
	for(i = 0; i < MAX_INVENTORY_SIZE; i++)
		out->inventario[i] = -1;

	//numero di tokens totali
	fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
	out->max_tokens = atoi(tmp);

	//tempo massimo in partita
	fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
	out->tempo_massimo = atoi(tmp);

	//numero di oggetti
	fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
	n = atoi(tmp);

	//oggetti
	for(i = 0; i < MAX_OBJECTS; i++){
		struct object* obj;
		if(i >= n){
			out->objects[i] = NULL;
			continue;
		}
		obj = (struct object*) malloc(sizeof(struct object));
		fgets(obj->nome, MAX_NAME_LENGTH, mappa);
		obj->nome[strcspn(obj->nome, "\n")] = 0;
		fgets(obj->descrizione, MAX_DESCRIPTION_LENGTH, mappa);
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		obj->enigma_bloccante = atoi(tmp);
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		obj->pickup = atoi(tmp);
		out->objects[i] = obj;
	}

	//numero locations
	fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
	n = atoi(tmp);

	//locations
	for(i = 0; i < MAX_LOCATIONS; i++){
		struct location* loc;

		if(i >= n){
			out->locations[i] = NULL;
			continue;
		}
		loc = (struct location*) malloc(sizeof(struct location));
		fgets(loc->nome, MAX_NAME_LENGTH, mappa);
		loc->nome[strcspn(loc->nome, "\n")] = 0;
		fgets(loc->descrizione, MAX_DESCRIPTION_LENGTH, mappa);

		out->locations[i] = loc;
	}

	//numero enigmi
	fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
	n = atoi(tmp);
	for(i = 0; i < MAX_ENIGMI; i++){
		struct enigma* eni;

		if(i >= n){
			out->enigmi[i] = NULL;
			continue;
		}
		eni = (struct enigma*) malloc(sizeof(struct enigma));

		//tipo
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		eni->tipo = atoi(tmp);

		//descrizione
		fgets(eni->descrizione, MAX_DESCRIPTION_LENGTH, mappa);

		//risposta
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		tmp[strcspn(tmp, "\n")] = 0;
		if(strcmp(tmp, "NULL") == 0){
			eni->risposta = NULL;
		}
		else{
			eni->risposta = (char*) malloc(MAX_NAME_LENGTH);
			strcpy(eni->risposta, tmp);
		}

		//errore
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		if(strcmp(tmp, "NULL") == 0){
			eni->errore = NULL;
		}
		else{
			eni->errore = (char*) malloc(MAX_DESCRIPTION_LENGTH);
			strcpy(eni->errore, tmp);
		}

		//risolto
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		if(strcmp(tmp, "NULL") == 0){
			eni->risolto = NULL;
		}
		else{
			eni->risolto = (char*) malloc(MAX_DESCRIPTION_LENGTH);
			strcpy(eni->risolto, tmp);
		}

		//obj
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		eni->obj = atoi(tmp);

		//target
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		eni->target = atoi(tmp);

		//token
		fgets(tmp, MAX_DESCRIPTION_LENGTH, mappa);
		eni->token = atoi(tmp);

		out->enigmi[i] = eni;
	}
	fclose(mappa);
}

// ---------------------------------------------------------------
// |          GESTIONE RICHIESTE RIGUARDANTI LE PARTITE          |
// ---------------------------------------------------------------

// ---------- START ----------
//inizializza la partita richiesta dal client
void start(int narg, char* mappa, struct sessione* current){
	int max, id_mappa, i;
	struct partita* part;
	char risp[2];
	char nome[MAX_NAME_LENGTH];
	FILE* indice_mappe;

	indice_mappe = fopen("lista.txt", "r");
	fscanf(indice_mappe, "%d", &max);

	//controlli formato richiesta
	if(narg == 1){
		id_mappa = atoi(mappa);
	}
	if(current->acc == NULL || current->pa != NULL || narg != 1 || max < id_mappa + 1){
		//login non fatto o partita già avviata o argomenti invalidi
		strcpy(risp, "ER");
		send(current->socket, risp, 2, 0);
		fclose(indice_mappe);
		return;
	}

	//ricerca di una partita già avviata senza giocatore 2
	for(i = 0; i < MAX_CONNESSIONI; i++){
		if(sessioni[i] != NULL && sessioni[i]->pa != NULL && sessioni[i]->pa->id == id_mappa && sessioni[i]->pa->port2 < 0){
			current->nplayer = 2;
			current->pa = sessioni[i]->pa;
			current->pa->port2 = current->msg_port;
			strcpy(risp, "G2");
			check(send(current->socket, risp, 2, 0));
			printf("[INFO] il socket %d partecipa alla partita del socket %d\n", current->socket, sessioni[i]->socket);
			return;
		}
	}
	
	//conversione da id a nome
	for(i = 0; i <= id_mappa; i++)
		fscanf(indice_mappe, "%s", nome);

	//inizializzazione
	part = (struct partita*) malloc(sizeof(struct partita));
	genera_partita(nome, part);
	current->pa = part;
	current->pa->port1 = current->msg_port;
	current->pa->port2 = -1;
	current->nplayer = 1;
	current->pa->id = id_mappa;

	//notifica client
	strcpy(risp, "GO");
	send(current->socket, risp, 2, 0);
	printf("[INFO] il socket %d ha avviato una partita su %s\n", current->socket, mappa);
}

// ---------- LOOK ----------
// manda al client la descrizione dell'elemento corrispondente al parametro specificato
void look(int narg, char* what, struct sessione* current){
	int i;
	char buffer[MAX_DESCRIPTION_LENGTH + 2];

	printf("[INFO] il socket %d ha effettuato il comando look %s\n", current->socket, what);
	memset(buffer, 0, sizeof(buffer));

	if(tempo_in_partita(&(current->pa->start), current->pa->tempo_massimo) <= 0){
		//tempo scaduto
		chiudi_partita(current);
		strcpy(buffer, "TS");
		send(current->socket, buffer, sizeof(buffer), 0);
		return;
	}

	//cerco tra gli oggetti
	i = cerca_object(what, current->pa);
	if(i >= 0){
		strcpy(buffer, "OK");
		strcat(buffer, current->pa->objects[i]->descrizione);
		send(current->socket, buffer, sizeof(buffer), 0);
		return;
	}

	//cerco tra le location
	i = cerca_location(what, current->pa);
	if(i >= 0){
		strcpy(buffer, "OK");
		strcat(buffer, current->pa->locations[i]->descrizione);
		send(current->socket, buffer, sizeof(buffer), 0);
		return;
	}
	
	//non trovato
	strcpy(buffer, "NF");
	send(current->socket, buffer, sizeof(buffer), 0);
}


// ---------- TAKE ----------
void take(int narg, char* what, struct sessione* current){
	char buffer[2];
	int i, inv_index = -1, obj_index, eni_index;
	
	if(narg < 1 || current->pa == NULL){
		//comando invalido
		strcpy(buffer, "ER");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	
	printf("[INFO] il socket %d ha effettuato il comando take %s\n", current->socket, what);

	//controllo tempo rimasto
	if(tempo_in_partita(&(current->pa->start), current->pa->tempo_massimo) <= 0){
		//tempo scaduto
		chiudi_partita(current);
		strcpy(buffer, "TS");
		send(current->socket, buffer, sizeof(buffer), 0);
		return;
	}
	
	//controlla spazio nell'inventario
	for(i = 0; i < MAX_INVENTORY_SIZE; i++){
		if(current->pa->inventario[i] == -1){
			inv_index = i;
			break;
		}
	}
	if(inv_index == -1){
		//inventario pieno
		strcpy(buffer, "NS");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	//cerca l'indice dell'oggetto
	obj_index = cerca_object(what, current->pa);
	if(obj_index < 0){
		//oggetto inesistente
		strcpy(buffer, "NF");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	if(cerca_in_inventario(obj_index, current->pa) >= 0){
		//oggetto già preso
		strcpy(buffer, "AT");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	if(current->pa->objects[obj_index]->pickup == 0){
		//l'oggetto non può essere preso
		strcpy(buffer, "NC");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	eni_index = current->pa->objects[obj_index]->enigma_bloccante;
	if(eni_index >= 0 && current->pa->enigmi[eni_index]->tipo == 0){
		//oggetto bloccato da enigma indovinello
		strcpy(buffer, "LK");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	
	//inserisci l'oggetto nell'inventario
	current->pa->inventario[inv_index] = obj_index;
	strcpy(buffer, "OK");
	check(send(current->socket, buffer, sizeof(buffer), 0));
}

// ---------- USE ----------
// gestione dell'azione use su uno o due oggetti
void use(int narg, char* what1, char* what2, struct sessione* current){
	char buffer[2 + MAX_DESCRIPTION_LENGTH];

	if(current->pa == NULL || narg == 0){
		strcpy(buffer, "ER");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	if(narg == 1)
		printf("[INFO] il socket %d ha effettuato il comando use %s\n", current->socket, what1);
	else
		printf("[INFO] il socket %d ha effettuato il comando use %s %s\n", current->socket, what1, what2);

	memset(buffer, 0, sizeof(buffer));

	//se il tempo è scaduto
	if(tempo_in_partita(&(current->pa->start), current->pa->tempo_massimo) <= 0){
		chiudi_partita(current);
		strcpy(buffer, "TS");
		send(current->socket, buffer, sizeof(buffer), 0);
		return;
	}

	//se si fa use su un solo oggetto o su 2
	if(narg == 1){
		int obj_index, eni_index, target_obj;

		obj_index = cerca_object(what1, current->pa);
		if(obj_index < 0){
			//oggetto non esistente
			strcpy(buffer, "NF");
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}
		if(cerca_in_inventario(obj_index, current->pa) < 0){
			//what1 non nell'inventario
			strcpy(buffer, "NT");
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}

		eni_index = current->pa->objects[obj_index]->enigma_bloccante;
		if(eni_index < 0){
			//oggetto non bloccato da enigma
			strcpy(buffer, "NL");
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}
		
		//inserisco la nuova descrizione dell'oggetto target
		target_obj = current->pa->enigmi[eni_index]->target;
		strcpy(current->pa->objects[target_obj]->descrizione, current->pa->enigmi[eni_index]->descrizione);
		current->pa->objects[obj_index]->enigma_bloccante = -1;

		current->pa->tokens_taken += current->pa->enigmi[eni_index]->token;
		
		//se ha vinto
		if(current->pa->tokens_taken >= current->pa->max_tokens){
			strcpy(buffer, "WI");
			strcat(buffer, current->pa->enigmi[eni_index]->risolto);
			check(send(current->socket, buffer, sizeof(buffer), 0));
			chiudi_partita(current);
			return;
		}

		strcpy(buffer, "OK");
		strcat(buffer, current->pa->enigmi[eni_index]->risolto);
		check(send(current->socket, buffer, sizeof(buffer), 0));
	}
	else{
		int obj1_index, obj2_index, eni_index, target_obj;

		obj1_index = cerca_object(what1, current->pa);
		obj2_index = cerca_object(what2, current->pa);

		if(obj1_index < 0 || obj2_index < 0){
			//oggetto non esistente
			strcpy(buffer, "NF");
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}
		if(cerca_in_inventario(obj1_index, current->pa) < 0){
			//what1 non nell'inventario
			strcpy(buffer, "NT");
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}

		eni_index = current->pa->objects[obj2_index]->enigma_bloccante;
		if(eni_index < 0){
			//oggetto non bloccato da enigma (il secondo oggetto sarà bloccato nel caso)
			strcpy(buffer, "NL");
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}

		if(current->pa->enigmi[eni_index]->obj != obj1_index){
			//what1 errato e non risolve l'enigma di what2
			strcpy(buffer, "NU");
			strcat(buffer, current->pa->enigmi[eni_index]->errore);
			check(send(current->socket, buffer, sizeof(buffer), 0));
			return;
		}

		current->pa->tokens_taken += current->pa->enigmi[eni_index]->token;

		//inserisco la nuova descrizione dell'oggetto target
		target_obj = current->pa->enigmi[eni_index]->target;
		strcpy(current->pa->objects[target_obj]->descrizione, current->pa->enigmi[eni_index]->descrizione);
		current->pa->objects[obj2_index]->enigma_bloccante = -1;

		//se ha vinto
		if(current->pa->tokens_taken >= current->pa->max_tokens){
			strcpy(buffer, "WI");
			strcat(buffer, current->pa->enigmi[eni_index]->risolto);
			check(send(current->socket, buffer, sizeof(buffer), 0));
			chiudi_partita(current);
			return;
		}

		//rispondo al client
		strcpy(buffer, "OK");
		strcat(buffer, current->pa->enigmi[eni_index]->risolto);
		check(send(current->socket, buffer, sizeof(buffer), 0));	
	}
}

// ---------- OBJS ----------
// manda al client la lista degli oggetti nell'inventario
void objs(struct sessione* current){
	char buffer[2 + MAX_DESCRIPTION_LENGTH];
	int i, count = 0;

	printf("[INFO] il socket %d ha effettuato il comando objs\n", current->socket);
	memset(buffer, 0, sizeof(buffer));

	if(current->pa == NULL){
		strcpy(buffer, "ER");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	//se il tempo è scaduto
	if(tempo_in_partita(&(current->pa->start), current->pa->tempo_massimo) <= 0){
		chiudi_partita(current);
		strcpy(buffer, "TS");
		send(current->socket, buffer, sizeof(buffer), 0);
		return;
	}

	strcpy(buffer, "OK");
	for(i = 0; i < MAX_INVENTORY_SIZE; i++){
		if(current->pa->inventario[i] >= 0){
			strcat(buffer, current->pa->objects[current->pa->inventario[i]]->nome);
			strcat(buffer, "\n");
			count++;
		}
	}
	if(count == 0)
		strcpy(buffer, "NI");
	
	check(send(current->socket, buffer, sizeof(buffer), 0));
}

// ---------- END ----------
// termina la partita della sessione specificata
void chiudi_partita(struct sessione* current){
	int i;

	if(current->pa == NULL)
		return;

	//se in partita c'erano 2 giocatori allora va liberata anche la partita della sessione dell'altro giocatore
	if(current->pa->port2 >= 0){
		for(i = 0; i < MAX_CONNESSIONI; i++){
			if(sessioni[i] != NULL && sessioni[i]->pa == current->pa && sessioni[i] != current){
				sessioni[i]->pa = NULL;
				break;
			}
		}
	}

	//liberamento memoria locations
	for(i = 0; i < MAX_LOCATIONS; i++){
		if(current->pa->locations[i] != NULL)
			free(current->pa->locations[i]);
	}

	//liberamento memoria objects
	for(i = 0; i < MAX_OBJECTS; i++){
		if(current->pa->objects[i] != NULL)
			free(current->pa->objects[i]);
	}

	//liberamento memoria enigmi
	for(i = 0; i < MAX_ENIGMI; i++){
		if(current->pa->enigmi[i] != NULL)
			free(current->pa->enigmi[i]);
	}

	free(current->pa);
	current->pa = NULL;
	
}

// ---------- ENIGMA ----------
// manda i dettagli di un enigma di tipo indovinello
void enigma(int narg, char* what, struct sessione* current){
	char buffer[2 + MAX_DESCRIPTION_LENGTH];
	int obj_index, eni_index;
	
	memset(buffer, 0, sizeof(buffer));

	if(narg < 1 || current->pa == NULL){
		//errore
		strcpy(buffer, "ER");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	
	printf("[INFO] il socket %d ha effettuato il comando enigma %s\n", current->socket, what);

	//ricerca dell'enigma collegato all'oggetto
	obj_index = cerca_object(what, current->pa);
	if(obj_index < 0){
		//oggetto inesistente
		strcpy(buffer, "NF");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	eni_index = current->pa->objects[obj_index]->enigma_bloccante;
	if(eni_index < 0){
		//oggetto non bloccato da enigma
		strcpy(buffer, "NL");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	if(current->pa->enigmi[eni_index]->tipo == 1){
		//richiede l'utilizzo di un altro oggetto
		strcpy(buffer, "RO");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	//restituzione enigma
	strcpy(buffer, "OK");
	strcat(buffer, current->pa->enigmi[eni_index]->descrizione);
	check(send(current->socket, buffer, sizeof(buffer), 0));
}

// ---------- UNLOCK ----------
// convalida la soluzione all'enigma di tipo indovinello mandata dal client
void risolvi(int narg, char* what, char* solution, struct sessione* current){
	char buffer[2 + MAX_DESCRIPTION_LENGTH];
	int obj_index, eni_index;
	
	memset(buffer, 0, sizeof(buffer));

	//controllo formato richiesta
	if(narg < 2 || current->pa == NULL){
		//errore
		strcpy(buffer, "ER");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	//ricerca dell'enigma collegato all'oggetto
	obj_index = cerca_object(what, current->pa);
	if(obj_index < 0){
		//oggetto inesistente
		strcpy(buffer, "NF");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}

	eni_index = current->pa->objects[obj_index]->enigma_bloccante;
	if(eni_index < 0){
		//oggetto non bloccato da enigma
		strcpy(buffer, "NL");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	if(current->pa->enigmi[eni_index]->tipo == 1){
		//richiede l'utilizzo di un altro oggetto, enigma non risolvibile con unlock
		strcpy(buffer, "RO");
		check(send(current->socket, buffer, sizeof(buffer), 0));
		return;
	}
	
	if(strcmp(solution, current->pa->enigmi[eni_index]->risposta) == 0){
		//risposta giusta
		current->pa->tokens_taken += current->pa->enigmi[eni_index]->token;
		
		//se ha vinto
		if(current->pa->tokens_taken >= current->pa->max_tokens){
			strcpy(buffer, "WI");
			strcat(buffer, current->pa->enigmi[eni_index]->risolto);
			check(send(current->socket, buffer, sizeof(buffer), 0));
			chiudi_partita(current);
			return;
		}

		strcpy(buffer, "OK");
		strcat(buffer, current->pa->enigmi[eni_index]->risolto);
		check(send(current->socket, buffer, sizeof(buffer), 0));

		current->pa->objects[obj_index]->enigma_bloccante = -1;
		return;
	}
	//risposta errata
	strcpy(buffer, "NU");
	strcat(buffer, current->pa->enigmi[eni_index]->errore);
	check(send(current->socket, buffer, sizeof(buffer), 0));	
}

// ---------- TIME ----------
// manda il tempo rimasto al client
void send_time(struct sessione* current){
	uint16_t risp;
	
	//in questo caso la risposta sarà semplicemente un intero
	//controlli
	if(current->pa == NULL)
		risp = 0;
	else
		risp = tempo_in_partita(&(current->pa->start), current->pa->tempo_massimo);

	check(send(current->socket, &risp, sizeof(risp), 0));
}

// ---------- TOKENS ----------
// manda il numero di tokens raccolti e quelli totali
void send_tokens(struct sessione* current){
	uint16_t risp1, risp2;

	if(current->pa == NULL){
		risp1 = 0;
		risp2 = 0;
	}
	else{
		risp1 = current->pa->tokens_taken;
		risp2 = current->pa->max_tokens;
	}

	check(send(current->socket, &risp1, sizeof(risp1), 0));
	check(send(current->socket, &risp2, sizeof(risp2), 0));
}

// ---------- DEPOSIT ----------
// deposita l'oggetto specificato dal client
void deposit(int narg, char* arg2, struct sessione* current){
	char risp[2];
	int obj_index, inv_index;

	//controllo formato richiesta
	if(narg <= 0 || current->pa == NULL){
		strcpy(risp, "ER");
		check(send(current->socket, risp, sizeof(risp), 0));
		return;
	}

	//se il tempo è scaduto
	if(tempo_in_partita(&(current->pa->start), current->pa->tempo_massimo) <= 0){
		chiudi_partita(current);
		strcpy(risp, "TS");
		send(current->socket, risp, sizeof(risp), 0);
		return;
	}

	obj_index = cerca_object(arg2, current->pa);
	if(obj_index < 0){
		//oggetto non esistente
		strcpy(risp, "NF");
		check(send(current->socket, risp, sizeof(risp), 0));
		return;
	}

	//ricerca dell'oggetto nell'inventario
	inv_index = cerca_in_inventario(obj_index, current->pa);
	if(inv_index < 0){
		//oggetto non presente nell'inventario
		strcpy(risp, "NT");
		check(send(current->socket, risp, sizeof(risp), 0));
		return;
	}

	//oggetto depositato
	current->pa->inventario[inv_index] = -1;
	strcpy(risp, "OK");
	check(send(current->socket, risp, sizeof(risp), 0));
	return;
}

// -----------------------------------------------------------------
// |          PARTE DEDICATA ALLA FUNZIONALITA' A PIACERE          |
// -----------------------------------------------------------------

//setta la porta su cui basare la messaggistica tra giocatori
void setport(int narg, char* arg2, struct sessione* current){	
	//controllo formato
	if(narg != 1)
		return;

	printf("[INFO] il socket %d ha comunicato la sua porta per i messaggi\n", current->socket);
	current->msg_port = atoi(arg2);
}

//manda la porta su cui contattare l'altro giocatore
void send_port(struct sessione* current){
	char risp[2 + MAX_NAME_LENGTH];

	printf("[INFO] il socket %d ha richiesto la porta dell'altro client in partita\n", current->socket);
	memset(risp, 0, sizeof(risp));

	//se nessuna partita è in corso o c'è un solo giocatore in partita
	if(current->pa == NULL || current->pa->port2 < 0){
		strcpy(risp, "ER");
		check(send(current->socket, risp, sizeof(risp), 0));
		return;
	}

	strcpy(risp, "OK");
	if(current->nplayer == 1)
		sprintf(&risp[2], "%d", current->pa->port2);
	else
		sprintf(&risp[2], "%d", current->pa->port1);

	check(send(current->socket, risp, sizeof(risp), 0));
}

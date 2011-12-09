#Makefile del progetto di Sistemi Operativi
VERSION = 1.0

#Path del compliatore 
CC = gcc
BIN = ./bin
OBJ = ./obj
SRC = ./src
LOG = ./log

#Compilazione
all: server client
server: $(BIN)/server
client: $(BIN)/download $(BIN)/upload 

#Installazione e rimozione directory
install:
		mkdir $(BIN) $(OBJ) $(LOG) $(LOG)/server $(LOG)/client
uninstall: 
		rm -rf $(BIN) $(OBJ) $(LOG)
reinstall: uninstall install all

#Tools di cancellazione file binary e di log
clean:	cleanbin
		rm -f $(LOG)/server/* $(LOG)/client/*
cleanbin: 
		rm -f $(OBJ)/* $(BIN)/*
 
#Tools di compilazione delle librerie
$(OBJ)/library.o: $(SRC)/library.c $(SRC)/library.h makefile
		$(CC) -c $(SRC)/library.c -o $(OBJ)/library.o
$(OBJ)/client.o: $(SRC)/client.c $(SRC)/client.h makefile
		$(CC) -c $(SRC)/client.c -o $(OBJ)/client.o

#Compilazione file binari
$(BIN)/download: $(SRC)/client_download.c $(OBJ)/library.o $(OBJ)/client.o
		$(CC) -o $(BIN)/download $(SRC)/client_download.c $(OBJ)/library.o $(OBJ)/client.o
$(BIN)/upload: $(SRC)/client_upload.c $(OBJ)/library.o $(OBJ)/client.o
		$(CC) -o $(BIN)/upload $(SRC)/client_upload.c $(OBJ)/library.o $(OBJ)/client.o
$(BIN)/server: $(SRC)/server.c $(SRC)/server.h $(OBJ)/library.o
		$(CC) -o $(BIN)/server $(SRC)/server.c $(OBJ)/library.o

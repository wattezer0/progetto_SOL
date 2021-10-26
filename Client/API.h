#define _DEFAULT_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>

#define UNIX_PATH_MAX 108
#define READ_AND_EXIT(a,b,c)    if(!read(a,b,c) ) { return -1;  }

int fd_skt;
int enable_print;
int canWriteFile = 0;
const char ops[10] = {'o', 'r', 'R', 'w', 'a', 'l','u', 'c', 'x','z'};
char  * errorss[100] = 
    {
    "Tutto ok", //0
    "Errore", //1
    "File non esistente", //2
    "Il file è in modalità locked", //3
    "Il file è acquisito da un altro utente",   //4
    "Il file non è in modalità locked",   //5
    "Il file è troppo grande",   //6
    "Il contenuto da aggiungere è troppo grande",   //7
    "Errore",   //8
    "Errore",   //9
    "Errore",   //10
    "Errore",   //11
    "Errore",   //12
    "Errore",   //13
    "Errore",   //14
    "Errore",   //15
    "Errore",   //16
    "File già esistente",   //17
    "Errore",   //18
    };



//STAMPA DELLE OPERAZIONI RELATIVE ALL'OPERAZIONE
void print_info (int flag, char * op_type, const char * file, int esito, size_t byte_letti, size_t byte_scritti) {
  if (flag) {
    if (op_type != NULL) printf("\nTipo di operazione : %s\n", op_type);
    if (file != NULL) printf("File di riferimento : %s\n", file);
    if (byte_letti != 0) printf("Byte letti : %zu\n", byte_letti);
    if (byte_scritti != 0) printf("Byte scritti : %zu\n", byte_scritti);
    if (esito == 0) printf("Esito operazione : SUCCESS\n");
    if (esito > 0) printf("Esito operazione : SUCCESS\nNumero di file letti : %d\n", esito);
    if (esito == -1) printf("Esito operazione : FAILED\n");
    if (esito != 0 && errno != 0) printf("ERRNO = %d\nCausa di fallimento : %s\n", errno, errorss[errno]);
    printf("\n");
  }
}

//RIMUOVE TUTTE LE OCCORRENZE DI UN CHAR DA UNA STRINGA
void removeChar(char * str, char charToRemmove){
    int i;
    int len = strlen(str);
    for(i=0; i<len; i++)
    {
        if(str[i] == charToRemmove)
        {
            str[i] = '_';
        }
    }
    
}


/*Viene aperta una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la
richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo
scadere del tempo assoluto ‘abstime’ specificato come terzo argomento. Ritorna 0 in caso di successo, -1 in caso
di fallimento, errno viene settato opportunamente.*/
int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    
    if ((fd_skt=socket(AF_UNIX,SOCK_STREAM,0)) == -1) {
        print_info(enable_print, "openConnection", NULL, -1, 0, 0);
        return -1;
    }
    //memset(&client_addr, '0', sizeof(client_addr));
    struct sockaddr_un client_addr;
    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, sockname, UNIX_PATH_MAX);
    int count = 0;
    while (connect(fd_skt,(struct sockaddr*)&client_addr, sizeof(client_addr)) == -1) {
        usleep(1000  * msec); /* sock non esiste */
        count += msec;
        printf("NON RIESCO A CONNETTERMI\n");
        if (count > 1000 * abstime.tv_sec){
            print_info(enable_print, "openConnection", NULL, -1, 0, 0);
            return -1;
    }}
    print_info(enable_print, "openConnection", NULL, 0, 0, 0);
    return 0;
}

/*Chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/
int closeConnection(const char* sockname) {
    write(fd_skt, &ops[9], sizeof(char));
    //CHIUSURA DEL SOCKET
    int check = close(fd_skt);
    print_info(enable_print, "closeConnection", NULL, check, 0, 0);
    return 0;
}

/*Richiesta di apertura o di creazione di un file. La semantica della openFile dipende dai flags passati come secondo
argomento che possono essere O_CREATE ed O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già
memorizzato nel server, oppure il file non esiste ed il flag O_CREATE non è stato specificato, viene ritornato un
errore. In caso di successo, il file viene sempre aperto in lettura e scrittura, ed in particolare le scritture possono
avvenire solo in append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE) il file viene
aperto e/o creato in modalità locked, che vuol dire che l’unico che può leggere o scrivere il file ‘pathname’ è il
processo che lo ha aperto. Il flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata unlockFile,
descritta di seguito.
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int openFile(const char* pathname, int flags) {
    if (fd_skt == -1) {
        print_info(enable_print, "openFile", pathname, -1, 0, 0);
        return -1;
    }
    errno = 0;
    write(fd_skt, &ops[0], sizeof(char));//codice operazione corrispondente a openFile

    int pathname_lenght = strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght);

    write(fd_skt, &flags, sizeof(int));

    int answer; 
    READ_AND_EXIT(fd_skt, &answer, sizeof(int));

    if (answer) {
        errno = answer;
    }
    
    if (answer && flags == 3) { //canwritefile è una variabile booleana che vincola il writeFile come operazione successiva solo a openfile (nel caso in cui OCREATE E OLOCK siano = 1)
        canWriteFile = 1;
    }
    else canWriteFile = 0;
    
    answer = (answer == 0? 0 : -1);
    print_info(enable_print, "openFile", pathname, answer, 0, 0);
    return answer;
}

/*Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore ad un'area allocata sullo heap nel
parametro ‘buf’, mentre ‘size’ conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In
caso di errore, ‘buf‘ e 'size' non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente*/
int readFile(const char * pathname, void ** buf, size_t * size) {
    
    canWriteFile = 0;

    if (fd_skt == -1) {
        print_info(enable_print, "readFile", pathname, -1, 0, 0);
        return -1;
    }
    
    write(fd_skt, &ops[1], sizeof(char));//codice operazione corrispondente a readFile  
    int pathname_lenght =  strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int)); //lunghezza pathname
    write (fd_skt, pathname, pathname_lenght);//pathname
    
    int check;  
    READ_AND_EXIT(fd_skt, &check, sizeof(int)); //risposta dal server 
    if(check == 0) {
        print_info(enable_print, "readFile", pathname, -1, 0, 0);
        printf("Il file %s non è presente nel server, operazione annullata\n", pathname);
        return -1;
    }
    if(check == -1) {
        printf("Il file %s è stato acquisito da un altro utente\n", pathname);
        print_info(enable_print, "readFile", pathname, -1, 0, 0);
        return -1;
    }
    int lenght;
    READ_AND_EXIT(fd_skt, &lenght, sizeof(int));
    char * content  = (char *) malloc (2 * lenght * sizeof(char));
    READ_AND_EXIT(fd_skt, content, lenght);
    *buf = content;
    *size = (size_t) lenght;    
    print_info(enable_print, "readFile", pathname, 0, *size, 0);
    return 0;

}

/*Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client. Se il server
ha meno di ‘N’ file disponibili, li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file
memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di successo (cioè ritorna il n. di file
effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.*/
int readNFiles(int N, const char* dirname) {

    canWriteFile = 0;

    if (fd_skt == -1) {
        print_info(enable_print, "readNFiles", NULL, -1, 0, 0);
        return -1;
    }
    write(fd_skt, "R", sizeof(char));//codice operazione corrispondente a readNFiles
    
    write(fd_skt, &N, sizeof(int));
    int fs_lenght; //numero di file nel file system
    READ_AND_EXIT(fd_skt, &fs_lenght, sizeof(int));
    if (fs_lenght == 0) {
        printf("Non ci sono file da leggere!\n");
        print_info(enable_print, "readNFiles", NULL, 0, 0, 0);
        return 0;
    }
    int i = 0;
    size_t byte_letti = 0;
    //LETTURA DEI FILE 
    for (i = 0; i < fs_lenght; i ++) {
        int file_name_lenght, file_cont_lenght;
        READ_AND_EXIT(fd_skt, &file_name_lenght, sizeof(int));
        char * file_name = (char *) malloc(2 * file_name_lenght * sizeof(char));
        READ_AND_EXIT(fd_skt, file_name, file_name_lenght);
        READ_AND_EXIT(fd_skt, &file_cont_lenght, sizeof(int));
        char * file_cont = (char *) malloc(2 * file_cont_lenght * sizeof(char));
        READ_AND_EXIT(fd_skt, file_cont, file_cont_lenght );
        byte_letti += file_cont_lenght;
        //salva il contenuto nella cartella dirname passata come parametro
        if (strlen(dirname)){ 
            FILE * tmpp;  
            char new[256];
            strcpy(new, dirname);
            strcat(new, "/");
            removeChar(file_name, '/');
            strcat(new, file_name); 
            if((tmpp = fopen(new, "w")) == NULL) {
                printf("Errore nell' apertura del file %s\n", new);
                free(file_name);
                free(file_cont);
                print_info(enable_print, "readNFiles", NULL, -1, 0, 0);
                return -1;
            }                    
            fprintf(tmpp, "%s", file_cont);
            fclose(tmpp);
        }
        else {
        printf("Contenuto del file %s :\n %s \n", file_name, file_cont);
        }
        free(file_name);
        free(file_cont);
    }
    
    print_info(enable_print, "readNFiles", NULL, i, byte_letti, 0);
    return i;  //numero di file letti

}


/*Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
terminata con successo, è stata openFile(pathname, O_CREATE| O_LOCK). Se ‘dirname’ è diverso da NULL, il
file eventualmente spedito dal server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere
scritto in ‘dirname’; Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int writeFile(const char* pathname, const char* dirname) {
    if (fd_skt == -1 && canWriteFile) {
        print_info(enable_print, "writeFile", pathname, -1, 0, 0);
        return -1;
    }
    write(fd_skt, &ops[3], sizeof(char)); //codice operazione corrispondente a writeFiles

    int pathname_lenght = strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght);
    //INVIARE IL CONTENUTO DEL FILE PATHNAME A SERVER!!!!
    FILE * tmp;
    if((tmp = fopen(pathname, "r")) == NULL) {
        printf("Errore nell' apertura del file %s\n", pathname);
        print_info(enable_print, "writeFile", pathname, -1, 0, 0);
        return -1;
    }

    
    fseek(tmp, 0, SEEK_END);
    int file_lenght = ftell(tmp);
    rewind(tmp);
    char * buffer = (char *) malloc(2*file_lenght*sizeof(char));
    int read_size = fread(buffer, sizeof(char), file_lenght, tmp);
    buffer[file_lenght] = '\0';
    if (file_lenght != read_size) {
        free(buffer);
        buffer = NULL;
        printf("ERRRORE NELLA LETTURA DEL FILE %s\n", pathname);
        print_info(enable_print, "writeFile", pathname, -1, 0, 0);
        return -1;
    }
    fclose(tmp);
    write(fd_skt, &file_lenght, sizeof(int));
    write(fd_skt, buffer, file_lenght + 1);
    free(buffer);

    int check;
    READ_AND_EXIT(fd_skt, &check, sizeof(int));

    char dir[256];
    if (strlen(dirname) != 0)
        strcpy(dir, dirname);

    if (check == 6) { //ERRORE
        printf(" Operazione fallita! %d = %s\n", check, errorss[check]);
        print_info(enable_print, "writeFile", pathname, -1, 0, 0);
        return -1;
    }

    size_t byte_letti = 0;
    //SALVATAGGIO DEI FILE ESPULSI DAL FILE SYSTEM
    while (check == 1) {
        char dir[256];
        int name_lenght, file_lenght;
        char name[256];

        READ_AND_EXIT(fd_skt, &name_lenght, sizeof(int));
    

        READ_AND_EXIT(fd_skt, name, name_lenght);
        READ_AND_EXIT(fd_skt, &file_lenght, sizeof(int));
        byte_letti += file_lenght;
        char * tmp_cont = (char *) malloc(2 * file_lenght * sizeof(char));
        //SALVA I FILE ESPULSI DENTRO DIRNAME
        if (strlen(dirname) > 0) {
            strcpy(dir, dirname);
            strcat(dir,"/");
            removeChar(name, '/');
            strcat(dir,name);
            FILE * new;
            if ( (new = fopen(dir, "w")) == NULL) {
                free(tmp_cont);
                printf("Errore nella creazione del file %s\n", pathname);
                print_info(enable_print, "writeFile", pathname, -1, 0, 0);
                return -1;
            }
            READ_AND_EXIT(fd_skt, tmp_cont, file_lenght);
            fprintf(new, "%s", tmp_cont);
            fclose(new);
        }
        else {
            READ_AND_EXIT(fd_skt, tmp_cont, file_lenght);
            printf("Il file %s è stato espulso dal file system : %s\n", name, tmp_cont);
        }
        free(tmp_cont);
        READ_AND_EXIT(fd_skt, &check, sizeof(int));

    }
    
    print_info(enable_print, "writeFile", pathname, check, byte_letti, file_lenght);
    return check;
}

// /*Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
// nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
// dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
// Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    if (fd_skt == -1) {
        print_info(enable_print, "appendToFile", pathname, -1, 0, 0);
        return -1;
    }
    write(fd_skt, &ops[4], sizeof(char)); //codice operazione corrispondente a appendToFile
    int pathname_lenght = strlen(pathname) + 1;
    int cont_lenght = size + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght * sizeof(char));
    write(fd_skt, &cont_lenght, sizeof(int));
    write(fd_skt, buf, cont_lenght * sizeof(char));    

    int check;
    READ_AND_EXIT(fd_skt, &check, sizeof(int));

    //CONTENUTO TROPPO GRANDE
    if (check == 7) {
        print_info(enable_print, "appendToFile", pathname, -1, 0, 0);
        return -1;
    }

    size_t byte_letti = 0;
    //SALVATAGGIO DEI FILE ESPULSI DAL FILE SYSTEM
    while (check) {
        char dir[256];
        int name_lenght, file_lenght;
        char name[256];

        READ_AND_EXIT(fd_skt, &name_lenght, sizeof(int));

        READ_AND_EXIT(fd_skt, name, name_lenght);
        READ_AND_EXIT(fd_skt, &file_lenght, sizeof(int));

        char * tmp_cont = (char *) malloc(2 * file_lenght * sizeof(char));
        if (strlen(dirname) != 0) {
            strcpy(dir, dirname);
            strcat(dir,"/");
            strcat(dir,name);
            FILE * new;
            if ( (new = fopen(dir, "w")) == NULL) {
                printf("Errore nella creazione del file %s\n", pathname);
                print_info(enable_print, "appendToFile", pathname, -1, 0, cont_lenght);
                return -1;
            }
            READ_AND_EXIT(fd_skt, tmp_cont, file_lenght);
            fprintf(new, "%s", tmp_cont);
            fclose(new);
        }
        else {
            READ_AND_EXIT(fd_skt, tmp_cont, file_lenght);
            printf("Il file %s è stato espulso dal file system : %s\n", name, tmp_cont);
        }
        byte_letti += file_lenght;
        free(tmp_cont);
        READ_AND_EXIT(fd_skt, &check, sizeof(int));

    }
    print_info(enable_print, "appendToFile", pathname, 0, byte_letti, cont_lenght);
    return 0;
}

// /*In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
// richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
// immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
// viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. Ritorna 0 in
// caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int lockFile(const char* pathname) {
    if (fd_skt == -1) {
        print_info(enable_print, "lockFile", pathname, -1, 0, 0);
        return -1;
    }
    write(fd_skt, &ops[5], sizeof(char)); //codice operazione corrispondente a lockFile
    int pathname_lenght = strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght);
    int check;
    READ_AND_EXIT(fd_skt, &check, sizeof(int));    
    while (check == 3) {   // --> O_LOCK è già settata da un altro client --> attesa
        printf("STO ASPETTANDO\n");
        READ_AND_EXIT(fd_skt, &check, sizeof(int));
    } 
    
    if (check == 0) {    //check = 0 --> O_LOCK settata con successo oppure O_LOCK era già stata settata dallo stesso client
        print_info(enable_print, "lockFile", pathname, 0, 0, 0);
        return 0;
    }
    
    else {   // --> qualcosa è andato storto!
        print_info(enable_print, "lockFile", pathname, -1, 0, 0);
        return -1;
    }

    return -1;   
}

/*Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
ha richiesto l’operazione, altrimenti l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/
int unlockFile(const char* pathname) {
    if (fd_skt == -1) {
        print_info(enable_print, "unlockFile", pathname, -1, 0, 0);
        return -1;
    }

    write(fd_skt, &ops[6], sizeof(char)); //codice operazione corrispondente a unlockFile
    int pathname_lenght = strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght);
    int check;
    READ_AND_EXIT(fd_skt, &check, sizeof(int));        
    
    if (check == 0) {    //check = 0 --> O_LOCK resettata con successo
        print_info(enable_print, "unlockFile", pathname, 0, 0, 0);
        return 0;
    }

    else {   // --> qualcosa è andato storto!
        print_info(enable_print, "unlockFile", pathname, -1, 0, 0);
        return -1;
    }
}

// /*Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
// Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int closeFile(const char* pathname) {
    if (fd_skt == -1) {
        print_info(enable_print, "closeFile", pathname, -1, 0, 0);
        return -1;
    }
    
    write(fd_skt, &ops[7], sizeof(char)); //codice operazione corrispondente a closeFile
    int pathname_lenght = strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght);
    int check;
    READ_AND_EXIT(fd_skt, &check, sizeof(int));
    if (check == 0) {
        print_info(enable_print, "closeFile", pathname, 0, 0, 0);
    }
    if (check == 1) {
        print_info(enable_print, "closeFile", pathname, -1, 0, 0);
        printf("Errore nella chiusura del file %s\n", pathname);
    }
    if (check == -2) {
        print_info(enable_print, "closeFile", pathname, -1, 0, 0);
        printf("Il file %s non è stato trovato\n", pathname);
    }
    check = check == 0 ? 0 : -1;
    return check;
}

/*Rimuove il file cancellandolo dal file storage server. L’operazione fallisce se il file non è in stato locked, o è in
stato locked da parte di un processo client diverso da chi effettua la removeFile.*/
int removeFile(const char* pathname) {
    if (fd_skt == -1) {
        print_info(enable_print, "removeFile", pathname, -1, 0, 0);
        return -1;
    }
    write(fd_skt, &ops[8], sizeof(char)); //codice operazione corrispondente a removeFile
    int pathname_lenght = strlen(pathname) + 1;
    write(fd_skt, &pathname_lenght, sizeof(int));
    write(fd_skt, pathname, pathname_lenght);
    int check;
    READ_AND_EXIT(fd_skt, &check, sizeof(int));
    if (check == 0) {
        print_info(enable_print, "removeFile", pathname, 0, 0, 0);
        return 0;    
    }
    else {
        print_info(enable_print, "removeFile", pathname, -1, 0, 0);
        return -1;
    }
}
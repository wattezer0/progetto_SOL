/* We want POSIX.1-2008 + XSI, i.e. SuSv4, features */
#define _XOPEN_SOURCE 700

/* Added on 2017-06-25:
   If the C library can support 64-bit file sizes
   and offsets, using the standard names,
   these defines tell the C library to do so. */
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64 

#include "API.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <errno.h>

/* POSIX.1 says each process has at least 20 file descriptors.
 * Three of those belong to the standard streams.
 * Here, we use a conservative estimate of 15 available;
 * assuming we use at most two for other uses in this program,
 * we should never run into any problems.
 * Most trees are shallower than that, so it is efficient.
 * Deeper trees are traversed fine, just a bit slower.
 * (Linux allows typically hundreds to thousands of open files,
 *  so you'll probably never see any issues even if you used
 *  a much higher value, say a couple of hundred, but
 *  15 is a safe, reasonable value.)
*/
#ifndef USE_FDS
#define USE_FDS 15
#endif

int file_limit;
char * removed_file_directory;

//CONTROLLA CHE PATH SIA UN FILE
int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

//CONTROLLA CHE PATH SIA UNA CARTELLA
int is_directory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

//FUNZIONE CHIAMATA RICORSIVAMENTE DA visit_directory_tree CHE SCRIVE IL FILE SUL FILE SYSTEM
int openEntry(const char *filepath, const struct stat *info, const int typeflag, struct FTW *pathinfo) {
    //NON STO GUARDANDO UN FILE
    if (typeflag != FTW_F) { 
        return 0;
    }

    if (file_limit > 0) {
        if (openFile(filepath, 0) == 0) {   //il file esiste già
            FILE * tmp;
            if((tmp = fopen(filepath, "r")) == NULL) {
                printf("Errore nell' apertura del file %s\n", filepath);
                return -1;
            }        
            fseek(tmp, 0, SEEK_END);
            int file_lenght = ftell(tmp);
            rewind(tmp);
            char * buffer = (char *) malloc(2*file_lenght * sizeof(char));
            int read_size = fread(buffer, sizeof(char), file_lenght, tmp);
            buffer[file_lenght] = '\0';
            if (file_lenght != read_size) {
                free(buffer);
                printf("ERRRORE NELLA LETTURA DEL FILE %s\n", filepath);
                return -1;
            }
            fclose(tmp);
            if (appendToFile(filepath, (void *) buffer, (size_t) file_lenght, removed_file_directory) == 0) {
                closeFile(filepath);
            }
            free(buffer);
        }
        else if (errno == 2) {
            if (openFile(filepath, 1) == 0)
                if (writeFile(filepath, removed_file_directory) == 0)
                    closeFile(filepath);
        }
        file_limit--;
    }
    else if (file_limit == -1) {
        if (openFile(filepath, 0) == 0) {
            FILE * tmp;
            if((tmp = fopen(filepath, "r")) == NULL) {
                printf("Errore nell' apertura del file %s\n", filepath);
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
                printf("ERRRORE NELLA LETTURA DEL FILE\n");
                return -1;
            }
            fclose(tmp);
            if (appendToFile(filepath, (void *) buffer, (size_t) file_lenght, removed_file_directory) == 0) {
                closeFile(filepath);
            }
            free(buffer);
        }
        else if (errno == 2) {//il file esiste già
            if (openFile(filepath, 1) == 0) {
                if (writeFile(filepath, removed_file_directory) == 0)
                    closeFile(filepath);
            }
        }
    }   
    return 0;
}

//APRE E SCRIVE IL FILE filepath SUL FILE SYSTEM
int write_request( char *filepath, char * dirname) {
    errno = 0;
    if (!is_regular_file(filepath)) {
        printf("Errore , %s non è un file !\n", filepath);
        return -1;
    }

    FILE * tmp;
    if((tmp = fopen(filepath, "r")) == NULL) {
        printf("Errore nell' apertura del file %s\n", filepath);
        return -1;
    }    
    if (openFile(filepath, 0) == 0) {
        fseek(tmp, 0, SEEK_END);
        int file_lenght = ftell(tmp);
        rewind(tmp);
        char * buffer = (char *) malloc(2*file_lenght*sizeof(char));
        int read_size = fread(buffer, sizeof(char), file_lenght, tmp);
        buffer[file_lenght] = '\0';
        if (file_lenght != read_size) {
            free(buffer);
            buffer = NULL;
            printf("ERRRORE NELLA LETTURA DEL FILE  %s\n", filepath);
            return -1;
        }
        fclose(tmp);
        if (appendToFile(filepath, (void *) buffer, (size_t) file_lenght, dirname) == 0) {
            closeFile(filepath);
        }
        free(buffer);
    }
    else if (errno == 2) {//il file esiste già
        if (openFile(filepath, 1) == 0) {
            if (writeFile(filepath, dirname) == 0)
                closeFile(filepath);
        }
    }

    return 0;
}

//VISITA LA CARTELLA dirpath ESTRAENDO number_of_files FILE
int visit_directory_tree(const char *const dirpath, int number_of_files, char * dirname)
{           
    if (!is_directory(dirpath)) {
        printf("Errore , %s non è una cartella !\n", dirpath);
        return -1;
        }

    int result;
    if(number_of_files)
        file_limit = number_of_files;
    else 
        file_limit = -1;

    removed_file_directory = dirname;

    
    /* Invalid directory path? */
    if (dirpath == NULL || *dirpath == '\0' || strlen(dirpath) == 0 ) {
        printf("ERRORE\n");
        return errno = EINVAL;
    }

    result = nftw(dirpath, openEntry, USE_FDS, FTW_PHYS);
    if (result >= 0) {
        errno = result;
    }
    

    return errno;
}

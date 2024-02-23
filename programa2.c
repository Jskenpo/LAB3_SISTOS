#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <omp.h>

#define SIZE 9

int board[SIZE][SIZE];  // Arreglo bidimensional global

#define MAX_FILENAME_LENGTH 100
#define BUFFER_SIZE 1024

void loadBoardFromFile(int archivo) {
    // Obtener el tamaño del archivo
    struct stat sb;
    fstat(archivo, &sb);

    // Mapear el archivo
    char* map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, archivo, 0);
    if (map == MAP_FAILED) {
        close(archivo);
        perror("Error al mapear el archivo");
        exit(EXIT_FAILURE);
    }

    // Copiar los valores del mapa a la matriz
    int index = 0;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            board[i][j] = map[index++] - '0'; // Convertir char a int
        }
    }

    // Desmapear
    munmap(map, sb.st_size);
}

int scanFile(){
    char filename[MAX_FILENAME_LENGTH];
    printf("Ingrese el nombre del archivo: ");
    fgets(filename, MAX_FILENAME_LENGTH, stdin);

    if (filename[strlen(filename) - 1] == '\n') {
        filename[strlen(filename) - 1] = '\0';
    }

    int archivo = open(filename, O_RDONLY);
    if (archivo == -1) {
        perror("Error al abrir el archivo");
        return 1;
    }
    loadBoardFromFile(archivo);
    return archivo;
}

void loadSudoku(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    char *data = mmap(NULL, SIZE * SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Error al mapear el archivo a memoria");
        close(fd);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            board[i][j] = data[i * SIZE + j] - '0';
        }
    }

    munmap(data, SIZE * SIZE);
    close(fd);
}

void *validateColumns(void *args){
    bool valid = true;
    for (int i = 0; i<9;i++){
        for (int j = 0; j<9; j++){
            int number = board[i][j];
            int positionY = j;
            if (number == -1) continue;
            for (int x = 0; x<9; x++){
                // verificar que no esten tambien el la misma posicion

                if (board[x][j] == number && x != i){
                    printf("Error: El número %d se repite en la columna %d.\n", number, j+1);
                    printf("Error de columna en el proceso: %ld\n", syscall(SYS_gettid));
                    valid = false;
                }
            }
            
        }
        printf("Validación de columna completada en el proceso: %ld\n", syscall(SYS_gettid));
    }
    return (void *)valid;
}

void *validateRows(void *arg) {
    bool valid = true;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int number = board[i][j];
            if (number == -1) continue;

            for (int y = 0; y < 9; y++) {
                if (board[i][y] == number && y != j) {
                    printf("Error: El número %d se repite en la fila %d.\n", number, i+1);
                    printf("Error de fila en el proceso: %ld\n", syscall(SYS_gettid));
                    valid = false;
                }
            }
            
        }
        printf("Validación de fila completada en el proceso: %ld\n", syscall(SYS_gettid));
    }
    sleep(3);
    return (void *)valid;
}

bool checkRow() {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int number = board[i][j];
            if (number == -1) continue;

            for (int y = 0; y < 9; y++) {
                if (board[i][y] == number && y != j) {
                    return false;
                }
            }
            
        }
    }
    sleep(3);
    return true;
}

bool checkSquare() {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int number = board[i][j];
            if (number == -1) continue;

            int startX = 3 * (i / 3);
            int startY = 3 * (j / 3);

            for (int x = startX; x < startX + 3; x++) {
                for (int y = startY; y < startY + 3; y++) {
                    if (board[x][y] == number && x != i && y != j) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void *validateSubgrids(void *arg) {
    bool valid = true;
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int number = board[i][j];
            if (number == -1) continue;

            int startX = 3 * (i / 3);
            int startY = 3 * (j / 3);

            for (int x = startX; x < startX + 3; x++) {
                for (int y = startY; y < startY + 3; y++) {
                    if (board[x][y] == number && x != i && y != j) {
                        printf("Error: El número %d se repite en el subgrid.\n", number);
                        printf("Error de subgrid en el proceso: %ld\n", syscall(SYS_gettid));
                        valid = false;
                    }
                    
                }
            }
        }
        printf("Validación de subgrid completada en el proceso: %ld\n", syscall(SYS_gettid));
    }
    sleep(3);
    return (void *)valid;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s sudoku\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    loadSudoku(argv[1]);

    printf("Sudoku cargado correctamente.\n");

    pthread_t columnThread;
    pthread_create(&columnThread, NULL, validateColumns, NULL);

    pid_t childPid = fork();

    if (childPid == -1) {
        perror("Error al hacer fork");
        exit(EXIT_FAILURE);
    }

    if (childPid == 0) {  // Proceso hijo
        char parentProc[16];
        sprintf(parentProc, "%d", getppid());
        execlp("ps", "ps", "-p", parentProc, "-lLf", NULL);
        perror("Error al ejecutar execlp");
        exit(EXIT_FAILURE);
    } else {  // Proceso padre
        pthread_join(columnThread, NULL);
        printf("Número de hilo en ejecución: %ld\n", syscall(SYS_gettid));

        int status;
        waitpid(childPid, &status, 0);

        // Realiza revisión de filas
        bool validRow = checkRow();

        // Realiza revisión de cuadrantes
        bool validSquare = checkSquare();    

        printf("---------------Resultado---------------\n");
        if (validRow && validSquare) {
            printf("La solucion implementada es incorrecta .\n");
        }else{
            printf("La solucion implementada es correcta.\n");
        }

        pid_t newChildPid = fork();

        if (newChildPid == -1) {
            perror("Error al hacer fork");
            exit(EXIT_FAILURE);
        }

        if (newChildPid == 0) {  // Nuevo proceso hijo
            char parentProc[16];
            sprintf(parentProc, "%d", getppid());
            execlp("ps", "ps", "-p", parentProc, "-lLf", NULL);
            perror("Error al ejecutar execlp");
            exit(EXIT_FAILURE);
        } else {  // Proceso padre esperando al nuevo hijo
            int newStatus;
            waitpid(newChildPid, &newStatus, 0);
        }
    }

    return 0;
}

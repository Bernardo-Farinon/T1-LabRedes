#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <dirent.h>

#define MAX_CLIENTS 4
#define BUFFER_SIZE 4096  
#define CMD_LIST 1
#define CMD_PUT  2
#define CMD_QUIT 3

int clientes_ativos = 0;
pthread_mutex_t lock;

// le exatamente "length" bytes
int read_all(int sock, char *buf, int length) {
    int total = 0, n;
    while (total < length) {
        n = read(sock, buf + total, length - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

// envia frame no formato [cmd(1)][length(2)][data(length)] sendo cmd um byte length dois bytes big-endian e data o payload
int send_frame(int sock, uint8_t cmd, uint16_t length, const char *data) {
    uint8_t header[3];
    header[0] = cmd;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;

    if (write(sock, header, 3) != 3) return -1;
    if (length > 0 && write(sock, data, length) != length) return -1;
    return 0;
}

// recebe frame no formato [cmd(1)][length(2)][data(length)] ...
int recv_frame(int sock, uint8_t *cmd, uint16_t *length, char *data) {
    uint8_t header[3];
    int n = read_all(sock, (char*)header, 3);
    if (n <= 0) return -1;

    *cmd = header[0];
    *length = (header[1] << 8) | header[2];

    if (*length > 0) {
        n = read_all(sock, data, *length);
        if (n <= 0) return -1;
        data[*length] = '\0'; 
    }
    return 0;
}

// thrread geral para cada cliente
void *client_handler(void *arg) {
    int connfd = *(int*)arg;
    free(arg);

    uint8_t cmd;
    uint16_t length;
    char buffer[BUFFER_SIZE];

    pthread_mutex_lock(&lock);
    clientes_ativos++;
    pthread_mutex_unlock(&lock);

    FILE *log = fopen("server_log.txt", "a");
    if (!log) {
        perror("erro ao abrir log do servidor");
        return NULL;
    }

    printf("Novo cliente conectado. Clientes ativos: %d\n", clientes_ativos);
    fprintf(log, "Novo cliente conectado. Clientes ativos: %d\n", clientes_ativos);
    fflush(log);

    while (recv_frame(connfd, &cmd, &length, buffer) == 0) {
        if (cmd == CMD_QUIT) {
            printf("Cliente pediu quit\n");
            break;
        }
        else if (cmd == CMD_LIST) {
            char list[BUFFER_SIZE] = "";
            system("mkdir -p storage"); 

            DIR *dp = opendir("./storage");
            if (dp != NULL) {
                struct dirent *entry;
                while ((entry = readdir(dp))) {
                    if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                        strcat(list, entry->d_name);
                        strcat(list, "\n");
                    }
                }
                closedir(dp);
            }

            if (strlen(list) == 0) {
                strcpy(list, "(Storage vazio)\n");
            }

            send_frame(connfd, CMD_LIST, strlen(list), list);
        }
        else if (cmd == CMD_PUT) {
            // Primeiro payload = nome do arquivo
            char filename[256];
            strncpy(filename, buffer, length);
            filename[length] = '\0';

            char filepath[300];
            snprintf(filepath, sizeof(filepath), "./storage/%s", filename);

            FILE *f = fopen(filepath, "wb");
            if (!f) {
                perror("Erro ao criar arquivo");
                break;
            }

            printf("Recebendo arquivo: %s\n", filename);
            fprintf(log, "Recebendo arquivo: %s\n", filename);
            fflush(log);

            // Recebe frames subsequentes com dados do arquivo 4 KB por vez até receber frame vazio
            while (recv_frame(connfd, &cmd, &length, buffer) == 0 && cmd == CMD_PUT) {
                if (length == 0) break; // fim do arquivo
                fwrite(buffer, 1, length, f);
            }
            fclose(f);
            
            printf("Arquivo %s recebido com sucesso!\n", filename);
            fprintf(log, "Arquivo %s recebido com sucesso!\n", filename);
            fflush(log);
        }
        else {
            printf("Comando desconhecido: %d\n", cmd);
        }
    }
    close(connfd);

    pthread_mutex_lock(&lock);
    clientes_ativos--;
    pthread_mutex_unlock(&lock);

    printf("Cliente desconectado. Clientes ativos: %d\n", clientes_ativos);
    fprintf(log, "Cliente desconectado. Clientes ativos: %d\n", clientes_ativos);
    fflush(log);

    fclose(log);
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <porta>\n", argv[0]);
        return 1;
    }

    int sockfd, *connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    pthread_mutex_init(&lock, NULL);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no bind");
        return 1;
    }
    
    if (listen(sockfd, 5) < 0) {
        perror("Erro no listen");
        return 1;
    }

    printf("Servidor rodando na porta %s\n", argv[1]);

    while (1) {
        connfd = malloc(sizeof(int));
        len = sizeof(cliaddr);
        *connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);

        pthread_mutex_lock(&lock);
        if (clientes_ativos >= MAX_CLIENTS) {
            pthread_mutex_unlock(&lock);
            printf("Máximo de clientes atingido. Conexão rejeitada.\n");
            close(*connfd);
            free(connfd);
            continue;
        }
        pthread_mutex_unlock(&lock);

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, connfd);
        pthread_detach(tid);
    }

    close(sockfd);
    pthread_mutex_destroy(&lock);
    return 0;
}

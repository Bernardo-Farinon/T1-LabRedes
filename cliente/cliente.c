#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define CMD_LIST 1
#define CMD_PUT  2
#define CMD_QUIT 3

long total_sent = 0;
long total_received = 0;
struct timeval start_time, end_time;

// Lê exatamente "length" bytes
int read_all(int sock, char *buf, int length) {
    int total = 0, n;
    while (total < length) {
        n = read(sock, buf + total, length - total);
        if (n <= 0) return -1;
        total += n;
        total_received += n;
    }
    return total;
}

// Envia frame
int send_frame(int sock, uint8_t cmd, uint16_t length, const char *data) {
    uint8_t header[3];
    header[0] = cmd;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;

    if (write(sock, header, 3) != 3) return -1;
    total_sent += 3;
    if (length > 0) {
        int n = write(sock, data, length);
        if (n != length) return -1;
        total_sent += n;
    }
    return 0;
}

// Recebe frame
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

void print_log() {
    gettimeofday(&end_time, NULL);
    timersub(&end_time, &start_time, &end_time);
    double duration = end_time.tv_sec + end_time.tv_usec / 1000000.0;

    FILE *log = fopen("client_log.txt", "a"); // abre para append
    if (!log) {
        perror("Erro abrindo log");
        return;
    }

    fprintf(log, "\n===== LOG DE CONEXÃO =====\n");
    fprintf(log, "Tempo de duração: %.2f segundos\n", duration);
    fprintf(log, "Bytes enviados: %ld\n", total_sent);
    fprintf(log, "Bytes recebidos: %ld\n", total_received);
    fprintf(log, "Taxa média: %.2f bytes/s\n", (total_sent + total_received) / duration);
    fprintf(log, "==========================\n");

    fclose(log);

    // terminal
    printf("\n===== LOG DE CONEXÃO =====\n");
    printf("Tempo de duração: %.2f segundos\n", duration);
    printf("Bytes enviados: %ld\n", total_sent);
    printf("Bytes recebidos: %ld\n", total_received);
    printf("Taxa média: %.2f bytes/s\n", (total_sent + total_received) / duration);
    printf("==========================\n");
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <ip> <porta>\n", argv[0]);
        return 1;
    }

    int sockfd;
    struct sockaddr_in serv;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv.sin_family = AF_INET;
    serv.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("Falha na conexão");
        return 1;
    }

    gettimeofday(&start_time, NULL); // início da conexão
    printf("Conectado ao servidor %s:%s\n", argv[1], argv[2]);

    while (1) {
        printf("\nDigite comando (list, put <arquivo>, quit): ");
        fgets(buffer, sizeof(buffer), stdin);

        buffer[strcspn(buffer, "\n")] = 0; 

        if (strncmp(buffer, "list", 4) == 0) {
            send_frame(sockfd, CMD_LIST, 0, NULL);

            uint8_t cmd;
            uint16_t length;
            if (recv_frame(sockfd, &cmd, &length, buffer) == 0 && cmd == CMD_LIST) {
                printf("Arquivos no servidor:\n%s\n", buffer);
            }
        }
        else if (strncmp(buffer, "put ", 4) == 0) {
            char *filename = buffer + 4;

            FILE *f = fopen(filename, "rb");
            if (!f) {
                perror("Erro abrindo arquivo");
                continue;
            }

            /* faz uma copia nome do arquivo, sobrescreve o buffer com dados depois
            char fname_copy[256];
            strncpy(fname_copy, filename, sizeof(fname_copy) - 1);
            fname_copy[sizeof(fname_copy) - 1] = '\0';*/

            // envia nome do arquivo
            send_frame(sockfd, CMD_PUT, strlen(filename), filename);

            printf("Arquivo %s enviado\n", filename);

            // frame com dados do arquivo 4 KB por vez
            int n;
            while ((n = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
                send_frame(sockfd, CMD_PUT, n, buffer);
            }
            fclose(f);

            // envia frame vazio para marcar fim
            send_frame(sockfd, CMD_PUT, 0, NULL);

        }
        else if (strncmp(buffer, "quit", 4) == 0) {
            send_frame(sockfd, CMD_QUIT, 0, NULL);
            break;
        }
        else {
            printf("Comando inválido\n");
        }
    }

    close(sockfd);
    gettimeofday(&end_time, NULL); // fim da conexão
    print_log();

    return 0;
}



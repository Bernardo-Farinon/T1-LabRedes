#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define port 8080

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <ip> \n", argv[0]);
        return 1;
    }

    int sockfd1;
    struct sockaddr_in serv1;
    char buffer[1024];
    
    sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    serv1.sin_family = AF_INET;
    serv1.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv1.sin_addr);
    connect(sockfd1, (struct sockaddr*)&serv1, sizeof(serv1));
    printf("Conectado ao servidor 1: %s:%s\n", argv[1], argv[2]);



    while (1) {
        printf("\nDigite uma mensagem: ");
        fgets(buffer, sizeof(buffer), stdin);

        write(sockfd1, buffer, strlen(buffer));

        if (strncmp(buffer, "FIM", 3) == 0 || strncmp(buffer, "fim", 3) == 0) {
            printf("Conexao encerrada\n");
            break;
        }

        int n1 = read(sockfd1, buffer, sizeof(buffer)-1);
        buffer[n1] = '\0';
        printf("Resposta do servidor 1: %s\n", buffer);

    }

    close(sockfd1);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <porta>\n", argv[0]);
        return 1;
    }

    int sockfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char buffer[BUFFER_SIZE];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        perror("Falha ao criar socket");
        exit(EXIT_FAILURE);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(atoi(argv[1]));

    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Falha no bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    if(listen(sockfd, 5) < 0) {
        perror("Falha no listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor rodando na porta: %s\n", argv[1]);

    len = sizeof(cliaddr);
    connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);

    if (connfd < 0) {
        perror("Falha no accept");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Cliente conectado %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

    while (1) {
        int n = read(connfd, buffer, sizeof(buffer)-1);
        if (n <= 0) break;
        buffer[n] = '\0';

        printf("Recebido do cliente: %s", buffer);
        printf("Enviado ao cliente: %s", buffer);
        write(connfd, buffer, n);
    }

    close(connfd);
    close(sockfd);
    return 0;
}

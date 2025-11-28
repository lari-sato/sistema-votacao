#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define TAM_BUFFER 1024

void enviarLinha(int descritor, const char *linha) {
    send(descritor, linha, strlen(linha), 0);
    send(descritor, "\n", 1, 0);
}

int receberLinha(int descritor, char *buffer, int tamanho) {
    int indice = 0;
    while (indice < tamanho - 1) {
        char caractere;
        int bytes = recv(descritor, &caractere, 1, 0);
        if (bytes <= 0) break;
        if (caractere == '\n') break;
        if (caractere == '\r') continue;
        buffer[indice++] = caractere;
    }
    buffer[indice] = '\0';
    return indice;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <host> <porta> <VOTER_ID>\n", argv[0]);
        exit(1);
    }

    char *host = argv[1];
    int porta = atoi(argv[2]);
    char *idEleitor = argv[3];

    int descritorSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (descritorSocket < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in enderecoServidor;
    memset(&enderecoServidor, 0, sizeof(enderecoServidor));
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_port = htons(porta);

    if (inet_pton(AF_INET, host, &enderecoServidor.sin_addr) <= 0) {
        perror("inet_pton");
        close(descritorSocket);
        exit(1);
    }

    if (connect(descritorSocket,
                (struct sockaddr *)&enderecoServidor,
                sizeof(enderecoServidor)) < 0) {
        perror("connect");
        close(descritorSocket);
        exit(1);
    }

    char buffer[TAM_BUFFER];

    /* HELLO <VOTER_ID> */
    char linhaHello[128];
    snprintf(linhaHello, sizeof(linhaHello), "HELLO %s", idEleitor);
    enviarLinha(descritorSocket, linhaHello);

    if (receberLinha(descritorSocket, buffer, sizeof(buffer)) <= 0) {
        fprintf(stderr, "Erro ao receber WELCOME do servidor.\n");
        close(descritorSocket);
        return 1;
    }

    if (strncmp(buffer, "WELCOME ", 8) == 0) {
        char idRecebido[64] = {0};
        strncpy(idRecebido, buffer + 8, sizeof(idRecebido) - 1);

        printf("Conectado como %s.\n%s\n",
               idRecebido, buffer);
    } else {
        fprintf(stderr, "Resposta inesperada do servidor: %s\n", buffer);
        close(descritorSocket);
        return 1;
    }

    int executando = 1;

    while (executando) {
        printf("\n-------MENU-------\n");
        printf("1) Ver opções\n");
        printf("2) Votar\n");
        printf("3) Placar\n");
        printf("4) Encerrar conexão\n");

        char entrada[32];
        printf("\nOpção: ");

        if (!fgets(entrada, sizeof(entrada), stdin)) {
            printf("Erro de leitura.\n");
            break;
        }

        entrada[strcspn(entrada, "\r\n")] = 0;

        char *endptr;
        long valor = strtol(entrada, &endptr, 10);

        if (endptr == entrada || *endptr != '\0') {
            printf("Opção inválida.\n");
            continue;
        }

        int opcao = (int)valor;

        if (opcao == 1) {
            /* LIST */
            enviarLinha(descritorSocket, "LIST");
            if (receberLinha(descritorSocket, buffer, sizeof(buffer)) > 0) {
                printf("%s\n", buffer);    // OPTIONS / ERR
            } else {
                printf("Erro ao receber resposta do servidor.\n");
                break;
            }

        } else if (opcao == 2) {
            /* VOTE <OPCAO> */
            char nomeOpcao[64];
            printf("Digite o nome da opção: ");
            if (!fgets(nomeOpcao, sizeof(nomeOpcao), stdin)) {
                printf("Erro de leitura.\n");
                break;
            }
            nomeOpcao[strcspn(nomeOpcao, "\r\n")] = 0;

            char linhaVoto[128];
            snprintf(linhaVoto, sizeof(linhaVoto), "VOTE %s", nomeOpcao);
            enviarLinha(descritorSocket, linhaVoto);

            if (receberLinha(descritorSocket, buffer, sizeof(buffer)) > 0) {
                printf("%s\n", buffer);    // OK VOTED / ERR DUPLICATE / ERR INVALID_OPTION / ERR CLOSED
            } else {
                printf("Erro ao receber resposta do servidor.\n");
                break;
            }

        } else if (opcao == 3) {
            /* SCORE */
            enviarLinha(descritorSocket, "SCORE");
            if (receberLinha(descritorSocket, buffer, sizeof(buffer)) > 0) {
                printf("%s\n", buffer);    // SCORE ... ou CLOSED FINAL ...
            } else {
                printf("Erro ao receber resposta do servidor.\n");
                break;
            }

        } else if (opcao == 4) {
            /* BYE */
            enviarLinha(descritorSocket, "BYE");
            if (receberLinha(descritorSocket, buffer, sizeof(buffer)) > 0) {
                printf("%s\n", buffer);    // BYE
            }
            executando = 0;

        } else {
            printf("Opção inválida.\n");
        }
    }

    close(descritorSocket);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

#define TAM_BUFFER 1024
#define MAX_OPCOES 10
#define MAX_ELEITORES 1000

typedef struct {
    char nome[64];
    int votos;
} OpcaoVoto;

typedef struct {
    char idEleitor[64];
} Eleitor;

int descritorServidor;
OpcaoVoto opcoesVoto[MAX_OPCOES];
int quantidadeOpcoes = 0;

Eleitor listaEleitores[MAX_ELEITORES];
int quantidadeEleitores = 0;

int votacaoEncerrada = 0;

pthread_mutex_t mutexVotos = PTHREAD_MUTEX_INITIALIZER;
FILE *arquivoLog = NULL;

/* PROTÓTIPOS */
void gravarResultadoFinal(void);
void encerrarEleicaoAdmin(void);
void *threadAdmin(void *arg);

/* LOG */
void obterTimestamp(char *buffer, size_t tamanho) {
    time_t agora = time(NULL);
    struct tm tmAgora;
    localtime_r(&agora, &tmAgora);
    strftime(buffer, tamanho, "%Y-%m-%d %H:%M:%S", &tmAgora);
}

void registrarLog(const char *idEleitor, const char *comando, const char *resultado) {
    if (!arquivoLog) return;

    char timestamp[32];
    obterTimestamp(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&mutexVotos);
    fprintf(arquivoLog, "%s [%s] %s %s\n",
            timestamp,
            (idEleitor && idEleitor[0]) ? idEleitor : "SEM_ID",
            comando,
            resultado);
    fflush(arquivoLog);
    pthread_mutex_unlock(&mutexVotos);
}

/* LÓGICA */
int eleitorJaVotou(const char *idEleitor) {
    for (int i = 0; i < quantidadeEleitores; i++) {
        if (strcmp(listaEleitores[i].idEleitor, idEleitor) == 0) return 1;
    }
    return 0;
}

void registrarEleitor(const char *idEleitor) {
    if (quantidadeEleitores >= MAX_ELEITORES) return;

    strncpy(listaEleitores[quantidadeEleitores].idEleitor, idEleitor,
            sizeof(listaEleitores[quantidadeEleitores].idEleitor) - 1);
    listaEleitores[quantidadeEleitores].idEleitor[
        sizeof(listaEleitores[quantidadeEleitores].idEleitor) - 1] = '\0';

    quantidadeEleitores++;
}

int obterIndiceOpcao(const char *nomeOpcao) {
    for (int i = 0; i < quantidadeOpcoes; i++) {
        if (strcmp(opcoesVoto[i].nome, nomeOpcao) == 0) return i;
    }
    return -1;
}

void enviarLinha(int descritorCliente, const char *linha) {
    send(descritorCliente, linha, strlen(linha), 0);
    send(descritorCliente, "\n", 1, 0);
}

/* RESULTADO FINAL */
void gravarResultadoFinal(void) {
    FILE *arquivoResultado = fopen("resultado_final.txt", "w");
    if (!arquivoResultado) return;

    fprintf(arquivoResultado, "RESULTADO_FINAL %d\n", quantidadeOpcoes);
    for (int i = 0; i < quantidadeOpcoes; i++) {
        fprintf(arquivoResultado, "%s:%d\n", opcoesVoto[i].nome, opcoesVoto[i].votos);
    }
    fclose(arquivoResultado);
}

/* ADMIN */
void encerrarEleicaoAdmin(void) {
    pthread_mutex_lock(&mutexVotos);
    if (!votacaoEncerrada) {
        votacaoEncerrada = 1;
        gravarResultadoFinal();
    }
    pthread_mutex_unlock(&mutexVotos);

    registrarLog("ADMIN", "ADMIN_CLOSE", "CLOSED_FINAL");
}

void *threadAdmin(void *arg) {
    (void)arg;
    char linha[128];

    printf("[ADMIN] Digite 'ADMIN CLOSE' para encerrar a eleição.\n\n");
    fflush(stdout);

    while (fgets(linha, sizeof(linha), stdin)) {
        linha[strcspn(linha, "\r\n")] = '\0';

        if (strcmp(linha, "ADMIN CLOSE") == 0) {
            encerrarEleicaoAdmin();
        }
    }

    return NULL;
}

/* THREAD DO CLIENTE */
void *threadCliente(void *arg) {
    int descritorCliente = *(int *)arg;
    free(arg);

    char buffer[TAM_BUFFER];
    char idEleitorAtual[64] = "";
    int helloFeito = 0;

    while (1) {
        ssize_t bytesRecebidos = recv(descritorCliente, buffer, sizeof(buffer) - 1, 0);
        if (bytesRecebidos <= 0) break;
        buffer[bytesRecebidos] = '\0';
        
        char *p = buffer;
        while (*p && *p != '\r' && *p != '\n') p++;
        *p = '\0';

        if (buffer[0] == '\0') {
            continue;
        }

        printf("%s\n", buffer);
        fflush(stdout);

        if (strncmp(buffer, "HELLO ", 6) == 0) {
            strncpy(idEleitorAtual, buffer + 6, sizeof(idEleitorAtual) - 1);
            idEleitorAtual[sizeof(idEleitorAtual) - 1] = '\0';
            helloFeito = 1;

            char resposta[128];
            snprintf(resposta, sizeof(resposta), "WELCOME %s", idEleitorAtual);
            enviarLinha(descritorCliente, resposta);

            registrarLog(idEleitorAtual, "HELLO", "OK");

        } else if (strcmp(buffer, "LIST") == 0) {
            if (!helloFeito) {
                enviarLinha(descritorCliente, "ERR NO_HELLO");
                registrarLog("SEM_ID", "LIST", "ERR_NO_HELLO");
                continue;
            }

            char linha[TAM_BUFFER];
            snprintf(linha, sizeof(linha), "OPTIONS %d", quantidadeOpcoes);
            for (int i = 0; i < quantidadeOpcoes; i++) {
                strcat(linha, " ");
                strcat(linha, opcoesVoto[i].nome);
            }
            enviarLinha(descritorCliente, linha);
            registrarLog(idEleitorAtual, "LIST", "OK");

        } else if (strncmp(buffer, "VOTE ", 5) == 0) {
            if (!helloFeito) {
                enviarLinha(descritorCliente, "ERR NO_HELLO");
                registrarLog("SEM_ID", "VOTE", "ERR_NO_HELLO");
                continue;
            }

            char *nomeOpcao = buffer + 5;

            // voto sob UM único lock 
            pthread_mutex_lock(&mutexVotos);

            if (votacaoEncerrada) {
                pthread_mutex_unlock(&mutexVotos);
                enviarLinha(descritorCliente, "ERR CLOSED");
                registrarLog(idEleitorAtual, "VOTE", "ERR_CLOSED");
                continue;
            }

            if (eleitorJaVotou(idEleitorAtual)) {
                pthread_mutex_unlock(&mutexVotos);
                enviarLinha(descritorCliente, "ERR DUPLICATE");
                registrarLog(idEleitorAtual, "VOTE", "ERR_DUPLICATE");
                continue;
            }

            int indice = obterIndiceOpcao(nomeOpcao);
            if (indice < 0) {
                pthread_mutex_unlock(&mutexVotos);
                enviarLinha(descritorCliente, "ERR INVALID_OPTION");
                registrarLog(idEleitorAtual, "VOTE", "ERR_INVALID_OPTION");
                continue;
            }

            // voto válido
            opcoesVoto[indice].votos++;
            registrarEleitor(idEleitorAtual);

            pthread_mutex_unlock(&mutexVotos);

            char resposta[128];
            snprintf(resposta, sizeof(resposta), "OK VOTED %s", nomeOpcao);
            enviarLinha(descritorCliente, resposta);

            registrarLog(idEleitorAtual, "VOTE", nomeOpcao);

        } else if (strcmp(buffer, "SCORE") == 0) {
            pthread_mutex_lock(&mutexVotos);
            char linha[TAM_BUFFER];

            if (votacaoEncerrada) {
                snprintf(linha, sizeof(linha), "CLOSED FINAL %d", quantidadeOpcoes);
            } else {
                snprintf(linha, sizeof(linha), "SCORE %d", quantidadeOpcoes);
            }

            for (int i = 0; i < quantidadeOpcoes; i++) {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), " %s:%d",
                         opcoesVoto[i].nome, opcoesVoto[i].votos);
                strcat(linha, tmp);
            }
            pthread_mutex_unlock(&mutexVotos);

            enviarLinha(descritorCliente, linha);
            registrarLog(helloFeito ? idEleitorAtual : "SEM_ID",
                         "SCORE",
                         votacaoEncerrada ? "CLOSED_FINAL" : "OK");

        } else if (strcmp(buffer, "BYE") == 0) {
            enviarLinha(descritorCliente, "BYE");
            registrarLog(helloFeito ? idEleitorAtual : "SEM_ID", "BYE", "OK");
            break;

        } else {
            enviarLinha(descritorCliente, "ERR UNKNOWN");
            registrarLog(helloFeito ? idEleitorAtual : "SEM_ID", "UNKNOWN", buffer);
        }
    }

    close(descritorCliente);
    return NULL;
}

/* MAIN */
int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <porta> <op1> <op2> <op3> [...]\n", argv[0]);
        exit(1);
    }

    int porta = atoi(argv[1]);
    quantidadeOpcoes = argc - 2;
    if (quantidadeOpcoes < 3) {
        fprintf(stderr, "É necessário pelo menos 3 opções de voto.\n");
        exit(1);
    }
    if (quantidadeOpcoes > MAX_OPCOES) {
        fprintf(stderr, "Limite máximo de opções: %d\n", MAX_OPCOES);
        exit(1);
    }

    for (int i = 0; i < quantidadeOpcoes; i++) {
        strncpy(opcoesVoto[i].nome, argv[i + 2],
                sizeof(opcoesVoto[i].nome) - 1);
        opcoesVoto[i].nome[sizeof(opcoesVoto[i].nome) - 1] = '\0';
        opcoesVoto[i].votos = 0;
    }

    arquivoLog = fopen("eleicao.log", "a");

    struct sockaddr_in enderecoServidor, enderecoCliente;
    socklen_t tamanhoCliente = sizeof(enderecoCliente);

    descritorServidor = socket(AF_INET, SOCK_STREAM, 0);
    if (descritorServidor < 0) {
        perror("socket");
        exit(1);
    }

    int opcao = 1;
    setsockopt(descritorServidor, SOL_SOCKET, SO_REUSEADDR, &opcao, sizeof(opcao));

    memset(&enderecoServidor, 0, sizeof(enderecoServidor));
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_addr.s_addr = INADDR_ANY;
    enderecoServidor.sin_port = htons(porta);

    if (bind(descritorServidor,
             (struct sockaddr *)&enderecoServidor,
             sizeof(enderecoServidor)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(descritorServidor, 20) < 0) {
        perror("listen");
        exit(1);
    }

    printf("[CONEXÃO] Servidor conectado: porta %d.\n", porta);

    // thread 'ADMIN CLOSE'
    pthread_t adminThreadId;
    if (pthread_create(&adminThreadId, NULL, threadAdmin, NULL) == 0) {
        pthread_detach(adminThreadId);
    } else {
        fprintf(stderr, "Falha ao criar thread administrativa.\n");
    }

    while (1) {
        int *descritorClientePtr = malloc(sizeof(int));
        if (!descritorClientePtr) continue;

        *descritorClientePtr = accept(descritorServidor,
                                      (struct sockaddr *)&enderecoCliente,
                                      &tamanhoCliente);
        if (*descritorClientePtr < 0) {
            perror("accept");
            free(descritorClientePtr);
            continue;
        }

        pthread_t idThread;
        pthread_create(&idThread, NULL, threadCliente, descritorClientePtr);
        pthread_detach(idThread);
    }

    if (arquivoLog) fclose(arquivoLog);
    close(descritorServidor);
    return 0;
}

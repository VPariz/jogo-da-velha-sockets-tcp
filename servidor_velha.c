/**
 * ============================================================================
 * INSTITUTO MILITAR DE ENGENHARIA - IME
 * DISCIPLINA: REDES DE COMPUTADORES I
 * 
 * PROJETO: JOGO DA VELHA DISTRIBUIDO VIA SOCKETS TCP (POSIX / WINSOCK2)
 * ARQUIVO: servidor_velha.c
 * 
 * AUTORES:
 *   - Pedro Augusto (Peu)
 *   - Joao Victor Pariz
 *   - Mateus Roma
 * 
 * DESCRICAO:
 *   Implementacao do no servidor para partida multiplayer de Jogo da Velha
 *   sobre TCP. Gerencia o ciclo de vida do socket passivo, sorteio de ordem
 *   de jogada (START:SERVER / START:CLIENT), sincronizacao de turnos e
 *   validacao independente de matriz de estado.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_NET_ERROR() WSAGetLastError()
    #define INIT_NETWORKING() do { \
        SetConsoleOutputCP(CP_UTF8); \
        SetConsoleCP(CP_UTF8); \
        WSADATA wsa; \
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { \
            fprintf(stderr, "[ERRO FATAL] Falha ao iniciar Winsock: %d\n", WSAGetLastError()); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)
    #define CLEANUP_NETWORKING() WSACleanup()
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define CLOSE_SOCKET(s) close(s)
    #define GET_NET_ERROR() errno
    #define INIT_NETWORKING()    ((void)0)
    #define CLEANUP_NETWORKING() ((void)0)
#endif

#define PORTA_PADRAO 6000
#define BUFFER_SIZE  128

#define SIMBOLO_SERVIDOR 'O'
#define SIMBOLO_CLIENTE  'X'

/**
 * Inspeciona interfaces de rede locais e exibe os enderecos IPv4 disponiveis.
 */
void exibir_ips_locais(int porta) {
    char hostname[256];

    printf("[INFO] Porta TCP: %d\n", porta);

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("[INFO] Hostname: %s\n", hostname);

        struct hostent *host_info = gethostbyname(hostname);
        if (host_info != NULL && host_info->h_addr_list != NULL) {
            printf("[INFO] Enderecos IP para conexao do Cliente:\n");
            for (int i = 0; host_info->h_addr_list[i] != NULL; i++) {
                struct in_addr addr;
                memcpy(&addr, host_info->h_addr_list[i], sizeof(struct in_addr));
                printf("       -> %s (Rede Local)\n", inet_ntoa(addr));
            }
            printf("       -> 127.0.0.1 (Loopback local)\n");
        }
    }
    printf("----------------------------------------------------\n");
}

void exibir_tabuleiro_guia(void) {
    printf("\n");
    printf(" 1 | 2 | 3 \n");
    printf("---+---+---\n");
    printf(" 4 | 5 | 6 \n");
    printf("---+---+---\n");
    printf(" 7 | 8 | 9 \n\n");
}

void exibir_tabuleiro(const char tabuleiro[9]) {
    printf("\n");
    printf(" %c | %c | %c \n", tabuleiro[0], tabuleiro[1], tabuleiro[2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n", tabuleiro[3], tabuleiro[4], tabuleiro[5]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n\n", tabuleiro[6], tabuleiro[7], tabuleiro[8]);
}

/**
 * Avalia condicoes de termino:
 *   1 -> Vitoria de 'X' (Cliente)
 *   2 -> Vitoria de 'O' (Servidor)
 *   0 -> Empate (todas preenchidas)
 *  -1 -> Jogo em andamento
 */
int verificar_termino(const char tab[9]) {
    const int linhas[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
        {0, 4, 8}, {2, 4, 6}
    };

    for (int i = 0; i < 8; i++) {
        int a = linhas[i][0];
        int b = linhas[i][1];
        int c = linhas[i][2];

        if (tab[a] != ' ' && tab[a] == tab[b] && tab[b] == tab[c]) {
            if (tab[a] == 'X') return 1;
            if (tab[a] == 'O') return 2;
        }
    }

    for (int i = 0; i < 9; i++) {
        if (tab[i] == ' ') {
            return -1;
        }
    }

    return 0;
}

/**
 * Leitura robusta da jogada com sanitizacao de buffer.
 */
int obter_jogada_usuario(const char tabuleiro[9], char simbolo) {
    char linha[128];
    int celula;

    while (1) {
        printf("Vez de %c celula > ", simbolo);
        fflush(stdout);

        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            printf("\nErro na leitura da entrada. Tente novamente.\n");
            clearerr(stdin);
            continue;
        }

        if (strchr(linha, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        linha[strcspn(linha, "\r\n")] = '\0';

        char *inicio = linha;
        while (*inicio == ' ' || *inicio == '\t') inicio++;

        char *fim = inicio + strlen(inicio) - 1;
        while (fim > inicio && (*fim == ' ' || *fim == '\t')) {
            *fim = '\0';
            fim--;
        }

        if (strlen(inicio) != 1 || inicio[0] < '1' || inicio[0] > '9') {
            printf("Entrada invalida! Digite apenas um unico numero de 1 a 9.\n");
            continue;
        }

        celula = (inicio[0] - '1');

        if (tabuleiro[celula] != ' ') {
            printf("Posicao invalida! A celula %d ja esta ocupada (%c). Escolha outra.\n",
                   celula + 1, tabuleiro[celula]);
            continue;
        }

        return celula + 1;
    }
}

int main(int argc, char *argv[]) {
    SOCKET listen_sd = INVALID_SOCKET;
    SOCKET client_sd = INVALID_SOCKET;
    struct sockaddr_in servAddr;
    struct sockaddr_in cliAddr;
    socklen_t cliLen;
    char tabuleiro[9];
    char buffer[BUFFER_SIZE];
    int rc;

    INIT_NETWORKING();

    int porta = PORTA_PADRAO;
    if (argc > 1) {
        int p = atoi(argv[1]);
        if (p > 1024 && p <= 65535) {
            porta = p;
        } else {
            printf("[AVISO] Porta informada invalida. Usando porta padrao %d.\n", PORTA_PADRAO);
        }
    }

    printf("====================================================\n");
    printf("   JOGO DA VELHA - SERVIDOR TCP\n");
    printf("====================================================\n");

    exibir_ips_locais(porta);

    listen_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sd == INVALID_SOCKET) {
        fprintf(stderr, "[ERRO] Nao foi possivel criar socket. Codigo: %d\n", GET_NET_ERROR());
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons((unsigned short)porta);

    rc = bind(listen_sd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (rc == SOCKET_ERROR) {
        fprintf(stderr, "[ERRO] Falha no bind na porta %d. Codigo: %d\n", porta, GET_NET_ERROR());
        CLOSE_SOCKET(listen_sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    rc = listen(listen_sd, 1);
    if (rc == SOCKET_ERROR) {
        fprintf(stderr, "[ERRO] Falha no listen. Codigo: %d\n", GET_NET_ERROR());
        CLOSE_SOCKET(listen_sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    printf("[INFO] Servidor aguardando conexao do Cliente na porta %d...\n", porta);

    cliLen = sizeof(cliAddr);
    client_sd = accept(listen_sd, (struct sockaddr *)&cliAddr, &cliLen);
    if (client_sd == INVALID_SOCKET) {
        fprintf(stderr, "[ERRO] Falha ao aceitar conexao. Codigo: %d\n", GET_NET_ERROR());
        CLOSE_SOCKET(listen_sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    printf("Conexao estabelecida com sucesso com %s:%d!\n\n",
           inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));

    CLOSE_SOCKET(listen_sd);
    listen_sd = INVALID_SOCKET;

    for (int i = 0; i < 9; i++) {
        tabuleiro[i] = ' ';
    }

    srand((unsigned int)time(NULL));
    int sorteio = rand() % 2;

    const char *msg_sorteio = (sorteio == 0) ? "START:SERVER\n" : "START:CLIENT\n";
    rc = send(client_sd, msg_sorteio, (int)strlen(msg_sorteio), 0);
    if (rc == SOCKET_ERROR) {
        fprintf(stderr, "[ERRO] Falha ao enviar mensagem de sorteio. Codigo: %d\n", GET_NET_ERROR());
        CLOSE_SOCKET(client_sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    printf("--- SORTEIO DE INICIO ---\n");
    if (sorteio == 0) {
        printf("Resultado: O SERVIDOR ('O') inicia a partida!\n");
    } else {
        printf("Resultado: O CLIENTE ('X') inicia a partida!\n");
    }
    printf("-------------------------\n");

    printf("\nPosicoes de referencia do tabuleiro:");
    exibir_tabuleiro_guia();

    int vez_do_servidor = (sorteio == 0) ? 1 : 0;
    int fim_de_jogo = 0;

    while (!fim_de_jogo) {
        exibir_tabuleiro(tabuleiro);

        if (vez_do_servidor) {
            int jogada = obter_jogada_usuario(tabuleiro, SIMBOLO_SERVIDOR);
            tabuleiro[jogada - 1] = SIMBOLO_SERVIDOR;

            snprintf(buffer, sizeof(buffer), "MOVE:%d\n", jogada);
            rc = send(client_sd, buffer, (int)strlen(buffer), 0);
            if (rc == SOCKET_ERROR) {
                fprintf(stderr, "\n[ERRO] Falha de conexao ao enviar jogada. Codigo: %d\n", GET_NET_ERROR());
                break;
            }

            int status = verificar_termino(tabuleiro);
            if (status != -1) {
                exibir_tabuleiro(tabuleiro);
                if (status == 2) {
                    printf("\n====================================================\n");
                    printf("  FIM DE JOGO: PARABENS! VOCE (SERVIDOR) VENCEU!\n");
                    printf("====================================================\n");
                } else if (status == 0) {
                    printf("\n====================================================\n");
                    printf("  FIM DE JOGO: DEU VELHA! (EMPATE)\n");
                    printf("====================================================\n");
                }
                fim_de_jogo = 1;
                break;
            }

            vez_do_servidor = 0;
        } else {
            printf("Aguardando jogada do Cliente ('X')...\n");
            fflush(stdout);

            memset(buffer, 0, sizeof(buffer));
            rc = recv(client_sd, buffer, sizeof(buffer) - 1, 0);

            if (rc > 0) {
                buffer[rc] = '\0';
                buffer[strcspn(buffer, "\r\n")] = '\0';

                int jogada_cliente = -1;
                if (strncmp(buffer, "MOVE:", 5) == 0) {
                    jogada_cliente = atoi(buffer + 5);
                } else {
                    jogada_cliente = atoi(buffer);
                }

                if (jogada_cliente < 1 || jogada_cliente > 9 || tabuleiro[jogada_cliente - 1] != ' ') {
                    printf("[ERRO PROTOCOLO] Jogada invalida recebida do cliente: '%s'\n", buffer);
                    break;
                }

                printf("Cliente jogou na posicao %d\n", jogada_cliente);
                tabuleiro[jogada_cliente - 1] = SIMBOLO_CLIENTE;

                int status = verificar_termino(tabuleiro);
                if (status != -1) {
                    exibir_tabuleiro(tabuleiro);
                    if (status == 1) {
                        printf("\n====================================================\n");
                        printf("  FIM DE JOGO: O CLIENTE VENCEU!\n");
                        printf("====================================================\n");
                    } else if (status == 0) {
                        printf("\n====================================================\n");
                        printf("  FIM DE JOGO: DEU VELHA! (EMPATE)\n");
                        printf("====================================================\n");
                    }
                    fim_de_jogo = 1;
                    break;
                }

                vez_do_servidor = 1;
            } else if (rc == 0) {
                printf("\n[AVISO] Conexao encerrada pelo adversario (Cliente).\n");
                break;
            } else {
                fprintf(stderr, "\n[ERRO] Conexao perdida inesperadamente. Codigo: %d\n", GET_NET_ERROR());
                break;
            }
        }
    }

    printf("\n====================================================\n");
    printf("Partida finalizada. Encerrando sockets...\n");

    if (client_sd != INVALID_SOCKET) {
        CLOSE_SOCKET(client_sd);
    }

    CLEANUP_NETWORKING();

    printf("Recursos liberados com sucesso.\n");
    return 0;
}

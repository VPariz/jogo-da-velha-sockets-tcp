/**
 * ============================================================================
 * INSTITUTO MILITAR DE ENGENHARIA - IME
 * DISCIPLINA: REDES DE COMPUTADORES I
 * 
 * PROJETO: JOGO DA VELHA DISTRIBUIDO VIA SOCKETS TCP (POSIX / WINSOCK2)
 * ARQUIVO: cliente_velha.c
 * 
 * AUTOR:
 *   - Joao Victor Pariz
 * 
 * DESCRICAO:
 *   Implementacao do no cliente para partida multiplayer de Jogo da Velha
 *   sobre TCP. Conecta ativamente ao servidor via handshake de 3 vias,
 *   interpreta o sorteio inicial, envia jogadas saneadas ("MOVE:<1-9>") e
 *   mantem o estado local do tabuleiro sincronizado.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    SOCKET sd = INVALID_SOCKET;
    struct sockaddr_in servAddr;
    char tabuleiro[9];
    char buffer[BUFFER_SIZE];
    int rc;

    INIT_NETWORKING();

    printf("====================================================\n");
    printf("   JOGO DA VELHA - CLIENTE TCP\n");
    printf("====================================================\n");

    char server_ip_str[128] = "";
    int porta = PORTA_PADRAO;

    if (argc > 1) {
        strncpy(server_ip_str, argv[1], sizeof(server_ip_str) - 1);
        server_ip_str[sizeof(server_ip_str) - 1] = '\0';
    }

    if (argc > 2) {
        int p = atoi(argv[2]);
        if (p > 1024 && p <= 65535) {
            porta = p;
        }
    }

    while (strlen(server_ip_str) == 0) {
        printf("\n[INFO] Digite o IP do servidor [Enter para 127.0.0.1]: ");
        if (fgets(server_ip_str, sizeof(server_ip_str), stdin) == NULL) {
            strcpy(server_ip_str, "127.0.0.1");
            break;
        }
        server_ip_str[strcspn(server_ip_str, "\r\n")] = '\0';
        if (strlen(server_ip_str) == 0) {
            strcpy(server_ip_str, "127.0.0.1");
        }
    }

    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd == INVALID_SOCKET) {
        fprintf(stderr, "[ERRO] Nao foi possivel criar socket. Codigo: %d\n", GET_NET_ERROR());
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(server_ip_str);
    servAddr.sin_port = htons((unsigned short)porta);

    if (servAddr.sin_addr.s_addr == INADDR_NONE && strcmp(server_ip_str, "255.255.255.255") != 0) {
        fprintf(stderr, "[ERRO] Endereco IP invalido: '%s'.\n", server_ip_str);
        CLOSE_SOCKET(sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    printf("[INFO] Conectando a %s:%d...\n", server_ip_str, porta);
    rc = connect(sd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (rc == SOCKET_ERROR) {
        fprintf(stderr, "[ERRO] Nao foi possivel conectar ao servidor %s:%d. Codigo: %d\n",
                server_ip_str, porta, GET_NET_ERROR());
        CLOSE_SOCKET(sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }
    printf("Conexao estabelecida com sucesso!\n\n");

    for (int i = 0; i < 9; i++) {
        tabuleiro[i] = ' ';
    }

    printf("Aguardando sorteio da ordem de jogada pelo Servidor...\n");
    memset(buffer, 0, sizeof(buffer));
    rc = recv(sd, buffer, sizeof(buffer) - 1, 0);
    if (rc <= 0) {
        fprintf(stderr, "[ERRO] Falha ao receber mensagem de sorteio do servidor.\n");
        CLOSE_SOCKET(sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }
    buffer[rc] = '\0';

    int vez_do_cliente = 0;
    if (strstr(buffer, "START:CLIENT") != NULL) {
        vez_do_cliente = 1;
        printf("--- SORTEIO DE INICIO ---\n");
        printf("Resultado: VOCE (CLIENTE - 'X') comeca jogando!\n");
        printf("-------------------------\n");
    } else if (strstr(buffer, "START:SERVER") != NULL) {
        vez_do_cliente = 0;
        printf("--- SORTEIO DE INICIO ---\n");
        printf("Resultado: O SERVIDOR ('O') comeca jogando!\n");
        printf("-------------------------\n");
    } else {
        fprintf(stderr, "[ERRO PROTOCOLO] Mensagem de inicio nao reconhecida: '%s'\n", buffer);
        CLOSE_SOCKET(sd);
        CLEANUP_NETWORKING();
        return EXIT_FAILURE;
    }

    printf("\nPosicoes de referencia do tabuleiro:");
    exibir_tabuleiro_guia();

    int fim_de_jogo = 0;

    while (!fim_de_jogo) {
        exibir_tabuleiro(tabuleiro);

        if (vez_do_cliente) {
            int jogada = obter_jogada_usuario(tabuleiro, SIMBOLO_CLIENTE);
            tabuleiro[jogada - 1] = SIMBOLO_CLIENTE;

            snprintf(buffer, sizeof(buffer), "MOVE:%d\n", jogada);
            rc = send(sd, buffer, (int)strlen(buffer), 0);
            if (rc == SOCKET_ERROR) {
                fprintf(stderr, "\n[ERRO] Falha de conexao ao enviar jogada. Codigo: %d\n", GET_NET_ERROR());
                break;
            }

            int status = verificar_termino(tabuleiro);
            if (status != -1) {
                exibir_tabuleiro(tabuleiro);
                if (status == 1) {
                    printf("\n====================================================\n");
                    printf("  FIM DE JOGO: PARABENS! VOCE (CLIENTE) VENCEU!\n");
                    printf("====================================================\n");
                } else if (status == 0) {
                    printf("\n====================================================\n");
                    printf("  FIM DE JOGO: DEU VELHA! (EMPATE)\n");
                    printf("====================================================\n");
                }
                fim_de_jogo = 1;
                break;
            }

            vez_do_cliente = 0;
        } else {
            printf("Aguardando jogada do Servidor ('O')...\n");
            fflush(stdout);

            memset(buffer, 0, sizeof(buffer));
            rc = recv(sd, buffer, sizeof(buffer) - 1, 0);

            if (rc > 0) {
                buffer[rc] = '\0';
                buffer[strcspn(buffer, "\r\n")] = '\0';

                int jogada_servidor = -1;
                if (strncmp(buffer, "MOVE:", 5) == 0) {
                    jogada_servidor = atoi(buffer + 5);
                } else {
                    jogada_servidor = atoi(buffer);
                }

                if (jogada_servidor < 1 || jogada_servidor > 9 || tabuleiro[jogada_servidor - 1] != ' ') {
                    printf("[ERRO PROTOCOLO] Jogada invalida recebida do servidor: '%s'\n", buffer);
                    break;
                }

                printf("Servidor jogou na posicao %d\n", jogada_servidor);
                tabuleiro[jogada_servidor - 1] = SIMBOLO_SERVIDOR;

                int status = verificar_termino(tabuleiro);
                if (status != -1) {
                    exibir_tabuleiro(tabuleiro);
                    if (status == 2) {
                        printf("\n====================================================\n");
                        printf("  FIM DE JOGO: O SERVIDOR VENCEU!\n");
                        printf("====================================================\n");
                    } else if (status == 0) {
                        printf("\n====================================================\n");
                        printf("  FIM DE JOGO: DEU VELHA! (EMPATE)\n");
                        printf("====================================================\n");
                    }
                    fim_de_jogo = 1;
                    break;
                }

                vez_do_cliente = 1;
            } else if (rc == 0) {
                printf("\n[AVISO] Conexao encerrada pelo adversario (Servidor).\n");
                break;
            } else {
                fprintf(stderr, "\n[ERRO] Conexao perdida inesperadamente. Codigo: %d\n", GET_NET_ERROR());
                break;
            }
        }
    }

    printf("\n====================================================\n");
    printf("Partida finalizada. Encerrando sockets...\n");

    if (sd != INVALID_SOCKET) {
        CLOSE_SOCKET(sd);
    }

    CLEANUP_NETWORKING();

    printf("Recursos liberados com sucesso.\n");
    return 0;
}

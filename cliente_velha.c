/**
 * ============================================================================
 * TRABALHO DE REDES DE COMPUTADORES - ENGENHARIA ELETRONICA I (2026.2)
 * JOGO DA VELHA CLIENTE-SERVIDOR VIA SOCKETS TCP (WINDOWS / WINSOCK)
 * Arquivo: cliente_velha.c
 * 
 * Funcionalidades:
 *   1. Inicializa o Winsock e abre o socket TCP ativo.
 *   2. Permite informar o IP do servidor via argumento de terminal:
 *        .\cliente_velha.exe [IP_DO_SERVIDOR] [PORTA]
 *      ou de forma interativa (pressionando Enter para usar 127.0.0.1).
 *   3. Nao exige edicao de codigo nem recompilacao para trocar de maquina.
 *   4. Conecta-se ao Servidor TCP (porta padrao 6000).
 *   5. Recebe mensagem de controle com o resultado do sorteio da moeda.
 *   6. Mantem o estado do tabuleiro local e sincroniza as jogadas.
 *   7. Valida jogadas (formato, limites 1-9 e casas ja ocupadas).
 *   8. Valida vitoria, derrota e empate a cada jogada em ambos os lados.
 *   9. Trata quedas abruptas de conexao e fecha os sockets graciosamente.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define PORTA_PADRAO 6000
#define BUFFER_SIZE  128

// Simbolos dos jogadores
#define SIMBOLO_SERVIDOR 'O'
#define SIMBOLO_CLIENTE  'X'

/**
 * Exibe o tabuleiro guia com numeros de 1 a 9 (formato exato da folha de instrucoes)
 */
void exibir_tabuleiro_guia(void) {
    printf("\n");
    printf(" 1 | 2 | 3 \n");
    printf("---+---+---\n");
    printf(" 4 | 5 | 6 \n");
    printf("---+---+---\n");
    printf(" 7 | 8 | 9 \n\n");
}

/**
 * Exibe o tabuleiro da partida atualizado
 */
void exibir_tabuleiro(const char tabuleiro[9]) {
    printf("\n");
    printf(" %c | %c | %c \n", tabuleiro[0], tabuleiro[1], tabuleiro[2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n", tabuleiro[3], tabuleiro[4], tabuleiro[5]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n\n", tabuleiro[6], tabuleiro[7], tabuleiro[8]);
}

/**
 * Verifica as condicoes de termino da partida
 * Retorno:
 *   1  -> Vitoria de 'X' (Cliente)
 *   2  -> Vitoria de 'O' (Servidor)
 *   0  -> Empate ("Deu velha")
 *  -1  -> Partida ainda em andamento
 */
int verificar_termino(const char tab[9]) {
    // 8 alinhamentos vitoriosos possiveis
    const int linhas[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // Horizontais
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // Verticais
        {0, 4, 8}, {2, 4, 6}             // Diagonais
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

    // Verifica se ainda ha espaco livre
    for (int i = 0; i < 9; i++) {
        if (tab[i] == ' ') {
            return -1; // Ainda ha jogo
        }
    }

    return 0; // Tabuleiro preenchido sem vencedor: empate
}

/**
 * Le e valida a entrada do usuario para a jogada
 * Trata:
 *  - Entradas vazias
 *  - Letras e simbolos especiais
 *  - Multiplos caracteres (ex: 12, abc)
 *  - Numeros fora do intervalo de 1 a 9
 *  - Celulas ja ocupadas
 */
int obter_jogada_usuario(const char tabuleiro[9], char simbolo) {
    char linha[128];
    int celula;

    while (1) {
        printf("Vez de %c célula > ", simbolo);
        fflush(stdout);

        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            printf("\nErro na leitura da entrada. Tente novamente.\n");
            clearerr(stdin);
            continue;
        }

        // Se o usuario digitou mais de 127 caracteres, consome o resto da linha
        if (strchr(linha, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        // Remove quebra de linha (\r e \n)
        linha[strcspn(linha, "\r\n")] = '\0';

        // Remove espacos em branco nas bordas
        char *inicio = linha;
        while (*inicio == ' ' || *inicio == '\t') inicio++;

        char *fim = inicio + strlen(inicio) - 1;
        while (fim > inicio && (*fim == ' ' || *fim == '\t')) {
            *fim = '\0';
            fim--;
        }

        // Validacao: exatamente 1 digito entre '1' e '9'
        if (strlen(inicio) != 1 || inicio[0] < '1' || inicio[0] > '9') {
            printf("Entrada invalida! Digite apenas um unico numero de 1 a 9.\n");
            continue;
        }

        celula = (inicio[0] - '1'); // Indice 0 a 8

        // Validacao: posicao ja ocupada
        if (tabuleiro[celula] != ' ') {
            printf("Posicao invalida! A celula %d ja esta ocupada (%c). Escolha outra.\n",
                   celula + 1, tabuleiro[celula]);
            continue;
        }

        return celula + 1; // Retorna numero da celula (1 a 9)
    }
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET sd = INVALID_SOCKET;
    struct sockaddr_in servAddr;
    char server_ip_str[128] = "";
    int porta = PORTA_PADRAO;
    char tabuleiro[9];
    char buffer[BUFFER_SIZE];
    int rc;

    // Configura suporte a UTF-8 no console Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    printf("====================================================\n");
    printf("   JOGO DA VELHA - CLIENTE TCP\n");
    printf("====================================================\n");

    // 1. Inicializacao do Winsock
    printf("\n[1] Inicializando Winsock (WS2_32.dll)...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[ERRO FATAL] Falha ao iniciar Winsock. Codigo: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    printf("    Winsock inicializado com sucesso.\n");

    // 2. Definicao do IP e Porta do Servidor (via argumentos ou entrada interativa)
    if (argc > 1) {
        strncpy(server_ip_str, argv[1], sizeof(server_ip_str) - 1);
    }
    if (argc > 2) {
        int p = atoi(argv[2]);
        if (p > 1024 && p <= 65535) {
            porta = p;
        }
    }

    while (strlen(server_ip_str) == 0) {
        printf("\n[2] Digite o IP do servidor [Enter para 127.0.0.1]: ");
        if (fgets(server_ip_str, sizeof(server_ip_str), stdin) == NULL) {
            strcpy(server_ip_str, "127.0.0.1");
            break;
        }
        server_ip_str[strcspn(server_ip_str, "\r\n")] = '\0';
        if (strlen(server_ip_str) == 0) {
            strcpy(server_ip_str, "127.0.0.1");
        }
    }

    // 3. Criacao do socket TCP (SOCK_STREAM)
    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd == INVALID_SOCKET) {
        printf("[ERRO] Nao foi possivel criar socket. Codigo: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    // 4. Configuracao do endereco do servidor
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(server_ip_str);
    servAddr.sin_port = htons((u_short)porta);

    if (servAddr.sin_addr.s_addr == INADDR_NONE && strcmp(server_ip_str, "255.255.255.255") != 0) {
        printf("[ERRO] Endereco IP invalido: '%s'. Verifique o IP digitado.\n", server_ip_str);
        closesocket(sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // 5. Conectar ao Servidor
    printf("    Conectando a %s:%d...\n", server_ip_str, porta);
    rc = connect(sd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Nao foi possivel conectar ao servidor %s:%d. Codigo: %d\n",
               server_ip_str, porta, WSAGetLastError());
        printf("       Dicas:\n");
        printf("       - Certifique-se de iniciar o 'servidor_velha.exe' primeiro.\n");
        printf("       - Verifique se o IP informado esta correto.\n");
        printf("       - Se estiver no mesmo PC, use 127.0.0.1.\n");
        closesocket(sd);
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("    Conexao estabelecida com sucesso!\n\n");

    // 6. Inicializacao da estrutura do tabuleiro
    for (int i = 0; i < 9; i++) {
        tabuleiro[i] = ' ';
    }

    // 7. Recebimento da mensagem de controle de sorteio do Servidor
    printf("Aguardando sorteio da ordem de jogada pelo Servidor...\n");
    memset(buffer, 0, sizeof(buffer));
    rc = recv(sd, buffer, sizeof(buffer) - 1, 0);
    if (rc <= 0) {
        printf("[ERRO] Falha ao receber mensagem de sorteio do servidor.\n");
        closesocket(sd);
        WSACleanup();
        return EXIT_FAILURE;
    }
    buffer[rc] = '\0';

    int vez_do_cliente = 0;
    if (strstr(buffer, "START:CLIENT") != NULL) {
        vez_do_cliente = 1;
    } else if (strstr(buffer, "START:SERVER") != NULL) {
        vez_do_cliente = 0;
    } else {
        printf("[ERRO] Protocolo de inicializacao desconhecido: %s\n", buffer);
        closesocket(sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("====================================================\n");
    printf("   CONFIGURACAO DA PARTIDA\n");
    printf("   Cliente (Voce): Simbolo '%c'\n", SIMBOLO_CLIENTE);
    printf("   Servidor:       Simbolo '%c'\n", SIMBOLO_SERVIDOR);
    if (vez_do_cliente) {
        printf("   Sorteio realizado: VOCÊ (Cliente) começa a partida!\n");
    } else {
        printf("   Sorteio realizado: SERVIDOR começa a partida!\n");
    }
    printf("====================================================\n");

    // Exibe tabuleiro de referencia numerado (exemplo da folha)
    exibir_tabuleiro_guia();

    // 8. Ciclo principal de turnos
    int jogo_ativo = 1;
    while (jogo_ativo) {
        if (vez_do_cliente) {
            // --- TURNO DO CLIENTE ---
            int jogada = obter_jogada_usuario(tabuleiro, SIMBOLO_CLIENTE);
            tabuleiro[jogada - 1] = SIMBOLO_CLIENTE;

            // Exibe tabuleiro atualizado
            exibir_tabuleiro(tabuleiro);

            // Transmite coordenadas da jogada ao Servidor
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "MOVE:%d\n", jogada);
            rc = send(sd, buffer, (int)strlen(buffer), 0);
            if (rc == SOCKET_ERROR) {
                printf("\n[ERRO] Falha de conexao ao enviar jogada. Codigo: %d\n", WSAGetLastError());
                break;
            }

            // Validacao de termino
            int status = verificar_termino(tabuleiro);
            if (status == 1) {
                printf("*********************************\n");
                printf("          Você venceu!\n");
                printf("*********************************\n");
                jogo_ativo = 0;
            } else if (status == 0) {
                printf("*********************************\n");
                printf("             Empate!\n");
                printf("*********************************\n");
                jogo_ativo = 0;
            } else {
                // Alterna turno para o Servidor
                vez_do_cliente = 0;
            }

        } else {
            // --- TURNO DO SERVIDOR (Cliente aguarda) ---
            printf("Aguardando jogada do adversario (Servidor)...\n");

            memset(buffer, 0, sizeof(buffer));
            rc = recv(sd, buffer, sizeof(buffer) - 1, 0);

            if (rc > 0) {
                buffer[rc] = '\0';
                int jogada_serv = 0;

                if (sscanf(buffer, "MOVE:%d", &jogada_serv) == 1 &&
                    jogada_serv >= 1 && jogada_serv <= 9 &&
                    tabuleiro[jogada_serv - 1] == ' ') {

                    tabuleiro[jogada_serv - 1] = SIMBOLO_SERVIDOR;
                    printf("Adversario jogou na celula %d.\n", jogada_serv);
                    exibir_tabuleiro(tabuleiro);

                    // Validacao de termino
                    int status = verificar_termino(tabuleiro);
                    if (status == 2) {
                        printf("*********************************\n");
                        printf("           Você perdeu!\n");
                        printf("*********************************\n");
                        jogo_ativo = 0;
                    } else if (status == 0) {
                        printf("*********************************\n");
                        printf("             Empate!\n");
                        printf("*********************************\n");
                        jogo_ativo = 0;
                    } else {
                        // Alterna turno para o Cliente
                        vez_do_cliente = 1;
                    }
                } else {
                    printf("[ERRO] Mensagem de jogada invalida recebida do servidor: %s\n", buffer);
                    break;
                }
            } else if (rc == 0) {
                printf("\n[AVISO] Conexao encerrada pelo adversario (Servidor).\n");
                break;
            } else {
                printf("\n[ERRO] Conexao perdida inesperadamente. Codigo: %d\n", WSAGetLastError());
                break;
            }
        }
    }

    // 9. Encerramento das conexoes e liberacao de recursos
    printf("\n====================================================\n");
    printf("Partida finalizada. Encerrando sockets e Winsock...\n");
    if (sd != INVALID_SOCKET) {
        closesocket(sd);
    }
    WSACleanup();
    printf("Recursos liberados com sucesso.\n");
    printf("Pressione Enter para encerrar o programa.\n");
    getchar();

    return 0;
}

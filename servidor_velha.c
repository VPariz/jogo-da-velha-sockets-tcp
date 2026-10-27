/**
 * ============================================================================
 * TRABALHO DE REDES DE COMPUTADORES - ENGENHARIA ELETRONICA I (2026.2)
 * JOGO DA VELHA CLIENTE-SERVIDOR VIA SOCKETS TCP (WINDOWS / WINSOCK)
 * Arquivo: servidor_velha.c
 * 
 * Funcionalidades:
 *   1. Inicializa o Winsock e abre o socket TCP passivo (escuta).
 *   2. Detecta e exibe na tela os IPs da propria maquina (dispensa uso de ipconfig).
 *   3. Aceita conexao do Cliente (porta padrao 6000 ou passada via argumento).
 *   4. Realiza o sorteio aleatorio (moeda) para definir quem inicia a partida.
 *   5. Envia mensagem de controle notificando o Cliente sobre o sorteio.
 *   6. Mantem o estado do tabuleiro (3x3) e coordena o ciclo de turnos.
 *   7. Valida jogadas (formato, limites 1-9 e casas ja ocupadas).
 *   8. Valida vitoria, derrota e empate a cada jogada em ambos os lados.
 *   9. Trata quedas abruptas de conexao e fecha os sockets graciosamente.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>

#define PORTA_PADRAO 6000
#define BUFFER_SIZE  128

// Simbolos dos jogadores
#define SIMBOLO_SERVIDOR 'O'
#define SIMBOLO_CLIENTE  'X'

/**
 * Detecta e exibe todos os enderecos IPv4 locais desta maquina.
 * Isso permite que quem esta apresentando saiba imediatamente qual IP
 * informar para o parceiro conectar, sem precisar abrir o cmd ou dar ipconfig.
 */
void exibir_ips_locais(int porta) {
    char hostname[256];
    printf("[INFO] Porta TCP: %d\n", porta);
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("[INFO] Nome do Computador: %s\n", hostname);
        struct hostent *host_info = gethostbyname(hostname);
        if (host_info != NULL && host_info->h_addr_list != NULL) {
            printf("[INFO] Enderecos IP para o Cliente conectar:\n");
            for (int i = 0; host_info->h_addr_list[i] != NULL; i++) {
                struct in_addr addr;
                memcpy(&addr, host_info->h_addr_list[i], sizeof(struct in_addr));
                printf("       -> %s (Rede Local)\n", inet_ntoa(addr));
            }
            printf("       -> 127.0.0.1 (se Cliente e Servidor estiverem no mesmo PC)\n");
        }
    }
    printf("----------------------------------------------------\n");
}

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
    SOCKET listen_sd = INVALID_SOCKET;
    SOCKET client_sd = INVALID_SOCKET;
    struct sockaddr_in servAddr, cliAddr;
    int cliLen;
    char tabuleiro[9];
    char buffer[BUFFER_SIZE];
    int rc;

    // Configura suporte a UTF-8 no console Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Permite porta personalizada via argumento: .\servidor_velha.exe [porta]
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

    // 1. Inicializacao do Winsock
    printf("\n[1] Inicializando Winsock (WS2_32.dll)...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[ERRO FATAL] Falha ao iniciar Winsock. Codigo: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    printf("    Winsock inicializado com sucesso.\n");

    // Exibe os IPs locais para facilitar conexao do parceiro em aula
    exibir_ips_locais(porta);

    // 2. Criacao do socket de escuta TCP (SOCK_STREAM)
    listen_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sd == INVALID_SOCKET) {
        printf("[ERRO] Nao foi possivel criar socket. Codigo: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Permite reuso rapido de porta (evita TIME_WAIT em reinicios rapidos)
    int opt = 1;
    setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    // 3. Associacao (bind) com porta local
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons((u_short)porta);

    rc = bind(listen_sd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Falha no bind na porta %d. Codigo: %d\n", porta, WSAGetLastError());
        printf("       Dica: A porta pode estar em uso. Tente: .\\servidor_velha.exe 7000\n");
        closesocket(listen_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // 4. Modo de escuta (listen)
    rc = listen(listen_sd, 1);
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Falha no listen. Codigo: %d\n", WSAGetLastError());
        closesocket(listen_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("[2] Servidor aguardando conexao do Cliente na porta %d...\n", porta);

    // 5. Aceita conexao do Cliente (accept)
    cliLen = sizeof(cliAddr);
    client_sd = accept(listen_sd, (struct sockaddr *)&cliAddr, &cliLen);
    if (client_sd == INVALID_SOCKET) {
        printf("[ERRO] Falha ao aceitar conexao. Codigo: %d\n", WSAGetLastError());
        closesocket(listen_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("    Conexao estabelecida com sucesso com %s:%d!\n\n",
           inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));

    // Nao precisamos de outras conexoes; fecha o socket de escuta
    closesocket(listen_sd);
    listen_sd = INVALID_SOCKET;

    // 6. Inicializacao da estrutura do tabuleiro
    for (int i = 0; i < 9; i++) {
        tabuleiro[i] = ' ';
    }

    // 7. Sorteio aleatorio do primeiro a jogar (moeda / booleano)
    srand((unsigned int)time(NULL));
    int vez_do_servidor = rand() % 2; // 1 = Servidor comeca, 0 = Cliente comeca

    printf("====================================================\n");
    printf("   CONFIGURACAO DA PARTIDA\n");
    printf("   Servidor (Voce): Simbolo '%c'\n", SIMBOLO_SERVIDOR);
    printf("   Cliente:         Simbolo '%c'\n", SIMBOLO_CLIENTE);
    printf("====================================================\n");

    // Envia mensagem de controle informando ao Cliente quem iniciara a partida
    memset(buffer, 0, sizeof(buffer));
    if (vez_do_servidor) {
        snprintf(buffer, sizeof(buffer), "START:SERVER\n");
        printf("Sorteio realizado: SERVIDOR (Voce) comeca a partida!\n");
    } else {
        snprintf(buffer, sizeof(buffer), "START:CLIENT\n");
        printf("Sorteio realizado: CLIENTE comeca a partida!\n");
    }

    rc = send(client_sd, buffer, (int)strlen(buffer), 0);
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Falha ao enviar mensagem de sorteio. Codigo: %d\n", WSAGetLastError());
        closesocket(client_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Exibe tabuleiro de referencia numerado (exemplo da folha)
    exibir_tabuleiro_guia();

    // 8. Ciclo principal de turnos
    int jogo_ativo = 1;
    while (jogo_ativo) {
        if (vez_do_servidor) {
            // --- TURNO DO SERVIDOR ---
            int jogada = obter_jogada_usuario(tabuleiro, SIMBOLO_SERVIDOR);
            tabuleiro[jogada - 1] = SIMBOLO_SERVIDOR;

            // Exibe tabuleiro atualizado
            exibir_tabuleiro(tabuleiro);

            // Transmite coordenadas da jogada ao Cliente
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "MOVE:%d\n", jogada);
            rc = send(client_sd, buffer, (int)strlen(buffer), 0);
            if (rc == SOCKET_ERROR) {
                printf("\n[ERRO] Falha de conexao ao enviar jogada. Codigo: %d\n", WSAGetLastError());
                break;
            }

            // Validacao de termino
            int status = verificar_termino(tabuleiro);
            if (status == 2) {
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
                // Alterna turno para o Cliente
                vez_do_servidor = 0;
            }

        } else {
            // --- TURNO DO CLIENTE (Servidor aguarda) ---
            printf("Aguardando jogada do adversario (Cliente)...\n");

            memset(buffer, 0, sizeof(buffer));
            rc = recv(client_sd, buffer, sizeof(buffer) - 1, 0);

            if (rc > 0) {
                buffer[rc] = '\0';
                int jogada_cli = 0;

                if (sscanf(buffer, "MOVE:%d", &jogada_cli) == 1 &&
                    jogada_cli >= 1 && jogada_cli <= 9 &&
                    tabuleiro[jogada_cli - 1] == ' ') {

                    tabuleiro[jogada_cli - 1] = SIMBOLO_CLIENTE;
                    printf("Adversario jogou na celula %d.\n", jogada_cli);
                    exibir_tabuleiro(tabuleiro);

                    // Validacao de termino
                    int status = verificar_termino(tabuleiro);
                    if (status == 1) {
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
                        // Alterna turno para o Servidor
                        vez_do_servidor = 1;
                    }
                } else {
                    printf("[ERRO] Mensagem de jogada invalida recebida do cliente: %s\n", buffer);
                    break;
                }
            } else if (rc == 0) {
                printf("\n[AVISO] Conexao encerrada pelo adversario (Cliente).\n");
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
    if (client_sd != INVALID_SOCKET) {
        closesocket(client_sd);
    }
    WSACleanup();
    printf("Recursos liberados com sucesso.\n");
    printf("Pressione Enter para encerrar o programa.\n");
    getchar();

    return 0;
}

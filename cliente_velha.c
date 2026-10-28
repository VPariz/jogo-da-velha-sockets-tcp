/**
 * ============================================================================
 * INSTITUTO MILITAR DE ENGENHARIA - IME
 * DISCIPLINA: REDES DE COMPUTADORES I
 * 
 * PROJETO: JOGO DA VELHA CLIENTE-SERVIDOR VIA SOCKETS TCP (POSIX / WINSOCK2)
 * ARQUIVO: cliente_velha.c
 * 
 * AUTOR:
 *   - Joao Victor Pariz
 * 
 * ----------------------------------------------------------------------------
 * DESCRICAO GERAL:
 *   Este arquivo implementa o no CLIENTE da aplicacao distribuida de Jogo
 *   da Velha. O cliente atua como a entidade ativa na conexao TCP: ele
 *   inicia a comunicacao com o servidor atraves de um handshake de 3 vias.
 * 
 * FLUXO DE EXECUCAO DO CLIENTE:
 *   1. Inicializacao da biblioteca Winsock2 (WSAStartup) no Windows.
 *   2. Obtencao do IP e Porta do Servidor (via argumentos de linha de comando
 *      ou via solicitacao interativa com padrao 127.0.0.1).
 *   3. Criacao do socket ativo TCP (AF_INET, SOCK_STREAM).
 *   4. Conexao com o Servidor remoto ou local (connect).
 *   5. Recepcao da mensagem de controle de sorteio (START:SERVER ou START:CLIENT).
 *   6. Ciclo principal alternado de turnos (envio/recebimento de "MOVE:<1-9>").
 *   7. Verificacao de vitoria, derrota e empate apos cada movimento.
 *   8. Finalizacao graciosa da conexao (closesocket) e descarregamento da DLL (WSACleanup).
 * ============================================================================
 */

#include <stdio.h>      // Entrada e saida padrao (printf, fgets, fflush, etc.)
#include <stdlib.h>     // Funcoes de utilidade geral (atoi, exit)
#include <string.h>     // Manipulacao de strings e buffers (memset, strlen, strstr, etc.)
#include <winsock2.h>   // API de Sockets do Windows (Winsock 2.2)
#include <windows.h>    // Funcoes do Windows (configuracao de codepage UTF-8)

// Porta padrao utilizada para a comunicacao TCP
#define PORTA_PADRAO 6000

// Tamanho maximo do buffer para envio e recepcao de mensagens na camada de aplicacao
#define BUFFER_SIZE  128

// Convencao de simbolos dos jogadores conforme a especificacao do trabalho
#define SIMBOLO_SERVIDOR 'O'  // Servidor joga com a peca 'O'
#define SIMBOLO_CLIENTE  'X'  // Cliente joga com a peca 'X'

/**
 * ============================================================================
 * Funcao: exibir_tabuleiro_guia
 * ----------------------------------------------------------------------------
 * Finalidade:
 *   Exibe o mapa de referencia numerado (posicoes 1 a 9) para que os jogadores
 *   saibam o indice de cada celula do tabuleiro (estilo teclado numerico/grid).
 * ============================================================================
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
 * ============================================================================
 * Funcao: exibir_tabuleiro
 * ----------------------------------------------------------------------------
 * Finalidade:
 *   Desenha no console o estado atualizado do jogo (matriz 3x3 representada
 *   por um vetor unidimensional de 9 caracteres).
 * 
 * Parametros:
 *   tabuleiro -> Vetor de 9 caracteres contendo ' ', 'O' ou 'X'.
 * ============================================================================
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
 * ============================================================================
 * Funcao: verificar_termino
 * ----------------------------------------------------------------------------
 * Finalidade:
 *   Avalia se o tabuleiro atingiu uma condicao de encerramento da partida.
 * 
 * Regra:
 *   - 3 simbolos iguais alinhados (linha, coluna ou diagonal) = Vitoria.
 *   - Tabuleiro completamente preenchido sem vencedor = Empate ("velha").
 *   - Casas vagas existentes sem alinhamento = Jogo continua.
 * 
 * Retorno:
 *    1 -> Vitoria do simbolo 'X' (Cliente)
 *    2 -> Vitoria do simbolo 'O' (Servidor)
 *    0 -> Empate (todas as celulas preenchidas, sem vencedor)
 *   -1 -> Partida ainda em andamento
 * ============================================================================
 */
int verificar_termino(const char tab[9]) {
    // Matriz com as 8 combinacoes de indices que formam uma linha vencedora
    const int linhas[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // 3 Linhas Horizontais
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // 3 Linhas Verticais
        {0, 4, 8}, {2, 4, 6}             // 2 Diagonais
    };

    // Testa cada uma das 8 retas
    for (int i = 0; i < 8; i++) {
        int a = linhas[i][0];
        int b = linhas[i][1];
        int c = linhas[i][2];

        // Se a primeira celula nao esta vazia e e igual a segunda e a terceira
        if (tab[a] != ' ' && tab[a] == tab[b] && tab[b] == tab[c]) {
            if (tab[a] == 'X') return 1; // Cliente venceu
            if (tab[a] == 'O') return 2; // Servidor venceu
        }
    }

    // Verifica se ainda resta alguma celula vazia no tabuleiro
    for (int i = 0; i < 9; i++) {
        if (tab[i] == ' ') {
            return -1; // O jogo ainda nao acabou
        }
    }

    // Se nao ha casas vazias e ninguem venceu, o resultado e empate
    return 0;
}

/**
 * ============================================================================
 * Funcao: obter_jogada_usuario
 * ----------------------------------------------------------------------------
 * Finalidade:
 *   Solicita, le e valida rigorosamente a jogada digitada pelo usuario no terminal.
 * 
 * Tratamentos de robustez implementados:
 *   - fgets: evita estouro de buffer (buffer overflow), mais seguro que scanf.
 *   - Descarte de restos de linha longas via getchar() se o buffer estourar.
 *   - Remocao de terminadores de linha (\r e \n) compativeis com Windows/Linux.
 *   - Remocao de espacos em branco ou tabulacoes no inicio e no fim da string.
 *   - Rejeicao de entradas vazias, multiplos digitos ou caracteres nao numericos.
 *   - Validacao de intervalo permitido: exatamente entre 1 e 9.
 *   - Validacao de celula: rejeita posicoes que ja estejam ocupadas no tabuleiro.
 * 
 * Parametros:
 *   tabuleiro -> Estado atual do jogo para inspecionar ocupacao.
 *   simbolo   -> Simbolo da peca do jogador ('O' ou 'X') para o prompt.
 * 
 * Retorno:
 *   Numero inteiro da celula escolhida (1 a 9).
 * ============================================================================
 */
int obter_jogada_usuario(const char tabuleiro[9], char simbolo) {
    char linha[128];
    int celula;

    while (1) {
        // Exibe o prompt de jogada exatamente no formato exigido
        printf("Vez de %c célula > ", simbolo);
        fflush(stdout); // Assegura exibicao imediata no console

        // Leitura segura da linha inteira
        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            printf("\nErro na leitura da entrada. Tente novamente.\n");
            clearerr(stdin);
            continue;
        }

        // Se o usuario digitou uma linha maior que o buffer, descarta o restante ate o '\n'
        if (strchr(linha, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        // Elimina quebras de linha '\r' (CR) e '\n' (LF)
        linha[strcspn(linha, "\r\n")] = '\0';

        // Trata espacos em branco no inicio da string
        char *inicio = linha;
        while (*inicio == ' ' || *inicio == '\t') inicio++;

        // Trata espacos em branco no final da string
        char *fim = inicio + strlen(inicio) - 1;
        while (fim > inicio && (*fim == ' ' || *fim == '\t')) {
            *fim = '\0';
            fim--;
        }

        // Validacao: a entrada deve ter exatamente 1 caractere entre '1' e '9'
        if (strlen(inicio) != 1 || inicio[0] < '1' || inicio[0] > '9') {
            printf("Entrada invalida! Digite apenas um unico numero de 1 a 9.\n");
            continue;
        }

        // Converte o caractere ASCII ('1' a '9') para indice de vetor (0 a 8)
        celula = (inicio[0] - '1');

        // Validacao: a celula selecionada ja esta preenchida?
        if (tabuleiro[celula] != ' ') {
            printf("Posicao invalida! A celula %d ja esta ocupada (%c). Escolha outra.\n",
                   celula + 1, tabuleiro[celula]);
            continue;
        }

        // Entrada valida: retorna a celula em numeracao humana (1 a 9)
        return celula + 1;
    }
}

/**
 * ============================================================================
 * Funcao: main (Ponto de Entrada do Cliente)
 * ----------------------------------------------------------------------------
 * Parametros opcionais de linha de comando:
 *   argv[1] -> IP do Servidor (ex: .\cliente_velha.exe 172.16.1.160)
 *   argv[2] -> Porta TCP customizada (ex: .\cliente_velha.exe 172.16.1.160 6000)
 * ============================================================================
 */
int main(int argc, char *argv[]) {
    WSADATA wsaData;               // Estrutura com detalhes da implementacao Winsock
    SOCKET sd = INVALID_SOCKET;    // Socket ativo do Cliente
    struct sockaddr_in servAddr;   // Estrutura com endereco e porta do Servidor de destino
    char server_ip_str[128] = "";  // String que contera o endereco IP informado
    int porta = PORTA_PADRAO;      // Porta TCP de conexao (padrao: 6000)
    char tabuleiro[9];             // Vetor de estado do tabuleiro (indices 0 a 8)
    char buffer[BUFFER_SIZE];      // Buffer de comunicacao de rede
    int rc;                        // Codigo de retorno das chamadas Winsock

    // Configura o console do Windows para UTF-8 (permite acentuacao correta em pt-BR)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    printf("====================================================\n");
    printf("   JOGO DA VELHA - CLIENTE TCP\n");
    printf("====================================================\n");

    // ------------------------------------------------------------------------
    // PASSO 1: INICIALIZACAO DA BIBLIOTECA WINSOCK (WS2_32.DLL)
    // ------------------------------------------------------------------------
    // Necessario no Windows para inicializar a pilha de rede antes de usar sockets.
    printf("\n[1] Inicializando Winsock (WS2_32.dll)...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[ERRO FATAL] Falha ao iniciar Winsock. Codigo: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    printf("    Winsock inicializado com sucesso.\n");

    // ------------------------------------------------------------------------
    // PASSO 2: OBTENCAO DO ENDERECO IP E DA PORTA DO SERVIDOR
    // ------------------------------------------------------------------------
    // Permite passagem via argumento de linha de comando:
    //   Ex: .\cliente_velha.exe 192.168.1.10 6000
    if (argc > 1) {
        strncpy(server_ip_str, argv[1], sizeof(server_ip_str) - 1);
    }
    if (argc > 2) {
        int p = atoi(argv[2]);
        if (p > 1024 && p <= 65535) {
            porta = p;
        }
    }

    // Se nao informado via argumento, solicita interativamente ao usuario
    while (strlen(server_ip_str) == 0) {
        printf("\n[2] Digite o IP do servidor [Enter para 127.0.0.1]: ");
        if (fgets(server_ip_str, sizeof(server_ip_str), stdin) == NULL) {
            strcpy(server_ip_str, "127.0.0.1");
            break;
        }
        server_ip_str[strcspn(server_ip_str, "\r\n")] = '\0';
        if (strlen(server_ip_str) == 0) {
            // Pressionar <Enter> assume conexao local (mesma maquina)
            strcpy(server_ip_str, "127.0.0.1");
        }
    }

    // ------------------------------------------------------------------------
    // PASSO 3: CRIACAO DO SOCKET TCP ATIVO DO CLIENTE
    // ------------------------------------------------------------------------
    // AF_INET     -> Familia de enderecos IPv4
    // SOCK_STREAM -> Protocolo confiavel e orientado a conexao (TCP)
    // IPPROTO_TCP -> Protocolo TCP explicito
    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd == INVALID_SOCKET) {
        printf("[ERRO] Nao foi possivel criar socket. Codigo: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------------
    // PASSO 4: CONFIGURACAO DA ESTRUTURA DE ENDERECO DO SERVIDOR
    // ------------------------------------------------------------------------
    servAddr.sin_family = AF_INET;
    // inet_addr converte a string decimal pontuada (ex: "172.16.1.160") em formato binario
    servAddr.sin_addr.s_addr = inet_addr(server_ip_str);
    // htons converte o numero da porta da ordem de bytes da maquina (Host) para a ordem da rede (Network Big-Endian)
    servAddr.sin_port = htons((u_short)porta);

    // Validacao do formato do IP informado
    if (servAddr.sin_addr.s_addr == INADDR_NONE && strcmp(server_ip_str, "255.255.255.255") != 0) {
        printf("[ERRO] Endereco IP invalido: '%s'. Verifique o IP digitado.\n", server_ip_str);
        closesocket(sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------------
    // PASSO 5: ESTABELECIMENTO DA CONEXAO TCP COM O SERVIDOR (CONNECT)
    // ------------------------------------------------------------------------
    // connect() dispara o handshake de 3 vias (SYN -> SYN-ACK -> ACK).
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

    // ------------------------------------------------------------------------
    // PASSO 6: INICIALIZACAO DO TABULEIRO DE JOGO LOCAL
    // ------------------------------------------------------------------------
    for (int i = 0; i < 9; i++) {
        tabuleiro[i] = ' ';
    }

    // ------------------------------------------------------------------------
    // PASSO 7: RECEBIMENTO DA MENSAGEM DE SORTEIO (QUEM INICIA A PARTIDA)
    // ------------------------------------------------------------------------
    // O Cliente bloqueia aguardando a decisao sorteada pelo Servidor.
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

    // Interpretacao do pacote do protocolo de aplicacao
    int vez_do_cliente = 0;
    if (strstr(buffer, "START:CLIENT") != NULL) {
        vez_do_cliente = 1; // Sorteio definiu que o Cliente joga primeiro
    } else if (strstr(buffer, "START:SERVER") != NULL) {
        vez_do_cliente = 0; // Sorteio definiu que o Servidor joga primeiro
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

    // Exibe o tabuleiro de referencia inicial para orientar o jogador
    exibir_tabuleiro_guia();

    // ------------------------------------------------------------------------
    // PASSO 8: CICLO PRINCIPAL DE TURNOS DO JOGO DA VELHA
    // ------------------------------------------------------------------------
    int jogo_ativo = 1;
    while (jogo_ativo) {
        if (vez_do_cliente) {
            // ================================================================
            // TURNO ATIVO DO CLIENTE:
            //   1. Solicita a jogada do usuario local.
            //   2. Atualiza o tabuleiro local com o simbolo 'X'.
            //   3. Desenha o tabuleiro na tela.
            //   4. Envia pacote "MOVE:<celula>\n" ao servidor via rede.
            //   5. Verifica se o cliente venceu ou deu empate.
            //   6. Se o jogo continuar, passa a vez ao servidor.
            // ================================================================
            int jogada = obter_jogada_usuario(tabuleiro, SIMBOLO_CLIENTE);
            tabuleiro[jogada - 1] = SIMBOLO_CLIENTE;

            // Exibe tabuleiro com a jogada aplicada
            exibir_tabuleiro(tabuleiro);

            // Monta o pacote de aplicacao: "MOVE:N\n"
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "MOVE:%d\n", jogada);

            // Transmissao do movimento pela rede
            rc = send(sd, buffer, (int)strlen(buffer), 0);
            if (rc == SOCKET_ERROR) {
                printf("\n[ERRO] Falha de conexao ao enviar jogada. Codigo: %d\n", WSAGetLastError());
                break;
            }

            // Inspeciona condicao de vitoria/empate no tabuleiro
            int status = verificar_termino(tabuleiro);
            if (status == 1) { // Cliente ('X') alinhou 3 pecas
                printf("*********************************\n");
                printf("          Você venceu!\n");
                printf("*********************************\n");
                jogo_ativo = 0;
            } else if (status == 0) { // Empate
                printf("*********************************\n");
                printf("             Empate!\n");
                printf("*********************************\n");
                jogo_ativo = 0;
            } else {
                // Passa a vez para o Servidor
                vez_do_cliente = 0;
            }

        } else {
            // ================================================================
            // TURNO PASSIVO DO CLIENTE (Aguardando movimento do Servidor):
            //   1. Bloqueia na chamada recv() aguardando dados da rede.
            //   2. Trata desconexoes abruptas ou fechamento gracioso do servidor.
            //   3. Interpreta a mensagem "MOVE:<celula>\n".
            //   4. Atualiza o tabuleiro local com o simbolo do servidor ('O').
            //   5. Exibe o tabuleiro atualizado.
            //   6. Verifica se o servidor venceu ou deu empate.
            //   7. Se o jogo continuar, passa a vez ao cliente.
            // ================================================================
            printf("Aguardando jogada do adversario (Servidor)...\n");

            memset(buffer, 0, sizeof(buffer));
            // Chamada bloqueante ao recv: retorna numero de bytes lidos
            rc = recv(sd, buffer, sizeof(buffer) - 1, 0);

            if (rc > 0) {
                // Adiciona o caractere terminador de string nulo
                buffer[rc] = '\0';
                int jogada_serv = 0;

                // Faz o parsing seguro da mensagem de movimento recebida
                if (sscanf(buffer, "MOVE:%d", &jogada_serv) == 1 &&
                    jogada_serv >= 1 && jogada_serv <= 9 &&
                    tabuleiro[jogada_serv - 1] == ' ') {

                    // Aplica a jogada do servidor no tabuleiro local
                    tabuleiro[jogada_serv - 1] = SIMBOLO_SERVIDOR;
                    printf("Adversario jogou na celula %d.\n", jogada_serv);
                    exibir_tabuleiro(tabuleiro);

                    // Inspeciona condicao de vitoria/empate no tabuleiro
                    int status = verificar_termino(tabuleiro);
                    if (status == 2) { // Servidor ('O') alinhou 3 pecas
                        printf("*********************************\n");
                        printf("           Você perdeu!\n");
                        printf("*********************************\n");
                        jogo_ativo = 0;
                    } else if (status == 0) { // Empate
                        printf("*********************************\n");
                        printf("             Empate!\n");
                        printf("*********************************\n");
                        jogo_ativo = 0;
                    } else {
                        // Passa a vez para o Cliente
                        vez_do_cliente = 1;
                    }
                } else {
                    printf("[ERRO] Mensagem de jogada invalida recebida do servidor: %s\n", buffer);
                    break;
                }
            } else if (rc == 0) {
                // rc == 0 indica fechamento gracioso do socket pelo servidor (FIN recebido)
                printf("\n[AVISO] Conexao encerrada pelo adversario (Servidor).\n");
                break;
            } else {
                // rc < 0 indica erro de socket ou queda repentina de conexao
                printf("\n[ERRO] Conexao perdida inesperadamente. Codigo: %d\n", WSAGetLastError());
                break;
            }
        }
    }

    // ------------------------------------------------------------------------
    // PASSO 9: ENCERRAMENTO E LIBERACAO DE RECURSOS DO SISTEMA
    // ------------------------------------------------------------------------
    printf("\n====================================================\n");
    printf("Partida finalizada. Encerrando sockets e Winsock...\n");

    // Fecha o socket ativo do cliente
    if (sd != INVALID_SOCKET) {
        closesocket(sd);
    }

    // Desregistra a aplicacao do Winsock e descarrega a dll ws2_32.dll
    WSACleanup();

    printf("Recursos liberados com sucesso.\n");
    printf("Pressione Enter para encerrar o programa.\n");
    getchar();

    return 0;
}

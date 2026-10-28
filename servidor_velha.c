/**
 * ============================================================================
 * INSTITUTO MILITAR DE ENGENHARIA - IME
 * DISCIPLINA: REDES DE COMPUTADORES I (2026.2)
 * PROFESSOR: CEL. PAULO CESAR SALGADO VIDAL
 * 
 * PROJETO: JOGO DA VELHA CLIENTE-SERVIDOR VIA SOCKETS TCP (WINSOCK2 / C)
 * ARQUIVO: servidor_velha.c
 * 
 * AUTORES:
 *   - Pedro Augusto (Peu)
 *   - Joao Victor Pariz
 *   - Mateus Roma
 * 
 * ----------------------------------------------------------------------------
 * DESCRICAO GERAL:
 *   Este arquivo implementa o no SERVIDOR da aplicacao distribuida de Jogo
 *   da Velha. O servidor atua no modelo classico Cliente-Servidor baseado em
 *   fluxo confiavel de dados sobre TCP (Transmission Control Protocol).
 * 
 * FLUXO DE EXECUCAO DO SERVIDOR:
 *   1. Inicializacao da biblioteca Winsock2 (WSAStartup) no Windows.
 *   2. Descoberta e exibicao automatica dos enderecos IP locais da maquina.
 *   3. Criacao do socket passivo TCP (AF_INET, SOCK_STREAM).
 *   4. Associacao do socket ao endereco e porta local (bind).
 *   5. Colocacao do socket em modo de escuta passiva (listen).
 *   6. Bloqueio aguardando conexao do Cliente (accept), gerando o socket de dados.
 *   7. Sorteio pseudoaleatorio (moeda) para definir quem inicia a partida.
 *   8. Transmissao da mensagem de controle de inicio (START:SERVER ou START:CLIENT).
 *   9. Ciclo principal alternado de turnos (envio/recebimento de "MOVE:<1-9>").
 *  10. Verificacao de vitoria, derrota e empate apos cada movimento.
 *  11. Finalizacao graciosa da conexao (closesocket) e descarregamento da DLL (WSACleanup).
 * ============================================================================
 */

#include <stdio.h>      // Entrada e saida padrao (printf, fgets, fflush, etc.)
#include <stdlib.h>     // Funcoes de utilidade geral (rand, srand, atoi, exit)
#include <string.h>     // Manipulacao de strings e buffers (memset, strlen, etc.)
#include <time.h>       // Funcoes de temporizacao para semente do rand (time)
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
 * Funcao: exibir_ips_locais
 * ----------------------------------------------------------------------------
 * Finalidade:
 *   Inspeciona as interfaces de rede do computador e imprime na tela todos os
 *   enderecos IPv4 locais disponiveis.
 * 
 * Por que isso e importante?
 *   Em uma apresentacao de laboratorio (ou redes com DHCP), o computador pode
 *   ter seu endereco IP alterado dinamicamente. Esta funcao elimina a necessidade
 *   de abrir outro terminal e executar 'ipconfig', exibindo diretamente o IP
 *   que o jogador cliente devera informar para se conectar.
 * 
 * Parametros:
 *   porta -> Porta TCP em que o servidor estara escutando.
 * ============================================================================
 */
void exibir_ips_locais(int porta) {
    char hostname[256];

    printf("[INFO] Porta TCP: %d\n", porta);

    // Obtem o nome de rede do computador local (hostname)
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("[INFO] Nome do Computador: %s\n", hostname);

        // Consulta a estrutura de informacoes de rede associada ao hostname
        struct hostent *host_info = gethostbyname(hostname);

        // Percorre a lista de enderecos IP vinculados as placas de rede do host
        if (host_info != NULL && host_info->h_addr_list != NULL) {
            printf("[INFO] Enderecos IP para o Cliente conectar:\n");
            for (int i = 0; host_info->h_addr_list[i] != NULL; i++) {
                struct in_addr addr;
                // Copia os 4 bytes do endereco binario IPv4
                memcpy(&addr, host_info->h_addr_list[i], sizeof(struct in_addr));
                // inet_ntoa: converte o endereco binario de rede para texto decimal com pontos
                printf("       -> %s (Rede Local)\n", inet_ntoa(addr));
            }
            // Sugestao de IP de loopback para execucao no mesmo dispositivo
            printf("       -> 127.0.0.1 (se Cliente e Servidor estiverem no mesmo PC)\n");
        }
    }
    printf("----------------------------------------------------\n");
}

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
 * Funcao: main (Ponto de Entrada do Servidor)
 * ----------------------------------------------------------------------------
 * Parametros opcionais de linha de comando:
 *   argv[1] -> Porta TCP customizada (ex: .\servidor_velha.exe 7000)
 * ============================================================================
 */
int main(int argc, char *argv[]) {
    WSADATA wsaData;                    // Estrutura que recebe dados da implementacao Winsock
    SOCKET listen_sd = INVALID_SOCKET;  // Socket passivo (escuta de novas conexoes)
    SOCKET client_sd = INVALID_SOCKET;  // Socket ativo conectado (comunicacao com o cliente)
    struct sockaddr_in servAddr;        // Estrutura com endereco e porta do Servidor
    struct sockaddr_in cliAddr;         // Estrutura com endereco e porta remotos do Cliente
    int cliLen;                         // Tamanho em bytes da estrutura de endereco do cliente
    char tabuleiro[9];                  // Vetor de estado do tabuleiro (indices 0 a 8)
    char buffer[BUFFER_SIZE];           // Buffer de comunicacao de rede
    int rc;                             // Codigo de retorno das funcoes Winsock

    // Configura o console do Windows para UTF-8 (permite acentuacao correta em pt-BR)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Permite configurar uma porta TCP alternativa via linha de comando
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

    // ------------------------------------------------------------------------
    // PASSO 1: INICIALIZACAO DA BIBLIOTECA WINSOCK (WS2_32.DLL)
    // ------------------------------------------------------------------------
    // No Windows, antes de qualquer operacao com sockets, o processo deve chamar
    // WSAStartup para carregar e negociar a versao da biblioteca (2.2 solicitada).
    printf("\n[1] Inicializando Winsock (WS2_32.dll)...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[ERRO FATAL] Falha ao iniciar Winsock. Codigo: %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    printf("    Winsock inicializado com sucesso.\n");

    // Exibe os enderecos IP locais da maquina para facilitar a conexao do Cliente
    exibir_ips_locais(porta);

    // ------------------------------------------------------------------------
    // PASSO 2: CRIACAO DO SOCKET DE ESCUTA TCP
    // ------------------------------------------------------------------------
    // Parametros:
    //   AF_INET     -> Familia de enderecos IPv4
    //   SOCK_STREAM -> Tipo orientado a conexao, fluxo confiavel e sequenciado (TCP)
    //   IPPROTO_TCP -> Protocolo de Transporte TCP
    listen_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sd == INVALID_SOCKET) {
        printf("[ERRO] Nao foi possivel criar socket. Codigo: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Configura a opcao SO_REUSEADDR para permitir o reuso imediato da porta local,
    // evitando erros de 'Address already in use' caso o programa seja reiniciado rapidamente.
    int opt = 1;
    setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    // ------------------------------------------------------------------------
    // PASSO 3: ASSOCIACAO DO SOCKET COM O ENDERECO E PORTA LOCAL (BIND)
    // ------------------------------------------------------------------------
    servAddr.sin_family = AF_INET;                 // Familia IPv4
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Escuta em todas as interfaces de rede locais
    servAddr.sin_port = htons((u_short)porta);      // Converte porta de Host byte order para Network byte order (Big-Endian)

    rc = bind(listen_sd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Falha no bind na porta %d. Codigo: %d\n", porta, WSAGetLastError());
        printf("       Dica: A porta pode estar em uso. Tente: .\\servidor_velha.exe 7000\n");
        closesocket(listen_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------------
    // PASSO 4: MODO DE ESCUTA PASSIVA (LISTEN)
    // ------------------------------------------------------------------------
    // Converte o socket em passivo e define o backlog (fila de conexoes pendentes = 1).
    rc = listen(listen_sd, 1);
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Falha no listen. Codigo: %d\n", WSAGetLastError());
        closesocket(listen_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("[2] Servidor aguardando conexao do Cliente na porta %d...\n", porta);

    // ------------------------------------------------------------------------
    // PASSO 5: ACEITACAO DA CONEXAO DO CLIENTE (ACCEPT)
    // ------------------------------------------------------------------------
    // accept() bloqueia a execucao ate que um cliente envie o pacote TCP SYN e
    // complete o handshake de 3 vias. Quando concluido, retorna um NOVO socket (client_sd)
    // dedicado exclusivamente para a transmissao de dados com este cliente.
    cliLen = sizeof(cliAddr);
    client_sd = accept(listen_sd, (struct sockaddr *)&cliAddr, &cliLen);
    if (client_sd == INVALID_SOCKET) {
        printf("[ERRO] Falha ao aceitar conexao. Codigo: %d\n", WSAGetLastError());
        closesocket(listen_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Exibe o IP e a porta efemera do Cliente conectado
    printf("    Conexao estabelecida com sucesso com %s:%d!\n\n",
           inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));

    // Como o jogo e estritamente para 2 participantes (1 cliente), nao precisamos
    // mais aceitar novas conexoes. Fechamos o socket de escuta para liberar o recurso.
    closesocket(listen_sd);
    listen_sd = INVALID_SOCKET;

    // ------------------------------------------------------------------------
    // PASSO 6: INICIALIZACAO DO TABULEIRO DE JOGO
    // ------------------------------------------------------------------------
    // Todas as 9 posicoes sao inicializadas com espaco em branco ' '
    for (int i = 0; i < 9; i++) {
        tabuleiro[i] = ' ';
    }

    // ------------------------------------------------------------------------
    // PASSO 7: SORTEIO ALEATORIO DA ORDEM DE JOGADA (QUEM INICIA)
    // ------------------------------------------------------------------------
    // Usa o relogio do sistema como semente para gerar valores pseudoaleatorios.
    // 1 = Servidor comeca jogando | 0 = Cliente comeca jogando.
    srand((unsigned int)time(NULL));
    int vez_do_servidor = rand() % 2;

    printf("====================================================\n");
    printf("   CONFIGURACAO DA PARTIDA\n");
    printf("   Servidor (Voce): Simbolo '%c'\n", SIMBOLO_SERVIDOR);
    printf("   Cliente:         Simbolo '%c'\n", SIMBOLO_CLIENTE);
    printf("====================================================\n");

    // Monta a mensagem de controle do protocolo de aplicacao
    memset(buffer, 0, sizeof(buffer));
    if (vez_do_servidor) {
        snprintf(buffer, sizeof(buffer), "START:SERVER\n");
        printf("Sorteio realizado: SERVIDOR (Voce) comeca a partida!\n");
    } else {
        snprintf(buffer, sizeof(buffer), "START:CLIENT\n");
        printf("Sorteio realizado: CLIENTE comeca a partida!\n");
    }

    // Envia a mensagem de controle ao Cliente via TCP
    rc = send(client_sd, buffer, (int)strlen(buffer), 0);
    if (rc == SOCKET_ERROR) {
        printf("[ERRO] Falha ao enviar mensagem de sorteio. Codigo: %d\n", WSAGetLastError());
        closesocket(client_sd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Exibe o tabuleiro de referencia inicial para orientar o jogador
    exibir_tabuleiro_guia();

    // ------------------------------------------------------------------------
    // PASSO 8: CICLO PRINCIPAL DE TURNOS DO JOGO DA VELHA
    // ------------------------------------------------------------------------
    int jogo_ativo = 1;
    while (jogo_ativo) {
        if (vez_do_servidor) {
            // ================================================================
            // TURNO ATIVO DO SERVIDOR:
            //   1. Solicita a jogada do usuario local.
            //   2. Atualiza o tabuleiro local.
            //   3. Desenha o tabuleiro na tela.
            //   4. Envia pacote "MOVE:<celula>\n" ao cliente via rede.
            //   5. Verifica se o servidor venceu ou deu empate.
            //   6. Se o jogo continuar, passa a vez ao cliente.
            // ================================================================
            int jogada = obter_jogada_usuario(tabuleiro, SIMBOLO_SERVIDOR);
            tabuleiro[jogada - 1] = SIMBOLO_SERVIDOR;

            // Exibe tabuleiro com a jogada aplicada
            exibir_tabuleiro(tabuleiro);

            // Monta o pacote de aplicacao: "MOVE:N\n"
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "MOVE:%d\n", jogada);

            // Transmissao do movimento pela rede
            rc = send(client_sd, buffer, (int)strlen(buffer), 0);
            if (rc == SOCKET_ERROR) {
                printf("\n[ERRO] Falha de conexao ao enviar jogada. Codigo: %d\n", WSAGetLastError());
                break;
            }

            // Inspeciona condicao de vitoria/empate no tabuleiro
            int status = verificar_termino(tabuleiro);
            if (status == 2) { // Servidor ('O') alinhou 3 pecas
                printf("*********************************\n");
                printf("          Você venceu!\n");
                printf("*********************************\n");
                jogo_ativo = 0;
            } else if (status == 0) { // Todas as casas ocupadas sem vencedor
                printf("*********************************\n");
                printf("             Empate!\n");
                printf("*********************************\n");
                jogo_ativo = 0;
            } else {
                // Passa a vez para o Cliente
                vez_do_servidor = 0;
            }

        } else {
            // ================================================================
            // TURNO PASSIVO DO SERVIDOR (Aguardando movimento do Cliente):
            //   1. Bloqueia na chamada recv() aguardando dados da rede.
            //   2. Trata desconexoes abruptas ou fechamento gracioso do cliente.
            //   3. Interpreta a mensagem "MOVE:<celula>\n".
            //   4. Atualiza o tabuleiro local com o simbolo do cliente ('X').
            //   5. Exibe o tabuleiro atualizado.
            //   6. Verifica se o cliente venceu ou deu empate.
            //   7. Se o jogo continuar, passa a vez ao servidor.
            // ================================================================
            printf("Aguardando jogada do adversario (Cliente)...\n");

            memset(buffer, 0, sizeof(buffer));
            // Chamada bloqueante ao recv: retorna numero de bytes lidos
            rc = recv(client_sd, buffer, sizeof(buffer) - 1, 0);

            if (rc > 0) {
                // Adiciona o caractere terminador de string nulo
                buffer[rc] = '\0';
                int jogada_cli = 0;

                // Faz o parsing seguro da mensagem de movimento recebida
                if (sscanf(buffer, "MOVE:%d", &jogada_cli) == 1 &&
                    jogada_cli >= 1 && jogada_cli <= 9 &&
                    tabuleiro[jogada_cli - 1] == ' ') {

                    // Aplica a jogada do cliente no tabuleiro local
                    tabuleiro[jogada_cli - 1] = SIMBOLO_CLIENTE;
                    printf("Adversario jogou na celula %d.\n", jogada_cli);
                    exibir_tabuleiro(tabuleiro);

                    // Inspeciona condicao de vitoria/empate no tabuleiro
                    int status = verificar_termino(tabuleiro);
                    if (status == 1) { // Cliente ('X') alinhou 3 pecas
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
                        // Passa a vez para o Servidor
                        vez_do_servidor = 1;
                    }
                } else {
                    printf("[ERRO] Mensagem de jogada invalida recebida do cliente: %s\n", buffer);
                    break;
                }
            } else if (rc == 0) {
                // rc == 0 indica fechamento gracioso do socket pelo cliente (FIN recebido)
                printf("\n[AVISO] Conexao encerrada pelo adversario (Cliente).\n");
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

    // Fecha o socket do cliente (disparando o handshake de encerramento TCP FIN-ACK)
    if (client_sd != INVALID_SOCKET) {
        closesocket(client_sd);
    }

    // Desregistra a aplicacao do Winsock e descarrega a dll ws2_32.dll
    WSACleanup();

    printf("Recursos liberados com sucesso.\n");
    printf("Pressione Enter para encerrar o programa.\n");
    getchar();

    return 0;
}

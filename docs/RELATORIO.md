# Relatório de Implementação: Jogo da Velha Cliente-Servidor (TCP)

**Disciplina:** Redes de Computadores para Engenharia Eletrônica I (2026.2)  
**Avaliação:** Trabalho para VE (Valor: 3,0 pontos)  
**Data:** 21 de outubro de 2026  

---

## 1. Introdução e Objetivos

O presente trabalho consiste na implementação de uma aplicação em rede para o **Jogo da Velha (Tic-Tac-Toe)** para dois jogadores (Cliente e Servidor), utilizando a **linguagem C** e a API de **Sockets TCP** (*Transmission Control Protocol*).

A aplicação foi desenvolvida para o ambiente operacional Windows, utilizando a biblioteca nativa **Winsock2** (`ws2_32.dll`), assegurando conformidade com os modelos desenvolvidos nas aulas da disciplina.

---

## 2. Requisitos e Decisões de Projeto

Em conformidade integral com a especificação fornecida:

1. **Conexão e Inicialização:**
   - O nó **Servidor** é iniciado primeiro, aloca o socket passivo TCP (`socket`), associa-o à porta local (`bind`) e aguarda ativamente (`listen`).
   - O Servidor detecta automaticamente e imprime no console todos os seus endereços IP locais, facilitando a visualização em apresentações no laboratório.
   - O nó **Cliente** pode receber o IP do servidor via linha de comando ou por entrada interativa no terminal (com valor padrão `127.0.0.1` ao teclar Enter).
   - Uma vez aceita a conexão (`accept` / `connect`), ambos os nós inicializam a estrutura do tabuleiro (vetor 3x3 preenchido com espaços vazios `' '`).

2. **Sorteio do Primeiro a Jogar (Ordem de Jogada):**
   - O Servidor utiliza uma semente pseudoaleatória baseada no relógio do sistema (`time(NULL)`) e realiza um sorteio binário (`rand() % 2`).
   - O Servidor joga com o símbolo **`'O'`** e o Cliente com o símbolo **`'X'`** (conforme ilustrado na folha de requisitos).
   - O Servidor transmite imediatamente uma mensagem de controle via socket:
     - `START:SERVER\n` se o Servidor foi sorteado.
     - `START:CLIENT\n` se o Cliente foi sorteado.

3. **Ciclo de Turnos e Transmissão de Dados:**
   - **No turno ativo:** O jogador visualiza o tabuleiro atual, insere sua jogada no prompt `Vez de [O/X] célula > `, o tabuleiro local é atualizado e o movimento é enviado pela rede através do pacote `MOVE:<celula>\n`.
   - **No turno passivo:** A aplicação aguarda no bloqueio de `recv()`. Ao receber os dados do adversário, atualiza a célula correspondente no tabuleiro local e libera o turno para o jogador local.
   - **Validação independente:** A cada atualização, tanto Cliente quanto Servidor verificam as 8 linhas de vitória (3 horizontais, 3 verticais e 2 diagonais) e a condição de empate (todas as 9 células preenchidas sem vencedor).

4. **Tratamento de Erros e Validações:**
   - **Validação de Entrada:** Tratamento rigoroso de caracteres inválidos (letras, símbolos, múltiplos dígitos, células já ocupadas e entradas vazias) com limpeza do buffer de entrada `stdin`.
   - **Tratamento de Rede:** Tratamento de erros de inicialização Winsock, criação de socket, bind, connect e desconexões abruptas do oponente (`recv() <= 0`), garantindo o fechamento correto dos recursos com `closesocket()` e `WSACleanup()`.
   - **Praticidade Operacional:** A aplicação aceita IP e Porta dinâmicos e nunca exige recompilação para mudar de computador ou rede.

---

## 3. Protocolo da Camada de Aplicação

| Mensagem | Direção | Descrição |
| :--- | :--- | :--- |
| `START:SERVER\n` | Servidor $\to$ Cliente | Notifica que o Servidor inicia jogando |
| `START:CLIENT\n` | Servidor $\to$ Cliente | Notifica que o Cliente inicia jogando |
| `MOVE:<N>\n` | Bidirecional | Informa a jogada realizada na célula $N \in [1, 9]$ |

---

## 4. Instruções de Compilação e Uso

### Compilação via MinGW GCC
Execute o arquivo `compilar.bat` ou os comandos manuais:
```bash
gcc -Wall -Wextra servidor_velha.c -o servidor_velha.exe -lws2_32
gcc -Wall -Wextra cliente_velha.c -o cliente_velha.exe -lws2_32
```

### Execução sem Recompilar (Flexível)

#### Cenário 1: Teste Local (No mesmo computador)
1. Terminal 1: `.\servidor_velha.exe`
2. Terminal 2: `.\cliente_velha.exe` *(aperte Enter quando pedir o IP para usar 127.0.0.1)*

#### Cenário 2: Entre Computadores Diferentes no Laboratório
1. **Computador 1 (Servidor):**
   Execute `.\servidor_velha.exe`. O programa listará na tela os IPs da máquina (exemplo: `172.16.7.88`).
2. **Computador 2 (Cliente):**
   Execute `.\cliente_velha.exe` e digite o IP exibido na tela do Servidor (ou rode direto: `.\cliente_velha.exe 172.16.7.88`).

---

## 5. Códigos-Fonte dos Módulos

Os arquivos completos encontram-se devidamente comentados nos arquivos:
- `servidor_velha.c`
- `cliente_velha.c`

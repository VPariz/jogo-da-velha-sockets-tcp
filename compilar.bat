@echo off
echo ===================================================
echo Compilando Jogo da Velha (Servidor e Cliente TCP)
echo ===================================================

gcc -Wall -Wextra servidor_velha.c -o servidor_velha.exe -lws2_32
if %ERRORLEVEL% EQU 0 (
    echo [OK] servidor_velha.exe compilado com sucesso!
) else (
    echo [ERRO] Falha ao compilar servidor_velha.c
    pause
    exit /b %ERRORLEVEL%
)

gcc -Wall -Wextra cliente_velha.c -o cliente_velha.exe -lws2_32
if %ERRORLEVEL% EQU 0 (
    echo [OK] cliente_velha.exe compilado com sucesso!
) else (
    echo [ERRO] Falha ao compilar cliente_velha.c
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo Compilacao finalizada com sucesso!
echo Para rodar:
echo 1. Em um terminal: .\servidor_velha.exe
echo 2. Em outro terminal: .\cliente_velha.exe
echo ===================================================
pause

@echo off
echo Iniciando Servidor e Cliente localmente para teste rapido...
cd /d "%~dp0"
start "SERVIDOR - Jogo da Velha (O)" cmd /k "servidor_velha.exe"
timeout /t 1 /nobreak >nul
start "CLIENTE - Jogo da Velha (X)" cmd /k "cliente_velha.exe 127.0.0.1"

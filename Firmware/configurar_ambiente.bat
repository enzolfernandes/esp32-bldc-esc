@echo off
title Configurar Ambiente PlatformIO, Clangd e Cursor

echo ===================================================
echo   1. Configurando PlatformIO no PATH do Windows
echo ===================================================
echo.

set "PIO_PATH=%USERPROFILE%\.platformio\penv\Scripts"
echo O seguinte diretorio sera verificado no PATH:
echo %PIO_PATH%

powershell -Command "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($userPath -notlike '*%PIO_PATH%*') { [Environment]::SetEnvironmentVariable('Path', $userPath + ';%PIO_PATH%', 'User'); Write-Host 'Sucesso: Caminho do PlatformIO adicionado!' -ForegroundColor Green } else { Write-Host 'Aviso: O PlatformIO ja esta no seu PATH.' -ForegroundColor Yellow }"

echo.
echo ===================================================
echo   1b. Configurando Clangd (LLVM) no PATH do Windows
echo ===================================================
echo.

set "CLANGD_PATH=C:\Program Files\LLVM\bin"
echo O seguinte diretorio sera verificado no PATH:
echo %CLANGD_PATH%

powershell -Command "$clangdPath = 'C:\Program Files\LLVM\bin'; $userPath = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($userPath -notlike '*LLVM*') { [Environment]::SetEnvironmentVariable('Path', $userPath + ';' + $clangdPath, 'User'); Write-Host 'Sucesso: Caminho do Clangd adicionado!' -ForegroundColor Green } else { Write-Host 'Aviso: O Clangd (LLVM) ja esta no seu PATH.' -ForegroundColor Yellow }"

echo.
echo ===================================================
echo   2. Configurando o Projeto (Clangd e Cursor)
echo ===================================================
echo.

:: Verifica se esta na raiz do projeto
if exist "platformio.ini" goto verify_vscode

echo [Erro] Arquivo platformio.ini nao encontrado.
echo Coloque e execute este script na raiz do projeto, junto ao platformio.ini.
goto fim

:verify_vscode
echo [Etapa 1/4] Preparando pasta oculta .vscode...
if not exist ".vscode" mkdir ".vscode"
echo - Pasta .vscode pronta.

echo.
echo [Etapa 2/4] Criando extra_script.py...
echo import os > ".vscode\extra_script.py"
echo Import("env") >> ".vscode\extra_script.py"
echo env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True) >> ".vscode\extra_script.py"
echo - .vscode/extra_script.py gerado.

:update_ini
echo.
echo [Etapa 3/4] Atualizando platformio.ini...
findstr /C:"extra_scripts = pre:.vscode/extra_script.py" platformio.ini >nul
if %errorlevel% equ 0 goto skip_ini

echo. >> platformio.ini
echo extra_scripts = pre:.vscode/extra_script.py >> platformio.ini
echo - Linha adicionada ao platformio.ini.
goto make_settings

:skip_ini
echo - O platformio.ini ja contem a chamada do script. Pulando.

:make_settings
echo.
echo [Etapa 4/4] Configurando settings.json...
(
echo {
echo     "clangd.path": "C:\\Program Files\\LLVM\\bin\\clangd.exe",
echo     "clangd.arguments": [
echo         "--log=verbose",
echo         "--pretty",
echo         "--background-index",
echo         "--compile-commands-dir=${workspaceFolder}/.vscode",
echo         "--header-insertion=iwyu",
echo         "--completion-style=detailed"
echo     ],
echo     "clangd.onConfigChanged": "restart",
echo     "platformio-ide.autoRebuildAutocompleteIndex": false
echo }
) > ".vscode\settings.json"
echo - Arquivo .vscode/settings.json atualizado.

echo.
echo [Finalizando] Gerando banco de dados do Clangd...
"%PIO_PATH%\pio.exe" run -t compiledb

if %errorlevel% neq 0 goto erro_pio

:: O Pulo do Gato: Move o arquivo gerado da raiz para a pasta .vscode
if exist "compile_commands.json" (
    move /y compile_commands.json .vscode\ >nul
    echo - compile_commands.json movido para a pasta .vscode com sucesso!
)

echo.
echo ===================================================
echo Sucesso: Raiz 100%% limpa e ambiente configurado!
echo ===================================================
goto fim

:erro_pio
echo.
echo [Erro] Falha ao gerar o compile_commands.json.

:fim
echo.
echo Processo concluido. Pressione qualquer tecla para sair...
pause >nul
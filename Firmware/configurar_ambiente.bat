@echo off
title Configurar Ambiente PlatformIO, Clangd e Cursor (Monorepo)

:: ===================================================
:: 0. Solicitacao de Privilegios de Administrador
:: ===================================================
net session >nul 2>&1
if %errorLevel% == 0 (
    goto :admin_ok
) else (
    echo Solicitando privilegios de administrador...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:admin_ok
:: Quando elevado a Admin, o Windows joga o terminal para System32.
:: O comando abaixo garante que o diretorio de trabalho volte a ser a pasta Firmware.
cd /d "%~dp0"

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
echo   2. Configurando Clangd (LLVM) no PATH do Windows
echo ===================================================
echo.

set "CLANGD_PATH=C:\Program Files\LLVM\bin"
echo O seguinte diretorio sera verificado no PATH:
echo %CLANGD_PATH%

powershell -Command "$clangdPath = 'C:\Program Files\LLVM\bin'; $userPath = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($userPath -notlike '*LLVM*') { [Environment]::SetEnvironmentVariable('Path', $userPath + ';' + $clangdPath, 'User'); Write-Host 'Sucesso: Caminho do Clangd adicionado!' -ForegroundColor Green } else { Write-Host 'Aviso: O Clangd (LLVM) ja esta no seu PATH.' -ForegroundColor Yellow }"

echo.
echo ===================================================
echo   3. Configurando o Workspace e Diretorios
echo ===================================================
echo.

:: Verifica se o script esta realmente dentro da pasta Firmware (junto ao platformio.ini)
if exist "platformio.ini" goto valid_dir

echo [Erro] Arquivo platformio.ini nao encontrado.
echo Certifique-se de executar este script de dentro da pasta Firmware.
pause
exit /b

:valid_dir
echo [Etapa 1/6] Criando arquivo de Workspace na raiz do repositorio...
(
echo {
echo     "folders": [
echo         {
echo             "path": "."
echo         },
echo         {
echo             "path": "Firmware"
echo         }
echo     ],
echo     "settings": {}
echo }
) > "..\esp32-bldc-esc.code-workspace"
echo - Arquivo esp32-bldc-esc.code-workspace gerado na raiz do repositorio.

echo.
echo [Etapa 2/6] Criando arquivo de mapeamento do Clangd dentro de Firmware...
(
echo CompileFlags:
echo   CompilationDatabase: .vscode
) > ".clangd"
echo - Arquivo .clangd gerado em Firmware/.

echo.
echo [Etapa 3/6] Preparando pasta oculta .vscode...
if not exist ".vscode" mkdir ".vscode"
echo - Pasta .vscode pronta.

echo.
echo [Etapa 4/6] Criando extra_script.py...
(
echo import atexit
echo import os
echo import shutil
echo Import^("env"^)
echo.
echo env.Replace^(COMPILATIONDB_INCLUDE_TOOLCHAIN=True^)
echo.
echo _project_dir = env.subst^("$PROJECT_DIR"^)
echo.
echo def cleanup^(^):
echo     src = os.path.join^(_project_dir, "compile_commands.json"^)
echo     dst = os.path.join^(_project_dir, ".vscode", "compile_commands.json"^)
echo     if os.path.exists^(src^):
echo         shutil.move^(src, dst^)
echo.
echo     gitignore = os.path.join^(_project_dir, ".gitignore"^)
echo     if os.path.exists^(gitignore^):
echo         os.remove^(gitignore^)
echo.
echo atexit.register^(cleanup^)
) > ".vscode\extra_script.py"
echo - .vscode/extra_script.py gerado.

:update_ini
echo.
echo [Etapa 5/6] Verificando platformio.ini...
findstr /C:"extra_scripts" platformio.ini >nul
if %errorlevel% equ 0 (
    echo - extra_scripts ja configurado no platformio.ini. OK.
    goto make_settings
)

echo AVISO: extra_scripts nao encontrado no platformio.ini.
echo Adicionando entrada...
echo. >> platformio.ini
echo extra_scripts = pre:.vscode/extra_script.py >> platformio.ini
echo - Linha adicionada ao platformio.ini.

:make_settings
echo.
echo [Etapa 6/6] Configurando settings.json do Firmware...
(
echo {
echo     "clangd.path": "C:\\Program Files\\LLVM\\bin\\clangd.exe",
echo     "clangd.arguments": [
echo         "--log=verbose",
echo         "--pretty",
echo         "--background-index",
echo         "--compile-commands-dir=${workspaceFolder:Firmware}/.vscode",
echo         "--header-insertion=iwyu",
echo         "--completion-style=detailed"
echo     ],
echo     "clangd.onConfigChanged": "restart",
echo     "platformio-ide.autoRebuildAutocompleteIndex": false
echo }
) > ".vscode\settings.json"
echo - Arquivo .vscode/settings.json atualizado.

echo.
echo ===================================================
echo [Finalizando] Gerando banco de dados do Clangd...
echo ===================================================
echo ATENCAO: Na primeira execucao, o PlatformIO ira baixar o
echo framework BluePad32 (varios MB). Isso pode demorar alguns minutos.
echo Aguarde a conclusao sem fechar esta janela.
echo.
"%PIO_PATH%\pio.exe" run -t compiledb

if %errorlevel% neq 0 goto erro_pio

:: Move o arquivo gerado para o lugar correto (.vscode)
if exist "compile_commands.json" (
    move /y compile_commands.json .vscode\ >nul
    echo - compile_commands.json movido para .vscode com sucesso!
)

echo.
echo ===================================================
echo Sucesso: Multi-root configurado!
echo Abra o arquivo "esp32-bldc-esc.code-workspace" na raiz do repositorio no VS Code/Cursor.
echo ===================================================
goto fim

:erro_pio
echo.
echo [Erro] Falha ao gerar o compile_commands.json.

:fim
echo.
echo Processo concluido. Pressione qualquer tecla para fechar...
pause >nul

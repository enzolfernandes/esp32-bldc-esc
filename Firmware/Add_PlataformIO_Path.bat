@echo off
title Configurar PlatformIO PATH
echo ===================================================
echo   Adicionando PlatformIO ao PATH do Windows
echo ===================================================
echo.

:: Define o caminho exato do PlatformIO baseado no perfil do usuario atual
set "PIO_PATH=%USERPROFILE%\.platformio\penv\Scripts"

echo O seguinte diretorio sera adicionado ao seu PATH de usuario:
echo %PIO_PATH%
echo.

:: Usa PowerShell para ler o PATH atual do usuario, verificar duplicidade e adicionar com seguranca
powershell -Command "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($userPath -notlike '*%PIO_PATH%*') { [Environment]::SetEnvironmentVariable('Path', $userPath + ';%PIO_PATH%', 'User'); Write-Host 'Sucesso: Caminho do PlatformIO adicionado!' -ForegroundColor Green } else { Write-Host 'Aviso: O PlatformIO ja esta no seu PATH. Nenhuma mudanca foi feita.' -ForegroundColor Yellow }"

echo.
echo Processo concluido. Pressione qualquer tecla para sair...
pause >nul